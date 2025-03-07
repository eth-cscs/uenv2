#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <util/environment.h>
#include <util/expected.h>

#include <uenv/uenv.h>
#include <uenv/view.h>

namespace uenv {

struct env {
    std::unordered_map<std::string, concrete_uenv> uenvs;

    // the order of views matters: views are initialised in order
    std::vector<qualified_view_description> views;
};

util::expected<env, std::string>
concretise_env(const std::string& uenv_args,
               std::optional<std::string> view_args,
               std::optional<std::filesystem::path> repo_arg);

environment::variables
generate_environment(const env&, const environment::variables&,
                     std::optional<std::string> = std::nullopt);

environment::variables
generate_slurm_environment(const env&, const environment::variables&);

// returns true iff in a running uenv session
bool in_uenv_session();

} // namespace uenv
