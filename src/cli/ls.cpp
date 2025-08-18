// vim: ts=4 sts=4 sw=4 et

#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include <site/site.h>
#include <uenv/parse.h>
#include <uenv/print.h>
#include <uenv/repository.h>
#include <util/expected.h>

#include "help.h"
#include "ls.h"
#include "terminal.h"

namespace uenv {

std::string image_ls_footer();

void image_ls_args::add_cli(CLI::App& cli,
                            [[maybe_unused]] global_settings& settings) {
    auto* ls_cli =
        cli.add_subcommand("ls", "search for uenv that are available to run");
    ls_cli->add_option("uenv", uenv_description, "search term");
    ls_cli->add_flag("--no-header", no_header,
                     "print only the matching records, with no header.");
    ls_cli->add_flag("--json", json, "format output as JSON.");
    ls_cli->callback(
        [&settings]() { settings.mode = uenv::cli_mode::image_ls; });

    ls_cli->footer(image_ls_footer);
}

int image_ls(const image_ls_args& args, const global_settings& settings) {
    // require that a valid repo has been provided
    if (!settings.config.repo) {
        term::error("a repo needs to be provided either using the --repo flag "
                    "in the config file");
        return 1;
    }

    //parse configuration.toml
    toml::table config;
    char *config_location = getenv("UENV_CONFIGURATION_PATH"); //gets config file from UENV_CONFIGURATION_PATH env variable
    std::string_view config_sv{config_location};
    try{
        config = toml::parse_file(config_sv);
    } catch(const toml::parse_error& err){
        term::error("{}", fmt::format("Error parsing configuration.toml:\n{}",err));
    }

    toml::array& config_repo = *config["uenv_local_repos"].as_array();

    // open the repo
    auto store = uenv::open_repository(settings.config.repo.value());
    if (!store) {
        term::error("unable to open repo: {}", store.error());
        return 1;
    }

    // find the search term that was provided by the user
    uenv_label label{};
    if (args.uenv_description) {
        if (const auto parse = parse_uenv_label(*args.uenv_description)) {
            label = *parse;
        } else {
            term::error("invalid search term: {}", parse.error().message());
            return 1;
        }
    }

    // set label->system to the current cluster name if it has not
    // already been set.
    label.system =
        site::get_system_name(label.system, settings.calling_environment);

    // query the repo
    const auto result = store->query(label);
    if (!result) {
        term::error("invalid search term: {}", store.error());
        return 1;
    }
    printf("User Repository: %s\n", settings.config.repo.value().c_str());
    print_record_set(*result, args.no_header, args.json);

    // query the local repos
    for (auto& repo : config_repo){
        if (repo.is_string()){//if repo is string, query
            std::string repo_path_string = repo.value_or("");
            
            //validate local repo and skip if validation fails
            if (auto rpath = uenv::validate_repo_path(repo_path_string, false, false)) {
                //open local repo if valid
                std::filesystem::path repo_path = repo_path_string;
                auto local_store = uenv::open_repository(repo_path);

                if (!local_store) {
                    term::error("unable to open local repo: {}", local_store.error());
                    return 1;
                }

                const auto local_result = local_store->query(label);
                if (!local_result) {
                    term::error("invalid search term: {}", local_store.error());
                    return 1;
                }
                printf("\nLocal Repository: %s\n", repo.value_or(""));
                print_record_set(*local_result, args.no_header, args.json); //print local repo listing

            } else {
                spdlog::warn("invalid repo path {}", rpath.error());
                continue;
            }
        }
    }

    return 0;
}

std::string image_ls_footer() {
    using enum help::block::admonition;
    std::vector<help::item> items{
        // clang-format off
        help::block{none, "Search for uenv that are available to run." },
        help::linebreak{},
        help::block{xmpl, "list all uenv"},
        help::block{code,   "uenv image ls"},
        help::linebreak{},
        help::block{xmpl, "list all uenv with the name prgenv-gnu"},
        help::block{code,   "uenv image ls prgenv-gnu"},
        help::linebreak{},
        help::block{xmpl, "list all uenv with the name prgenv-gnu and version 24.7"},
        help::block{code,   "uenv image ls prgenv-gnu/24.7"},
        help::linebreak{},
        help::block{xmpl, "list all uenv with the name prgenv-gnu, version 24.7 and release v2"},
        help::block{code, "uenv image ls prgenv-gnu/24.7:v2"},
        help::linebreak{},
        help::block{xmpl, "use the @ symbol to specify a target system name"},
        help::block{code,   "uenv image ls prgenv-gnu@todi"},
        help::block{none, "this feature is useful when using images that were built for a different system",
                          "than the one you are currently working on."},
        help::linebreak{},
        help::block{xmpl, "use the @ symbol to specify a target system name"},
        help::block{code,   "uenv image ls prgenv-gnu@todi"},
        help::block{none, "this feature is useful when using images that were built for a different system",
                          "than the one you are currently working on."},
        help::linebreak{},
        help::block{xmpl, "use the % symbol to specify a target microarchitecture (uarch)"},
        help::block{code,   "uenv image ls prgenv-gnu%gh200"},
        help::block{none, "this feature is useful on a system with multiple uarch."},
        help::linebreak{},
        help::block{xmpl, "list any uenv with a concrete sha256 checksum"},
        help::block{code,   "uenv image ls 510094ddb3484e305cb8118e21cbb9c94e9aff2004f0d6499763f42bdafccfb5"},
        help::linebreak{},
        help::block{note, "more than one uenv might be listed if there are two uenv that refer",
                          "to the same underlying uenv sha256."},
        help::linebreak{},
        help::block{xmpl, "search for uenv by id (id is the first 16 characters of the sha256):"},
        help::block{code,   "uenv image ls 510094ddb3484e30"},
        help::linebreak{},
        help::block{xmpl, "list all uenv on any target system"},
        help::block{code,   "uenv image ls @*"},

        // clang-format on
    };

    return fmt::format("{}", fmt::join(items, "\n"));
}

} // namespace uenv
