#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <fmt/core.h>

#include <util/expected.h>

namespace uenv {

struct mount_description {
    std::string sqfs_path;
    std::string mount_path;
};

struct mount_pair {
    std::filesystem::path sqfs;
    std::filesystem::path mount;
};

struct tmpfs_description {
    std::filesystem::path mount;
    std::optional<std::uint64_t> size;
};

struct bindmount_description {
    std::filesystem::path src;
    std::filesystem::path dst;
};

// convert a description to a mount_pair that has a validated squashfs path
util::expected<mount_pair, std::string>
make_mount_pair(const mount_description& description);

using mount_list = std::vector<mount_pair>;

// one shot from string description to sorted and validated inputs
//
// auto mountvar ;
// if (auto mountvar = env.get("UENV_MOUNT_LIST")) {
//     auto mounts = parse_and_validate_mounts(mountvar.value());
// }
util::expected<mount_list, std::string>
parse_and_validate_mounts(const std::string& description,
                          bool mount_points_must_exist = true);

/// called as root, in slurm-plugin
util::expected<void, std::string> unshare_as_root();

/// mount sqfs images, make sure mnt ns has been unshared before calling this
/// function
util::expected<void, std::string> do_mount(const mount_list& mount_entries);

/// wrapper to `mount` from `sys/mount.h`
util::expected<void, std::string> mount(std::optional<std::string> source,
                                        const std::string& dest,
                                        std::optional<std::string> fstype,
                                        unsigned long mountflags,
                                        const void* nullable_data);

/// create a tmpfs
util::expected<void, std::string>
mount_tmpfs(std::filesystem::path dst, std::optional<std::uint64_t> size);

/// bind mount
util::expected<void, std::string> bind_mount(std::filesystem::path src,
                                             std::filesystem::path dst);

} // namespace uenv

template <> class fmt::formatter<uenv::mount_pair> {
  public:
    // parse format specification and store it:
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }
    // format a value using stored specification:
    template <typename FmtContext>
    constexpr auto format(uenv::mount_pair const& r, FmtContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}:{}", r.sqfs.string(),
                              r.mount.string());
    }
};
