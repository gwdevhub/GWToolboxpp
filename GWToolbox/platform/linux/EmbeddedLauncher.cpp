#include "EmbeddedLauncher.h"

#include "LauncherPaths.h"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

__asm__(
    ".data\n"
    ".balign 16\n"
    ".globl gwtb_windows_launcher_begin\n"
    "gwtb_windows_launcher_begin:\n"
    ".incbin \"" GWTB_WINDOWS_LAUNCHER_PATH "\"\n"
    ".globl gwtb_windows_launcher_end\n"
    "gwtb_windows_launcher_end:\n"
    ".text\n");

extern "C" const unsigned char gwtb_windows_launcher_begin[];
extern "C" const unsigned char gwtb_windows_launcher_end[];

namespace fs = std::filesystem;

namespace {

using Bytes = std::pair<const unsigned char*, std::size_t>;

const char* const kLauncherName = "GWToolbox.exe";

Bytes embeddedWindowsLauncher()
{
    return {gwtb_windows_launcher_begin, static_cast<std::size_t>(gwtb_windows_launcher_end - gwtb_windows_launcher_begin)};
}

bool sameAsEmbedded(const fs::path& path, const Bytes& bytes)
{
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::file_size(path, ec) != bytes.second) {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text.size() == bytes.second && std::memcmp(text.data(), bytes.first, bytes.second) == 0;
}

}

std::optional<fs::path> extractWindowsLauncher(std::string& error)
{
    const Bytes bytes = embeddedWindowsLauncher();
    if (bytes.second == 0) {
        error = "This build has no GWToolbox.exe.";
        return std::nullopt;
    }

    const fs::path target = launcherDirectory() / kLauncherName;
    if (sameAsEmbedded(target, bytes)) {
        return target;
    }

    const fs::path staged = target.string() + ".new";
    {
        std::ofstream file(staged, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.first), static_cast<std::streamsize>(bytes.second));
        if (!file) {
            error = "GWToolbox.exe couldn't be written to " + staged.string();
            return std::nullopt;
        }
    }

    std::error_code ec;
    fs::rename(staged, target, ec);
    if (ec) {
        fs::remove(staged, ec);
        error = "GWToolbox.exe couldn't be saved to " + target.string() + ": " + ec.message();
        return std::nullopt;
    }
    return target;
}
