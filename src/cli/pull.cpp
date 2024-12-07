// vim: ts=4 sts=4 sw=4 et

#include <csignal>
#include <filesystem>
#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <site/site.h>
#include <uenv/oras.h>
#include <uenv/parse.h>
#include <uenv/print.h>
#include <uenv/repository.h>
#include <util/curl.h>
#include <util/expected.h>
#include <util/fs.h>
#include <util/signal.h>

#include "help.h"
#include "pull.h"
#include "terminal.h"

namespace uenv {

std::string image_pull_footer();

void image_pull_args::add_cli(CLI::App& cli,
                              [[maybe_unused]] global_settings& settings) {
    auto* pull_cli =
        cli.add_subcommand("pull", "download a uenv from a registry");
    pull_cli
        ->add_option("uenv", uenv_description,
                     "the uenv to pull, either name/version:tag, sha256 or id")
        ->required();
    pull_cli->add_flag("--only-meta", only_meta, "only download meta data");
    pull_cli->add_flag("--force", force,
                       "download and overwrite existing images");
    pull_cli->callback(
        [&settings]() { settings.mode = uenv::cli_mode::image_pull; });

    pull_cli->footer(image_pull_footer);
}

int image_pull([[maybe_unused]] const image_pull_args& args,
               [[maybe_unused]] const global_settings& settings) {

    namespace fs = std::filesystem;

    spdlog::info("image pull: {}", args);

    // pull the search term that was provided by the user
    uenv_label label{};
    std::string nspace{site::default_namespace()};
    if (const auto parse = parse_uenv_nslabel(args.uenv_description)) {
        label = parse->label;
        if (parse->nspace) {
            nspace = *parse->nspace;
        }
    } else {
        term::error("invalid search term: {}", parse.error().message());
        return 1;
    }

    label.system = site::get_system_name(label.system);
    if (!label.name) {
        term::error(
            "the uenv description '{}' must specify the name of the uenv",
            args.uenv_description);
        return 1;
    }

    spdlog::info("image_pull: {}::{}", nspace, label);

    auto registry = site::registry_listing(nspace);
    if (!registry) {
        term::error("unable to get a listing of the uenv", registry.error());
        return 1;
    }

    // search db for matching records
    const auto remote_matches = registry->query(label);
    if (!remote_matches) {
        term::error("invalid search term: {}", registry.error());
        return 1;
    }

    // check that there is one record with a unique sha
    if (remote_matches->empty()) {
        using enum help::block::admonition;
        term::error("no uenv found that matches '{}'\n\n{}",
                    args.uenv_description,
                    help::block(info, "try searching for the uenv to pull "
                                      "first using 'uenv image find'"));
        return 1;
    } else if (!remote_matches->unique_sha()) {
        std::string errmsg =
            fmt::format("more than one uenv found that matches '{}':\n",
                        args.uenv_description);
        errmsg += format_record_set(*remote_matches);
        term::error("{}", errmsg);
        return 1;
    }

    // pick a record to use for pulling
    const auto record = *(remote_matches->begin());
    spdlog::info("pulling {} {}", record.sha, record);

    // open the repo
    auto store = uenv::open_repository(*settings.repo, repo_mode::readwrite);
    if (!store) {
        term::error("unable to open repo: {}", store.error());
        return 1;
    }

    auto paths = store->uenv_paths(record.sha);

    // acquire a file lock so that only one process can try to pull an image.
    // TODO: how do we handle the case where we have many processes waiting, and
    // there is a failure (e.g. file system problem), that causes the processes
    // to attempt the pull one-after-the-other
    auto lock = util::make_file_lock(paths.store.string() + ".lock");

    bool meta_exists = fs::exists(paths.meta);
    bool sqfs_exists = fs::exists(paths.squashfs);

    auto in_repo = [&store](uenv_label label) -> bool {
        return !(store->query(label)->empty());
    };
    const bool sha_in_repo = in_repo({.name = record.sha.string()});
    const bool label_in_repo = in_repo({.name = record.name,
                                        .version = record.version,
                                        .tag = record.tag,
                                        .system = record.system,
                                        .uarch = record.uarch});

    spdlog::debug("sha   in repo: {}", sha_in_repo);
    spdlog::debug("label in repo: {}", label_in_repo);

    if (args.force || !sha_in_repo) {
        try {
            bool pull_sqfs = !args.only_meta && (args.force || !sqfs_exists);
            bool pull_meta = args.force || !meta_exists;

            spdlog::debug("pull meta: {}", pull_meta);
            spdlog::debug("pull sqfs: {}", pull_sqfs);

            auto rego_url = site::registry_url();
            spdlog::debug("registry url: {}", rego_url);

            auto digests = oras::discover(rego_url, nspace, record);
            if (!digests || digests->empty()) {
                term::error(
                    "unable to pull image - rerun with -vvv flag and send "
                    "error report to service desk.\n");
                return 1;
            }
            spdlog::debug("manifests: {}", fmt::join(*digests, ", "));

            const auto digest = *(digests->begin());

            if (auto okay = oras::pull_digest(rego_url, nspace, record, digest,
                                              paths.store);
                !okay) {
                term::error(
                    "unable to pull image - rerun with -vvv flag and send "
                    "error report to service desk.\n");
                return 1;
            }

            auto tag_result =
                oras::pull_tag(rego_url, nspace, record, paths.store);
            if (!tag_result) {
                return 1;
            }
        } catch (util::signal_exception& e) {
            spdlog::debug("removing record {}", record);
            store->remove(record.sha);
            spdlog::debug("deleting path {}", paths.store);
            std::filesystem::remove_all(paths.store);
            raise(e.signal);
        }
    } else {
        term::msg("no uenv to pull: the sha\n  {}\nis already in the local "
                  "repository.",
                  color::yellow(record.sha.string()));
    }

    // add the label to the repo, even if there was no download.
    // download may have been skipped if a squashfs with the same sha has
    // been downloaded, and this download uses a different label.
    for (auto& r : *remote_matches) {
        bool exists = in_repo({.name = r.name,
                               .version = r.version,
                               .tag = r.tag,
                               .system = r.system,
                               .uarch = r.uarch});
        if (!exists) {
            term::msg("updating {}", r);
            store->add(r);
        }
    }

    return 0;
}

std::string image_pull_footer() {
    using enum help::block::admonition;
    std::vector<help::item> items{
        // clang-format off
        help::block{none, "Download a uenv from a registry." },
        help::linebreak{},
        // clang-format on
    };

    return fmt::format("{}", fmt::join(items, "\n"));
}

} // namespace uenv
