#include "ScanResult.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

#include "Command.h"
#include "EmbeddedLauncher.h"
#include "Installation.h"
#include "Messages.h"
#include "Inject.h"

namespace fs = std::filesystem;

namespace {

constexpr auto kScanTimeout = std::chrono::seconds(120);
constexpr auto kCheckTimeout = std::chrono::seconds(20);
const std::set<std::string> kWineInternalVariables{"WINESERVERSOCKET", "WINELOADERNOEXEC"};

struct GameProcess {
    int pid = 0;
    std::string prefix;
    fs::path exe;
    std::vector<std::string> environment;
};

std::vector<std::string> readNulSeparated(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < data.size()) {
        const size_t end = data.find('\0', start);
        const size_t stop = end == std::string::npos ? data.size() : end;
        if (stop > start) {
            parts.push_back(data.substr(start, stop - start));
        }
        start = stop + 1;
    }
    return parts;
}

std::string lowerCase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool isGuildWarsCommand(const std::vector<std::string>& args)
{
    if (args.empty()) {
        return false;
    }
    std::string first = args[0];
    std::replace(first.begin(), first.end(), '/', '\\');
    first = lowerCase(first);
    return first == "gw.exe" || first.ends_with("\\gw.exe");
}

std::optional<std::string> variable(const std::vector<std::string>& environment, const std::string& name)
{
    const std::string prefix = name + "=";
    for (const std::string& entry : environment) {
        if (entry.starts_with(prefix)) {
            return entry.substr(prefix.size());
        }
    }
    return std::nullopt;
}

std::vector<GameProcess> findGameProcesses()
{
    std::vector<GameProcess> games;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator("/proc", ec)) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c); })) {
            continue;
        }
        const auto args = readNulSeparated(entry.path() / "cmdline");
        if (!isGuildWarsCommand(args)) {
            continue;
        }
        GameProcess game;
        game.pid = std::stoi(name);
        game.exe = fs::read_symlink(entry.path() / "exe", ec);
        game.environment = readNulSeparated(entry.path() / "environ");
        game.prefix = variable(game.environment, "WINEPREFIX").value_or("");
        games.push_back(std::move(game));
    }
    return games;
}

std::optional<fs::path> siblingWine(const fs::path& directory)
{
    const fs::path wine = directory / "wine";
    std::error_code ec;
    if (fs::is_regular_file(wine, ec)) {
        return wine;
    }
    return std::nullopt;
}

std::optional<fs::path> wineServerWine(const std::string& prefix)
{
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator("/proc", ec)) {
        const fs::path exe = fs::read_symlink(entry.path() / "exe", ec);
        if (ec || !exe.filename().string().starts_with("wineserver")) {
            continue;
        }
        const auto server_prefix = variable(readNulSeparated(entry.path() / "environ"), "WINEPREFIX");
        if (server_prefix != prefix) {
            continue;
        }
        if (const auto wine = siblingWine(exe.parent_path())) {
            return wine;
        }
    }
    return std::nullopt;
}

std::optional<fs::path> preloaderWine(const fs::path& preloader)
{
    fs::path directory = preloader.parent_path();
    for (int depth = 0; depth < 4 && !directory.empty() && directory != directory.root_path(); ++depth) {
        for (const fs::path& candidate : {directory / "bin", directory}) {
            if (const auto wine = siblingWine(candidate)) {
                return wine;
            }
        }
        directory = directory.parent_path();
    }
    return std::nullopt;
}

std::optional<fs::path> wineFor(const GameProcess& game)
{
    const std::string name = game.exe.filename().string();
    if (name == "wine" || name == "wine64") {
        return game.exe;
    }
    if (const auto wine = wineServerWine(game.prefix)) {
        return wine;
    }
    return preloaderWine(game.exe);
}

std::vector<std::string> environmentFor(const GameProcess& game)
{
    std::vector<std::string> environment;
    for (const std::string& entry : game.environment) {
        const std::string name = entry.substr(0, entry.find('='));
        if (!kWineInternalVariables.contains(name) && name != "WINEDEBUG") {
            environment.push_back(entry);
        }
    }
    environment.push_back("WINEDEBUG=-all");
    return environment;
}

