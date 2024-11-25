// vim: ts=4 sts=4 sw=4 et

#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <uenv/env.h>
#include <uenv/meta.h>
#include <uenv/parse.h>
#include <util/expected.h>
#include <util/shell.h>

#include "help.h"
#include "run.h"
#include "terminal.h"
#include "uenv.h"

namespace uenv {

std::string run_footer();

void run_args::add_cli(CLI::App& cli, global_settings& settings) {
    auto* run_cli = cli.add_subcommand("run", "run a uenv session");
    run_cli->add_option("-v,--view", view_description,
                        "comma separated list of views to load");
    run_cli
        ->add_option("uenv", uenv_description,
                     "comma separated list of uenv to mount")
        ->required();
    run_cli
        ->add_option("commands", commands,
                     "the command to run, including with arguments")
        ->required();
    run_cli->callback([&settings]() { settings.mode = uenv::cli_mode::run; });
    run_cli->footer(run_footer);
}

int run(const run_args& args, const global_settings& globals) {
    spdlog::info("run with options {}", args);
    const auto env = concretise_env(args.uenv_description,
                                    args.view_description, globals.repo);

    if (!env) {
        term::error("{}", env.error());
        return 1;
    }

    // generate the environment variables to set
    auto env_vars = uenv::getenv(*env);

    if (auto rval = uenv::setenv(env_vars, "SQFSMNT_FWD_"); !rval) {
        term::error("setting environment variables {}", rval.error());
        return 1;
    }

    // generate the mount list
    std::vector<std::string> commands = {"squashfs-mount"};
    for (auto e : env->uenvs) {
        commands.push_back(fmt::format("{}:{}", e.second.sqfs_path.string(),
                                       e.second.mount_path));
    }

    commands.push_back("--");
    commands.insert(commands.end(), args.commands.begin(), args.commands.end());

    return util::exec(commands);
}

std::string run_footer() {
    using enum help::block::admonition;
    using help::block;
    using help::linebreak;
    using help::lst;
    std::vector<help::item> items{
        // clang-format off
        block{none, "Run a command in an environment."},
        linebreak{},
        block{xmpl, "run the script job.sh in an evironmnent"},
        block{code,   "uenv run prgenv-gnu/24.2:v1 -- ./job.sh"},
        block{none, "This will mount prgenv-gnu, execute job.sh, then return to the calling shell."},
        block{note, "how the command to execute comes after the two dashes '--'."},
        linebreak{},
        block{xmpl, "run the script job.sh in an evironmnent with a view loaded"},
        block{code,   "uenv run prgenv-gnu/24.2:v1 --view=default -- ./job.sh"},
        linebreak{},
        block{note, "the spec must uniquely identify the uenv. To ensure this, always use a"},
        block{none, "fully qualified spec in the form of name/version:tag, the unique 16 digit id,"},
        block{none, "or sha256 of a uenv. If more than one uenv match the spec, an error message"},
        block{none, "is printed."},
        linebreak{},
        block{xmpl, "run the job.sh script with two images mounted"},
        block{code,   "uenv run prgenv-gnu/24.2:v1 ddt/23.1 -- ./job.sh"},
        linebreak{},
        block{xmpl, "run the job.sh script with two images mounted at specific mount points"},
        block{code,   "uenv run prgenv-gnu/24.2:v1:$SCRATCH/pe ddt/23.1:/user-tools -- ./job.sh"},
        block{none, "Here the mount point for each image is specified using a ':'."},
        linebreak{},
        block{note, "uenv must be mounted at the mount point for which they were built."},
        block{none, "If mounted at the wrong location, a warning message will be printed, and"},
        block{none, "views will be disabled."},
        linebreak{},
        block{xmpl, "the run command can be used to execute workflow steps with", "separate environments"},
        block{code,   "uenv run gromacs/23.1  -- ./simulation.sh"},
        block{code,   "uenv run paraview/5.11 -- ./render.sh"},
        // clang-format on
    };

    return fmt::format("{}", fmt::join(items, "\n"));
}

} // namespace uenv
