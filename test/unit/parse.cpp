#include <catch2/catch_all.hpp>

#include <uenv/env.h>
#include <uenv/lex.h>
#include <uenv/mount.h>
#include <uenv/parse.h>

// forward declare private parsers
namespace uenv {
util::expected<std::string, parse_error> parse_name(lexer&);
util::expected<std::string, parse_error> parse_path(lexer&);
util::expected<uenv_label, parse_error> parse_uenv_label(lexer&);
util::expected<uenv_description, parse_error> parse_uenv_description(lexer&);
util::expected<view_description, parse_error> parse_view_description(lexer& L);
util::expected<mount_entry, parse_error> parse_mount_entry(lexer& L);
} // namespace uenv

TEST_CASE("sanitise inputs", "[parse]") {
    REQUIRE(uenv::strip("wombat") == "wombat");
    REQUIRE(uenv::strip("wombat soup") == "wombat soup");
    REQUIRE(uenv::strip("wombat-soup") == "wombat-soup");
    REQUIRE(uenv::strip("wombat \nsoup") == "wombat \nsoup");
    REQUIRE(uenv::strip("") == "");
    REQUIRE(uenv::strip(" ") == "");
    REQUIRE(uenv::strip(" x") == "x");
    REQUIRE(uenv::strip("x ") == "x");
    REQUIRE(uenv::strip(" x ") == "x");
    REQUIRE(uenv::strip(" \n\f  ") == "");
    REQUIRE(uenv::strip(" wombat") == "wombat");
    REQUIRE(uenv::strip("wombat \n") == "wombat");
    REQUIRE(uenv::strip("\t\f\vwombat \n") == "wombat");
}

TEST_CASE("parse names", "[parse]") {
    for (const auto& in : {"default", "prgenv-gnu", "a", "x.y", "x_y", "_"}) {
        auto L = uenv::lexer(in);
        auto result = uenv::parse_name(L);
        REQUIRE(result);
        REQUIRE(*result == in);
    }
}

TEST_CASE("parse path", "[parse]") {
    for (const auto& in :
         {"./etc", "/etc", "/etc.", "/etc/usr/file.txt", "/etc-car/hole_s/_.",
          ".", "./.ssh/config", ".bashrc", ".2", "./2-w_00",
          "/tmp/uenv-repo/create-6urQBN"}) {
        auto L = uenv::lexer(in);
        auto result = uenv::parse_path(L);
        REQUIRE(result);
        REQUIRE(*result == in);
    }
}

TEST_CASE("parse uenv label", "[parse]") {
    {
        auto L = uenv::lexer("prgenv-gnu");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(!result->version);
        REQUIRE(!result->tag);
        REQUIRE(!result->uarch);
        REQUIRE(!result->system);
    }
    {
        auto L = uenv::lexer("prgenv-gnu/24.7");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->version == "24.7");
        REQUIRE(!result->tag);
        REQUIRE(!result->uarch);
        REQUIRE(!result->system);
    }
    {
        auto L = uenv::lexer("prgenv-gnu/24.7:v1");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->version == "24.7");
        REQUIRE(result->tag == "v1");
        REQUIRE(!result->uarch);
        REQUIRE(!result->system);
    }
    {
        auto L = uenv::lexer("prgenv-gnu:v1");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(!result->version);
        REQUIRE(result->tag == "v1");
        REQUIRE(!result->uarch);
        REQUIRE(!result->system);
    }
    {
        auto L = uenv::lexer("prgenv-gnu/24.7:v1@santis%a100");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->version == "24.7");
        REQUIRE(result->tag == "v1");
        REQUIRE(result->uarch == "a100");
        REQUIRE(result->system == "santis");
    }
    {
        auto L = uenv::lexer("prgenv-gnu%a100");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->uarch == "a100");
    }
    {
        auto L = uenv::lexer("prgenv-gnu/24.7:v1%a100@santis");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->version == "24.7");
        REQUIRE(result->tag == "v1");
        REQUIRE(result->uarch == "a100");
        REQUIRE(result->system == "santis");
    }
    {
        auto L = uenv::lexer("prgenv-gnu/24.7:v1%a100");
        auto result = uenv::parse_uenv_label(L);
        REQUIRE(result);
        REQUIRE(result->name == "prgenv-gnu");
        REQUIRE(result->version == "24.7");
        REQUIRE(result->tag == "v1");
        REQUIRE(result->uarch == "a100");
        REQUIRE(!result->system);
    }
    for (auto defective_label : {
             "prgenv-gnu/:v1",
             "prgenv-gnu/wombat:",
             "prgenv-gnu/:v1",
             "prgenv-gnu/24:v1@",
             "prgenv-gnu/24:@",
             "prgenv-gnu/24:v1@gh200%",
             ".wombat",
         }) {
        auto L = uenv::lexer(defective_label);
        REQUIRE(!uenv::parse_uenv_label(L));
    }
}

