#pragma once

#include <filesystem>
#include <optional>

#include <fmt/format.h>

#include <util/expected.h>

namespace uenv {

struct squashfs_image {
    // the absolute path of the squashfs file
    std::filesystem::path sqfs;

    // a temporary path containing the meta data
    std::optional<std::filesystem::path> meta;

    // the sha256 hash of the file
    std::string hash;
};

util::expected<squashfs_image, std::string>
validate_squashfs_image(const std::string& path);

} // namespace uenv

template <> class fmt::formatter<uenv::squashfs_image> {
  public:
    // parse format specification and store it:
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }
    // format a value using stored specification:
    template <typename FmtContext>
    constexpr auto format(uenv::squashfs_image const& img,
                          FmtContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "squashfs_file(path {}, meta {}, hash {})",
                              img.sqfs, img.meta, img.hash);
    }
};
