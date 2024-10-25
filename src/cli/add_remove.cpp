// vim: ts=4 sts=4 sw=4 et

#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <uenv/cscs.h>
#include <uenv/parse.h>
#include <uenv/repository.h>
#include <util/expected.h>
#include <util/fs.h>
#include <util/subprocess.h>

#include "add_remove.h"
#include "help.h"

namespace uenv {

std::string image_add_footer();
std::string image_remove_footer();

void image_add_args::add_cli(CLI::App& cli,
                             [[maybe_unused]] global_settings& settings) {
    auto* add_cli = cli.add_subcommand("add", "manage and query uenv images");
    add_cli
        ->add_option("label", uenv_description,
                     "the label, of the form name:version:tag@system#uarch")
        ->required();
    add_cli->add_option("squashfs", squashfs, "the squashfs file to add.")
        ->required();
    add_cli->callback(
        [&settings]() { settings.mode = uenv::cli_mode::image_add; });

    add_cli->footer(image_add_footer);
}

void image_remove_args::add_cli(CLI::App& cli,
                                [[maybe_unused]] global_settings& settings) {
    auto* remove_cli =
        cli.add_subcommand("remove", "manage and query uenv images");
    remove_cli->add_option("uenv", uenv_description, "the uenv to remove.");
    remove_cli->callback(
        [&settings]() { settings.mode = uenv::cli_mode::image_remove; });

    remove_cli->footer(image_remove_footer);
}

int image_add(const image_add_args& args, const global_settings& settings) {
    namespace fs = std::filesystem;

    //
    // parse the cli args
    //
    // - the squashfs file
    // - the label
    auto label = uenv::parse_uenv_label(args.uenv_description);
    if (!label) {
        spdlog::error("the label {} is not valid: {}", args.uenv_description,
                      label.error().description);
        return 1;
    }
    if (!(label->name && label->version && label->tag)) {
        spdlog::error("the label {} must provide at a minimum name/version:tag",
                      args.uenv_description);
        return 1;
    }
    spdlog::info("image_add: label {}", *label);

    auto file = uenv::parse_path(args.squashfs);
    if (!file) {
        spdlog::error("invalid squashfs file {}: {}", args.squashfs,
                      file.error().description);
        return 1;
    }
    fs::path sqfs = *file;
    if (!fs::is_regular_file(sqfs)) {
        spdlog::error("invalid squashfs file {} is not a file", args.squashfs);
        return 1;
    }
    spdlog::info("image_add: squashfs {}", fs::absolute(sqfs));

    //
    // get the sha256 of the file
    //
    std::string hash;
    {
        auto proc = util::run({"sha256sum", sqfs.string()});
        if (!proc) {
            spdlog::info("{}", proc.error());
            spdlog::error("unable to calculate sha256 of squashfs file {}",
                          args.squashfs);
            return 1;
        }
        auto success = proc->wait();
        if (success != 0) {
            spdlog::error("unable to calculate sha256 of squashfs file {}",
                          args.squashfs);
            return 1;
        }

        auto raw = *proc->out.getline();

        hash = raw.substr(0, 64);
    }
    spdlog::debug("image_add: squashfs hash {}", hash);

    //
    // Open the repository
    //
    if (!settings.repo) {
        spdlog::error(
            "a repo needs to be provided either using the --repo flag or by "
            "setting the UENV_REPO_PATH environment variable");
        return 1;
    }
    auto store = uenv::open_repository(settings.repo.value());
    if (!store) {
        spdlog::error("unable to open repo: {}", store.error());
        return 1;
    }

    /*
    if (!store->is_readwrite()) {
        spdlog::error("the repository {} is read only",
                      settings.repo.value().string());
        return 1;
    }
    */

    //
    // check whether the label also exists
    //
    bool existing_label = false;
    {
        auto results = store->query(*label);
        // TODO check error on results
        existing_label = !results->empty();
        for (auto& r : *results) {
            fmt::println(":: {}", r);
        }

        if (existing_label) {
            spdlog::error(
                "image_add: a uenv image already exists with the label {}",
                *label);
            return 1;
        }
    }

    //
    // check whether repository already contains an image with the same
    // hash
    //
    bool existing_hash = false;
    {
        uenv_label hash_label{hash};
        auto results = store->query(hash_label);
        // TODO check error on results
        existing_hash = !results->empty();
        for (auto& r : *results) {
            fmt::println(":: {}", r);
        }

        if (existing_hash) {
            spdlog::warn(
                "image_add: a uenv with the same sha is already in the repo");
        }
    }

    //
    // create the path inside the repo
    //
    fmt::println("repo {}", *(settings.repo));

    std::error_code ec;
    auto img_path = settings.repo.value() / "images" / hash;
    // if the path exists, delete it, as it might contain a partial download
    if (fs::exists(img_path)) {
        spdlog::debug("image_add: remove the target path {} before copying",
                      img_path.string());
        fs::remove_all(img_path);
    }

    fs::create_directories(img_path, ec);
    if (ec) {
        spdlog::error("unable to create path {}: {}", img_path.string(),
                      ec.message());
        return 1;
    }

    fs::copy_file(sqfs, img_path / "store.squashfs", ec);
    if (ec) {
        spdlog::error("unable to copy squashfs image {} to {}: {}",
                      sqfs.string(), img_path.string(), ec.message());
        return 1;
    }

    //
    // copy the meta data into the repo
    //
    if (auto p = util::unsquashfs_tmp(sqfs, "meta")) {
        // TODO: make this recursive, check error codes
        fs::copy_options options;
        options |= fs::copy_options::recursive;
        options |= fs::copy_options::update_existing;
        fs::copy(p.value() / "meta", img_path / "meta", options, ec);
        if (ec) {
            spdlog::error("unable to copy meta data to {}: {}",
                          (img_path / "meta").string(), ec.message());
            return 1;
        }
    }

    //
    // add the uenv to the database
    //

    // this interface lets us fully control
    // store.add_uenv(hash, label);

    return 0;
}

int image_remove(const image_remove_args& args,
                 const global_settings& settings) {
    return 0;
}

std::string image_add_footer() {
    using enum help::block::admonition;
    std::vector<help::item> items{
        // clang-format off
        help::block{none, "Add a uenv image to a repository." },
        help::linebreak{},
        help::block{xmpl, "add an image, providing the full label"},
        help::block{code,   "uenv image add myenv/24.7:v1 ./store.squashfs"},
        help::block{code,   "uenv image add myenv/24.7:v1@todi#gh200 ./store.squashfs"},
        /*
        help::linebreak{},
        help::block{xmpl, "infer the name from the meta data"},
        help::block{code,   "uenv image add --version=24.7 --tag=v1 ./store.squashfs"},
        help::linebreak{},
        help::block{xmpl, "infer the name from the meta data"},
        help::block{code,   "uenv image add --version=24.7 --tag=v1 --uarch=a100 ./store.squashfs"},
        help::linebreak{},
        help::block{xmpl, "add cluster name"},
        help::block{code,   "uenv image add --version=24.7 --tag=v1 --uarch=a100 --system=daint ./store.squashfs"},
        */
        // clang-format on
    };

    return fmt::format("{}", fmt::join(items, "\n"));
}

std::string image_remove_footer() {
    using enum help::block::admonition;
    std::vector<help::item> items{
        // clang-format off
        help::block{none, "Remove a uenv image from a repository." },
        help::linebreak{},
        help::block{xmpl, "by label"},
        help::block{code,   "uenv image remove prgenv-gnu/24.7:v1"},
        help::linebreak{},
        help::block{xmpl, "by label"},
        help::block{code,   "uenv image remove prgenv-gnu"},
        help::block{code,   "uenv image remove prgenv-gnu/24.7"},
        help::block{none, "should this delete all images that match?"},
        help::linebreak{},
        help::block{xmpl, "by sha"},
        help::block{code,   "uenv image remove abcd1234abcd1234abcd1234abcd1234"},
        help::linebreak{},
        help::block{xmpl, "by id"},
        help::block{code,   "uenv image remove abcd1234"},
        help::linebreak{},
        // clang-format on
    };

    return fmt::format("{}", fmt::join(items, "\n"));
}

} // namespace uenv
