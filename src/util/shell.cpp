#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <util/expected.h>

namespace util {

/// returns the path of the shell currently being used
util::expected<std::filesystem::path, std::string> current_shell() {
    auto env_shell = std::getenv("SHELL");

    if (env_shell == nullptr) {
        return util::unexpected("SHELL environment variable is not set");
    }

    const auto raw_path = std::filesystem::path(env_shell);
    std::error_code ec;
    const auto can_path = std::filesystem::canonical(raw_path, ec);

    if (ec) {
        return util::unexpected(
            fmt::format("unable to determine canonical form of the shell '{}'",
                        raw_path.string()));
    }

    return can_path;
}

int exec(const std::vector<std::string>& args) {
    std::vector<char*> argv;

    // unsafe {
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    fmt::print("[log] calling exec with the following:\n  {}\n",
               fmt::join(args, " "));
    int r = execvp(argv[0], argv.data());
    // } // end unsafe

    fmt::print("[error] unable to exec\n  {}\n", fmt::join(args, " "));

    return r;
}

} // namespace util
