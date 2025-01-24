#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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

std::unordered_map<std::string, std::string> getenv(const env&);

util::expected<int, std::string>
setenv(const std::unordered_map<std::string, std::string>& variables,
       const std::string& prefix);

// returns true iff in a running uenv session
bool in_uenv_session();

} // namespace uenv