std::optional<Json> jsonLine(const std::string& text)
{
    std::string line;
    std::string found;
    for (const char c : text) {
        if (c == '\n') {
            if (line.starts_with("{")) {
                found = line;
            }
            line.clear();
        }
        else {
            line += c;
        }
    }
    if (line.starts_with("{")) {
        found = line;
    }
    if (found.empty()) {
        return std::nullopt;
    }
    Json parsed = Json::parse(found, nullptr, false);
    if (parsed.is_discarded()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<fs::path> launcherExecutable(std::string& error)
{
    if (const char* override_path = std::getenv("GWTOOLBOX_EXE")) {
        return fs::path(override_path);
    }
    return extractWindowsLauncher(error);
}

void appendTargets(ScanResult& result, const Json& scan, const GameProcess& game, const fs::path& wine)
{
    for (const Json& process : scan.at("processes")) {
        ScanTarget target;
        target.prefix = game.prefix;
        target.wine = wine.string();
        target.pid = process.contains("pid") && process["pid"].is_number() ? process["pid"].get<uint32_t>() : 0;
        target.kind = process.at("kind").get<std::string>();
        target.character = process.at("character").get<std::string>();
        target.injected = process.at("injected").get<bool>();
        result.targets.push_back(std::move(target));
    }
}

std::string versionNumber(const std::string& text)
{
    const auto start = text.find_first_of("0123456789");
    if (start == std::string::npos) {
        return {};
    }
    const auto end = text.find_first_not_of("0123456789.", start);
    return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

std::optional<std::string> wineProblem(const fs::path& wine, const std::vector<std::string>& environment)
{
    const CommandOutput client = runCommand({wine.string(), "--version"}, environment, kCheckTimeout);
    if (client.exit_code != 0 || versionNumber(client.text).empty()) {
        return "Wine at " + wine.string() + " didn't start (exit " + std::to_string(client.exit_code) + ").";
    }

    const fs::path server = wine.parent_path() / "wineserver";
    std::error_code ec;
    if (!fs::is_regular_file(server, ec)) {
        return std::nullopt;
    }
    const CommandOutput server_output = runCommand({server.string(), "--version"}, environment, kCheckTimeout);
    const std::string server_version = versionNumber(server_output.text);
    if (server_output.exit_code == 0 && !server_version.empty() && server_version != versionNumber(client.text)) {
        return "Wine " + versionNumber(client.text) + " doesn't match its wineserver (" + server_version + ").";
    }
    return std::nullopt;
}

}

ScanResult ScanGuildWars()
{
    ScanResult result;
    std::string launcher_error;
    const auto launcher = launcherExecutable(launcher_error);
    if (!launcher) {
        result.errors.push_back({"GWToolbox", launcher_error});
        return result;
    }
    std::set<std::string> scanned;

    for (const GameProcess& game : findGameProcesses()) {
        if (game.prefix.empty()) {
            result.errors.push_back({"pid " + std::to_string(game.pid), "the game's environment has no WINEPREFIX"});
            continue;
        }
        if (!scanned.insert(game.prefix).second) {
            continue;
        }
        const auto wine = wineFor(game);
        if (!wine) {
            result.errors.push_back({game.prefix, "no Wine program found for this prefix"});
            continue;
        }
        if (const auto problem = wineProblem(*wine, environmentFor(game))) {
            result.errors.push_back({game.prefix, *problem});
            continue;
        }

        const CommandOutput output = runCommand({wine->string(), launcher->string(), "/scan", "/json"}, environmentFor(game), kScanTimeout);
        const auto scan = jsonLine(output.text);
        if (!scan || !scan->contains("processes")) {
            result.errors.push_back({game.prefix, "the scan printed no result (exit " + std::to_string(output.exit_code) + ")"});
            continue;
        }
        appendTargets(result, *scan, game, *wine);
        if (scan->contains("error") && scan->at("processes").empty()) {
            result.errors.push_back({game.prefix, scan->at("error").get<std::string>()});
        }
    }

    std::sort(result.targets.begin(), result.targets.end(), [](const ScanTarget& a, const ScanTarget& b) {
        return std::tie(a.character, a.prefix) < std::tie(b.character, b.prefix);
    });
    return result;
}

InjectOutcome injectTarget(const ScanTarget& target)
{
    if (target.kind != "native" || target.pid == 0) {
        return {false, "Only native game processes can be injected into."};
    }

    const fs::path dll = FindInstalledDll();
    if (dll.empty()) {
        return {false, "GWToolbox isn't installed yet. Install it from the launcher first."};
    }
    std::string launcher_error;
    const auto launcher = launcherExecutable(launcher_error);
    if (!launcher) {
        return {false, launcher_error};
    }

    const std::vector<GameProcess> games = findGameProcesses();
    const auto game = std::ranges::find_if(games, [&target](const GameProcess& candidate) {
        return candidate.prefix == target.prefix;
    });
    if (game == games.end()) {
        return {false, "Guild Wars is no longer running in that prefix."};
    }

    const fs::path stage = fs::path(target.prefix) / "drive_c" / "GWToolbox";
    const fs::path staged_exe = stage / "GWToolbox.exe";
    std::error_code ec;
    fs::create_directories(stage, ec);
    fs::copy_file(*launcher, staged_exe, fs::copy_options::overwrite_existing, ec);
    if (!ec) {
        fs::copy_file(dll, stage / "GWToolboxdll.dll", fs::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        return {false, "GWToolbox couldn't be copied into the prefix: " + ec.message()};
    }

    const CommandOutput output = runCommand({target.wine, staged_exe.string(), "/localdll", "/pid", std::to_string(target.pid), "/json"},
                                            environmentFor(*game), kScanTimeout);
    const auto reply = jsonLine(output.text);
    if (!reply || !reply->contains("ok")) {
        return {false, "GWToolbox printed no result (exit " + std::to_string(output.exit_code) + ")."};
    }
    return {reply->at("ok").get<bool>(), reply->value("message", std::string())};
}