TEST_CASE("parse namespace uenv label", "[parse]") {
    {
        auto r = uenv::parse_uenv_nslabel("");
        REQUIRE(r);
        REQUIRE(!r->nspace);
        REQUIRE(r->label.empty());
    }
    {
        auto r = uenv::parse_uenv_nslabel("deploy::");
        REQUIRE(r);
        REQUIRE(r->nspace == "deploy");
        REQUIRE(r->label.empty());
    }
    {
        auto r = uenv::parse_uenv_nslabel("prgenv-gnu/24.7:v1");
        REQUIRE(r);
        REQUIRE(!r->nspace);
        REQUIRE(r->label.name == "prgenv-gnu");
        REQUIRE(r->label.version == "24.7");
        REQUIRE(r->label.tag == "v1");
        REQUIRE(!r->label.uarch);
        REQUIRE(!r->label.system);
    }
    {
        auto r = uenv::parse_uenv_nslabel("deploy:::v1");
        REQUIRE(r);
        REQUIRE(r->nspace == "deploy");
        REQUIRE(!r->label.name);
        REQUIRE(!r->label.version);
        REQUIRE(r->label.tag == "v1");
        REQUIRE(!r->label.uarch);
        REQUIRE(!r->label.system);
    }
    {
        auto r = uenv::parse_uenv_nslabel("wombat::@*");
        REQUIRE(r);
        REQUIRE(r->nspace == "wombat");
        REQUIRE(!r->label.name);
        REQUIRE(!r->label.version);
        REQUIRE(!r->label.tag);
        REQUIRE(!r->label.uarch);
        REQUIRE(r->label.system == "*");
    }
    for (auto defective_label : {
             "build::prgenv-gnu/:v1",
             "build::prgenv-gnu/wombat:",
             "build::.wombat",
             "-build::.wombat",
             "_build::.wombat",
         }) {
        REQUIRE(!uenv::parse_uenv_nslabel(defective_label));
    }
}

TEST_CASE("parse view list", "[parse]") {
    {
        auto in = "spack,modules";
        auto result = uenv::parse_view_args(in);
        REQUIRE(result);
        REQUIRE((*result)[0].name == "spack");
        REQUIRE(!(*result)[0].uenv);
        REQUIRE((*result)[1].name == "modules");
        REQUIRE(!(*result)[1].uenv);
    }
    {
        auto in = "default";
        auto result = uenv::parse_view_args(in);
        REQUIRE(result);
        REQUIRE((*result)[0].name == "default");
        REQUIRE(!(*result)[0].uenv);
    }
    {
        auto in = "prgenv-gnu:default,wombat";
        auto result = uenv::parse_view_args(in);
        REQUIRE(result);
        REQUIRE((*result)[0].name == "default");
        REQUIRE((*result)[0].uenv == "prgenv-gnu");
        REQUIRE((*result)[1].name == "wombat");
        REQUIRE(!(*result)[1].uenv);
    }
    for (auto in : {"", " ", "default, spack", "jack/bull"}) {
        REQUIRE(!uenv::parse_view_args(in));
    }
}

