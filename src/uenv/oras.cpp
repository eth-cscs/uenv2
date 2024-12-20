// #include <filesystem>
#include <unistd.h>

#include <string>
#include <vector>

#include <barkeep/barkeep.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <uenv/oras.h>
#include <uenv/uenv.h>
#include <util/color.h>
#include <util/fs.h>
#include <util/signal.h>
#include <util/subprocess.h>

namespace uenv {
namespace oras {

using opt_creds = std::optional<credentials>;

struct oras_output {
    int rcode = -1;
    std::string stdout;
    std::string stderr;
};

std::vector<std::string>
redact_arguments(const std::vector<std::string>& args) {
    std::vector<std::string> redacted_args{};
    bool redact_next = false;
    for (auto arg : args) {
        if (redact_next) {
            redacted_args.push_back(std::string(arg.size(), 'X'));
            redact_next = false;
        } else if (arg.find("password") != std::string::npos) {
            if (auto eqpos = arg.find("="); eqpos != std::string::npos) {
                redacted_args.push_back(
                    std::string(arg.begin(), arg.begin() + eqpos + 1) +
                    std::string(arg.size() - eqpos, 'X'));
            } else {
                redacted_args.push_back(arg);
                redact_next = true;
            }
        } else {
            redacted_args.push_back(arg);
        }
    }

    return redacted_args;
}

oras_output run_oras(std::vector<std::string> args) {
    auto oras = util::oras_path();
    if (!oras) {
        spdlog::error("no oras executable found");
        return {.rcode = -1};
    }

    args.insert(args.begin(), oras->string());
    spdlog::trace("run_oras: {}", fmt::join(redact_arguments(args), " "));
    auto proc = util::run(args);

    auto result = proc->wait();

    return {.rcode = result,
            .stdout = proc->out.string(),
            .stderr = proc->err.string()};
}

util::expected<util::subprocess, std::string>
run_oras_async(std::vector<std::string> args) {
    auto oras = util::oras_path();
    if (!oras) {
        return util::unexpected{"no oras executable found"};
    }

    args.insert(args.begin(), oras->string());
    spdlog::trace("run_oras: {}", fmt::join(args, " "));
    return util::run(args);
}

util::expected<std::vector<std::string>, std::string>
discover(const std::string& registry, const std::string& nspace,
         const uenv_record& uenv, const opt_creds token) {
    auto address =
        fmt::format("{}/{}/{}/{}/{}/{}:{}", registry, nspace, uenv.system,
                    uenv.uarch, uenv.name, uenv.version, uenv.tag);

    std::vector<std::string> args = {"discover",        "--format",  "json",
                                     "--artifact-type", "uenv/meta", address};
    if (token) {
        args.push_back("--password");
        args.push_back(token->token);
        args.push_back("--username");
        args.push_back(token->username);
    }
    auto result = run_oras(args);

    if (result.rcode) {
        spdlog::error("oras discover {}: {}", result.rcode, result.stderr);
        return util::unexpected{result.stderr};
    }

    std::vector<std::string> manifests;
    using json = nlohmann::json;
    try {
        const auto raw = json::parse(result.stdout);
        for (const auto& j : raw["manifests"]) {
            manifests.push_back(j["digest"]);
        }
    } catch (std::exception& e) {
        spdlog::error("unable to parse oras discover json: {}", e.what());
        return util::unexpected(fmt::format("", e.what()));
    }

    return manifests;
}

util::expected<void, int>
pull_digest(const std::string& registry, const std::string& nspace,
            const uenv_record& uenv, const std::string& digest,
            const std::filesystem::path& destination, const opt_creds token) {
    auto address =
        fmt::format("{}/{}/{}/{}/{}/{}@{}", registry, nspace, uenv.system,
                    uenv.uarch, uenv.name, uenv.version, digest);

    spdlog::debug("oras::pull_digest: {}", address);

    std::vector<std::string> args{"pull", "--output", destination.string(),
                                  address};
    if (token) {
        args.push_back("--password");
        args.push_back(token->token);
        args.push_back("--username");
        args.push_back(token->username);
    }
    auto proc = run_oras(args);

    if (proc.rcode) {
        spdlog::error("unable to pull digest with oras: {}", proc.stderr);
        return util::unexpected{proc.rcode};
    }

    return {};
}

util::expected<void, int> pull_tag(const std::string& registry,
                                   const std::string& nspace,
                                   const uenv_record& uenv,
                                   const std::filesystem::path& destination,
                                   const opt_creds token) {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    namespace bk = barkeep;

    auto address =
        fmt::format("{}/{}/{}/{}/{}/{}:{}", registry, nspace, uenv.system,
                    uenv.uarch, uenv.name, uenv.version, uenv.tag);

    spdlog::debug("oras::pull_tag: {}", address);
    std::vector<std::string> args{"pull",     "--concurrency",      "10",
                                  "--output", destination.string(), address};
    if (token) {
        args.push_back("--password");
        args.push_back(token->token);
        args.push_back("--username");
        args.push_back(token->username);
    }
    auto proc = run_oras_async(args);

    if (!proc) {
        spdlog::error("unable to pull tag with oras: {}", proc.error());
        return util::unexpected{-1};
    }

    util::set_signal_catcher();

    const fs::path& sqfs = destination / "store.squashfs";

    std::size_t downloaded_mb{0u};
    std::size_t total_mb{uenv.size_byte / (1024 * 1024)};
    auto bar = bk::ProgressBar(
        &downloaded_mb,
        {
            .total = total_mb,
            .message = fmt::format("pulling {}", uenv.id.string()),
            .speed = 1.,
            .speed_unit = "MB/s",
            .style = color::use_color() ? bk::ProgressBarStyle::Rich
                                        : bk::ProgressBarStyle::Bars,
            .no_tty = !isatty(fileno(stdout)),
        });
    while (!proc->finished()) {
        std::this_thread::sleep_for(100ms);
        // handle a signal, usually SIGTERM or SIGINT
        if (util::signal_raised()) {
            spdlog::warn("signal raised - interrupting download");
            throw util::signal_exception(util::last_signal_raised());
        }
        if (fs::is_regular_file(sqfs)) {
            auto downloaded_bytes = fs::file_size(sqfs);
            downloaded_mb = downloaded_bytes / (1024 * 1024);
        }
    }
    downloaded_mb = total_mb;

    if (proc->rvalue()) {
        spdlog::error("unable to pull tag with oras: {}", proc->err.string());
        return util::unexpected{proc->rvalue()};
    }

    return {};
}

util::expected<void, int>
copy(const std::string& registry, const std::string& src_nspace,
     const uenv_record& src_uenv, const std::string& dst_nspace,
     const uenv_record& dst_uenv, const std::optional<credentials> token) {

    auto address = [&registry](auto& nspace, auto& record) -> std::string {
        return fmt::format("{}/{}/{}/{}/{}/{}:{}", registry, nspace,
                           record.system, record.uarch, record.name,
                           record.version, record.tag);
    };
    const auto src_url = address(src_nspace, src_uenv);
    const auto dst_url = address(dst_nspace, dst_uenv);

    std::vector<std::string> args = {"cp",          "--concurrency", "10",
                                     "--recursive", src_url,         dst_url};
    if (token) {
        args.push_back(fmt::format("--from-password={}", token->token));
        args.push_back(fmt::format("--from-username={}", token->username));
        args.push_back(fmt::format("--to-password={}", token->token));
        args.push_back(fmt::format("--to-username={}", token->username));
    }
    auto result = run_oras(args);

    if (result.rcode) {
        spdlog::error("oras cp {}: {}", result.rcode, result.stderr);
        return util::unexpected{result.rcode};
    }

    return {};
}

} // namespace oras
} // namespace uenv
