#pragma once

#include <string>
#include <vector>

#include <util/expected.h>

namespace uenv {

struct mount_entry {
    std::string sqfs_path;
    std::string mount_path;
};

} // namespace uenv