TEST_CASE("parse uenv list", "[parse]") {
    {
        auto in = "prgenv-gnu/24.7:rc1:/user-environment";
        auto result = uenv::parse_uenv_args(in);
        REQUIRE(result);
        REQUIRE(result->size() == 1);
        auto d = (*result)[0];
        auto l = *d.label();
        REQUIRE(l.name == "prgenv-gnu");
        REQUIRE(l.version == "24.7");
        REQUIRE(l.tag == "rc1");
        REQUIRE(*d.mount() == "/user-environment");
    }
    {
        // test case where no tag is provide - ensure that the mount point
        // after the : character is read correctly.
        auto in = "prgenv-gnu/24.7:/user-environment";
        auto result = uenv::parse_uenv_args(in);
        REQUIRE(result);
        REQUIRE(result->size() == 1);
        auto d = (*result)[0];
        auto l = *d.label();
        REQUIRE(l.name == "prgenv-gnu");
        REQUIRE(l.version == "24.7");
        REQUIRE(*d.mount() == "/user-environment");
    }
    {
        // test that no mount point is handled correctly
        auto in = "prgenv-gnu/24.7:rc1";
        auto result = uenv::parse_uenv_args(in);
        REQUIRE(result);
        REQUIRE(result->size() == 1);
        auto d = (*result)[0];
        auto l = *d.label();
        REQUIRE(l.name == "prgenv-gnu");
        REQUIRE(l.version == "24.7");
        REQUIRE(l.tag == "rc1");
        REQUIRE(!d.mount());
    }
    {
        auto in = "/scratch/.uenv-images/sdfklsdf890df9a87sdf/store.squashfs:/"
                  "user-environment/store-asdf/my-image_mnt_point3//"
                  ",prgenv-nvidia";
        auto result = uenv::parse_uenv_args(in);
        REQUIRE(result);
        REQUIRE(result->size() == 2);
        auto d = (*result)[0];
        auto n = *d.filename();
        REQUIRE(n ==
                "/scratch/.uenv-images/sdfklsdf890df9a87sdf/store.squashfs");
        REQUIRE(*d.mount() ==
                "/user-environment/store-asdf/my-image_mnt_point3//");
        d = (*result)[1];
        auto l = *d.label();
        REQUIRE(l.name == "prgenv-nvidia");
        REQUIRE(!l.version);
        REQUIRE(!l.tag);
    }
}

TEST_CASE("parse mount", "[parse]") {
    {
        auto in = "/images/store.squashfs:/user-environment";
        auto result = uenv::parse_mount_list(in);
        REQUIRE(result);
        REQUIRE(result->size() == 1);
        auto m = (*result)[0];
        REQUIRE(m.sqfs_path == "/images/store.squashfs");
        REQUIRE(m.mount_path == "/user-environment");
    }
    {
        auto in = "/images/store.squashfs:/user-environment,/images/"
                  "wombat.squashfs:/user-tools";
        auto result = uenv::parse_mount_list(in);
        REQUIRE(result);
        REQUIRE(result->size() == 2);
        auto m = (*result)[0];
        REQUIRE(m.sqfs_path == "/images/store.squashfs");
        REQUIRE(m.mount_path == "/user-environment");
        m = (*result)[1];
        REQUIRE(m.sqfs_path == "/images/wombat.squashfs");
        REQUIRE(m.mount_path == "/user-tools");
    }
    {
        auto in = "";
        auto result = uenv::parse_mount_list(in);
        REQUIRE(!result);
    }
}

