#include <filesystem>

#include <catch2/catch_all.hpp>
#include <fmt/core.h>

#include <util/fs.h>
#include <util/subprocess.h>

namespace fs = std::filesystem;

TEST_CASE("make_temp_dir", "[fs]") {
    auto dir1 = util::make_temp_dir();
    REQUIRE(fs::is_directory(dir1));
    auto dir2 = util::make_temp_dir();
    REQUIRE(dir1 != dir2);
    fmt::println("end-of-funtion");
}

TEST_CASE("unsquashfs", "[fs]") {
    std::string sqfs =
        "/home/bcumming/software/uenv2/test/scratch/repos/apptool/images/"
        "0343a5636e6d59ed8e5f7fcc35c34719597ff024be0ce796cc0532804b3aeefa/"
        "store.squashfs";
    {
        auto meta = util::unsquashfs_tmp(sqfs, "meta");
        fmt::println("returned from unsquashfs");
        REQUIRE(meta);
        REQUIRE(fs::is_directory(*meta));
        REQUIRE(fs::is_directory(*meta / "meta"));
    }
    {
        const int nbuf = 5;
        {
            for (int i = 0; i < nbuf; ++i) {
                auto x = util::unsquashfs_tmp(sqfs, "meta/env.json");
            }
        }
    }
}
