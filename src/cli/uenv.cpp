// vim: ts=4 sts=4 sw=4 et

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <uenv/log.h>

#include <uenv/config.h>
#include <uenv/parse.h>
#include <uenv/repository.h>
#include <util/expected.h>

#include "add_remove.h"
#include "color.h"
#include "help.h"
#include "image.h"
#include "repo.h"
#include "run.h"
#include "start.h"
#include "uenv.h"

std::string help_footer();

int main(int argc, char** argv) {
    uenv::global_settings settings;
    bool print_version = false;

    // enable/disable color depending on NOCOLOR env. var
    // and tty terminal status.
    color::default_color();

    CLI::App cli(fmt::format("uenv {}", UENV_VERSION));
    cli.add_flag("-v,--verbose", settings.verbose, "enable verbose output");
    cli.add_flag_callback(
        "--no-color", []() -> void { color::set_color(false); },
        "disable color output");
    cli.add_flag_callback(
        "--color", []() -> void { color::set_color(true); },
        "enable color output");
    cli.add_flag("--repo", settings.repo_, "the uenv repository");
    cli.add_flag("--version", print_version, "print version");

    cli.footer(help_footer);

    uenv::start_args start;
    uenv::run_args run;
    uenv::image_args image;
    uenv::repo_args repo;

    start.add_cli(cli, settings);
    run.add_cli(cli, settings);
    image.add_cli(cli, settings);
    repo.add_cli(cli, settings);

    CLI11_PARSE(cli, argc, argv);

    // color::set_color(!settings.no_color);

    // Warnings and errors are always logged. The verbosity level is increased
    // with repeated uses of --verbose.
    spdlog::level::level_enum console_log_level = spdlog::level::warn;
    if (settings.verbose == 1) {
        console_log_level = spdlog::level::info;
    } else if (settings.verbose == 2) {
        console_log_level = spdlog::level::debug;
    } else if (settings.verbose >= 3) {
        console_log_level = spdlog::level::trace;
    }
    uenv::init_log(console_log_level, spdlog::level::trace);

    // print the version and exit if the --version flag was passed
    if (print_version) {
        fmt::print("{}\n", UENV_VERSION);
        return 0;
    }

    // if a repo was not provided as a flag, look at environment variables
    if (!settings.repo_) {
        if (const auto p = uenv::default_repo_path()) {
            settings.repo_ = *p;
        } else {
            spdlog::warn("ignoring the default repo path: {}", p.error());
        }
    }

    // post-process settings after the CLI arguments have been parsed
    if (settings.repo_) {
        if (const auto rpath =
                uenv::validate_repo_path(*settings.repo_, false, false)) {
            settings.repo = *rpath;
        } else {
            spdlog::warn("ignoring repo path due to an error, {}",
                         rpath.error());
            settings.repo = std::nullopt;
            settings.repo_ = std::nullopt;
        }
    }

    if (settings.repo) {
        spdlog::info("the repo {}", *settings.repo);
    }

    spdlog::info("{}", settings);

    switch (settings.mode) {
    case settings.start:
        return uenv::start(start, settings);
    case settings.run:
        return uenv::run(run, settings);
    case settings.image_ls:
        return uenv::image_ls(image.ls_args, settings);
    case settings.image_add:
        return uenv::image_add(image.add_args, settings);
    case settings.image_inspect:
        return uenv::image_inspect(image.inspect_args, settings);
    case settings.image_remove:
        return uenv::image_remove(image.remove_args, settings);
    case settings.repo_create:
        return uenv::repo_create(repo.create_args, settings);
    case settings.repo_status:
        return uenv::repo_status(repo.status_args, settings);
    case settings.unset:
        fmt::println("uenv version {}", UENV_VERSION);
        fmt::println("call '{} --help' for help", argv[0]);
        return 0;
    default:
        spdlog::warn("{}", (int)settings.mode);
        spdlog::error("internal error, missing implementation for mode {}",
                      settings.mode);
        return 1;
    }

    return 0;
}

std::string help_footer() {
    using enum help::block::admonition;
    using help::lst;

    // clang-format off
    std::vector<help::item> items{
        help::block{none, "Use the --help flag in with sub-commands for more information."},
        help::linebreak{},
        help::block{xmpl, fmt::format("use the {} flag to generate more verbose output", lst{"-v"})},
        help::block{code,   "uenv -v  image ls    # info level logging"},
        help::block{code,   "uenv -vv image ls    # debug level logging"},
        help::linebreak{},
        help::block{xmpl, "get help with the run command"},
        help::block{code,   "uenv run --help"},
        help::linebreak{},
        help::block{xmpl, fmt::format("get help with the {} command", lst("image ls"))},
        help::block{code,   "uenv image ls --help"},
    };
    // clang-format on

    return fmt::format("{}", fmt::join(items, "\n"));
}