TEST_CASE("parse registry entry", "[parse]") {
    {
        auto r = uenv::parse_registry_entry("deploy/balfrin/a100/mch/v8/rc1");
        REQUIRE(r);
        REQUIRE(r->nspace == "deploy");
        REQUIRE(r->system == "balfrin");
        REQUIRE(r->uarch == "a100");
        REQUIRE(r->name == "mch");
        REQUIRE(r->version == "v8");
        REQUIRE(r->tag == "rc1");
    }

    {
        auto r = uenv::parse_registry_entry(
            "build/eiger/zen2/prgenv-gnu/24.11/1529952520");
        REQUIRE(r);
        REQUIRE(r->nspace == "build");
        REQUIRE(r->system == "eiger");
        REQUIRE(r->uarch == "zen2");
        REQUIRE(r->name == "prgenv-gnu");
        REQUIRE(r->version == "24.11");
        REQUIRE(r->tag == "1529952520");
    }
    for (auto s : {
             "build/eiger/zen2/cp2k/2024.3/1456857513",
             "deploy/todi/gh200/cp2k/2024.3/v1",
             "deploy/daint/gh200/cp2k/2024.3/v1",
             "deploy/eiger/zen2/cp2k/2024.3/v1",
             "build/eiger/zen2/prgenv-gnu/24.7/1459977671",
             "build/eiger/zen2/julia/24.9/1462160001",
             "deploy/todi/gh200/linaro-forge/24.0.2/v1",
             "build/todi/gh200/prgenv-gnu/24.7/1465755671",
             "build/todi/gh200/eurohack/24.9/1466916530",
             "build/eiger/zen2/cp2k/2024.3/1468668296",
             "build/todi/gh200/eurohack/24.9/1475733521",
             "deploy/todi/gh200/eurohack/24.9/rc1",
             "deploy/todi/gh200/quantumespresso/v7.3.1/v2",
             "build/balfrin/a100/climana/24.10/1481801863",
             "deploy/balfrin/a100/climana/24.10/rc1",
             "build/todi/gh200/eurohack/24.9/1481930276",
             "deploy/todi/gh200/eurohack/24.9/v2",
             "build/todi/gh200/eurohack/24.9-nvhpc/1486605082",
             "deploy/todi/gh200/eurohack/24.9-nvhpc/v2-nvhpc",
             "build/todi/gh200/eurohack/24.9-nvhpc/1486538704",
             "deploy/balfrin/a100/climana/24.10/v1",
             "build/eiger/zen2/prgenv-gnu/24.7/1496614274",
             "build/todi/gh200/gromacs/2024/1502090596",
             "build/todi/gh200/cp2k/2024.3/1502220409",
             "deploy/todi/gh200/cp2k/2024.3/v2",
             "build/balfrin/a100/mch/prgenv-icon/1518374117",
             "deploy/balfrin/a100/mch/prgenv-icon/rc1",
             "build/balfrin/a100/mch/v8/1529719759",
             "deploy/balfrin/a100/mch/v8/rc1",
             "build/eiger/zen2/prgenv-gnu/24.11/1529952520",
         }) {
        auto r = uenv::parse_registry_entry(s);
        if (!r) {
            fmt::println("{}", r.error().message());
        } else {
            REQUIRE(r);
        }
    }
}

TEST_CASE("date", "[parse]") {
    {
        auto in = "2024-12-3";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(result);
        REQUIRE(result->year == 2024);
        REQUIRE(result->month == 12);
        REQUIRE(result->day == 3);
    }
    {
        auto in = "2024-12-03";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(result);
        REQUIRE(result->year == 2024);
        REQUIRE(result->month == 12);
        REQUIRE(result->day == 3);
    }
    {
        auto in = "2024-2-29";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(result);
        REQUIRE(result->year == 2024);
        REQUIRE(result->month == 2);
        REQUIRE(result->day == 29);
    }
    {
        auto in = "2024-03-11 17:08:35.976000+00:00";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(result);
        REQUIRE(result->year == 2024);
        REQUIRE(result->month == 3);
        REQUIRE(result->day == 11);
        REQUIRE(result->hour == 17);
        REQUIRE(result->minute == 8);
        REQUIRE(result->second == 35);

        REQUIRE(*result == *uenv::parse_uenv_date("2024-03-11 17:08:35"));
    }
    {
        auto in = "2024-10-15T11:46:22.533Z";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(result);
        REQUIRE(result->year == 2024);
        REQUIRE(result->month == 10);
        REQUIRE(result->day == 15);
        REQUIRE(result->hour == 11);
        REQUIRE(result->minute == 46);
        REQUIRE(result->second == 22);
    }
    {
        for (auto in : {"2024-0-3", "2024-13-3", "2023-2-29"}) {
            auto result = uenv::parse_uenv_date(in);
            REQUIRE(!result);
        }
    }
    {
        auto in = "2024-1a-3";
        auto result = uenv::parse_uenv_date(in);
        REQUIRE(!result);
    }
}
