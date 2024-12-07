#pragma once

#include <optional>
#include <string>

#include <uenv/repository.h>
#include <util/expected.h>

namespace site {

// return the name of the current system
// on CSCS systems this is derived from the CLUSTER_NAME environment variable
std::optional<std::string> get_system_name(std::optional<std::string>);

// default namespace for image deployment
std::string default_namespace();

util::expected<uenv::repository, std::string>
registry_listing(const std::string& nspace);

std::string registry_url();

} // namespace site
