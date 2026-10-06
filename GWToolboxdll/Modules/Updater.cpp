#include "stdafx.h"

#include <atomic>
#include <exception>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <bcrypt.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/BackupModule.h>
#include <Modules/Resources.h>
#include <Modules/Updater.h>

namespace github_api {
    struct ReleaseAsset {
        std::string name;
        std::string browser_download_url;
        uintmax_t size = 0;
        std::optional<std::string> digest;
    };

    struct Release {
        std::string tag_name;
        std::optional<std::string> body;
        bool prerelease = false;
        std::vector<ReleaseAsset> assets;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    using ReleaseType = Updater::ReleaseType;
    using Mode = Updater::Mode;

    Updater::Settings settings;

    enum Step {
        Checking,
        CheckAndAsk,
        CheckAndWarn,
        CheckAndAutoUpdate,
        Downloading,
        Success,
        Done
    };

    std::atomic<Step> step = Done;

    std::atomic_bool is_latest_version = true;
    std::atomic_bool startup_check_complete = false;
    bool show_star_request = false;

    GWToolboxRelease latest_release;
    GWToolboxRelease current_release;

    int CompareBaseVersions(const std::string_view left, const std::string_view right)
    {
        size_t left_pos = 0;
        size_t right_pos = 0;
        while (true) {
            uint64_t left_component = 0;
            while (left_pos < left.size() && std::isdigit(static_cast<unsigned char>(left[left_pos]))) {
                left_component = left_component * 10 + left[left_pos++] - '0';
            }
            uint64_t right_component = 0;
            while (right_pos < right.size() && std::isdigit(static_cast<unsigned char>(right[right_pos]))) {
                right_component = right_component * 10 + right[right_pos++] - '0';
            }
            if (left_component != right_component) return left_component < right_component ? -1 : 1;

            const bool left_has_next = left_pos + 1 < left.size() && left[left_pos] == '.' && std::isdigit(static_cast<unsigned char>(left[left_pos + 1]));
            const bool right_has_next = right_pos + 1 < right.size() && right[right_pos] == '.' && std::isdigit(static_cast<unsigned char>(right[right_pos + 1]));
            if (!left_has_next && !right_has_next) return 0;
            if (left_has_next) ++left_pos;
            if (right_has_next) ++right_pos;
        }
    }

    std::string_view VersionSuffix(const std::string_view version)
    {
        size_t position = 0;
        while (position < version.size() && (std::isdigit(static_cast<unsigned char>(version[position])) || version[position] == '.')) {
            ++position;
        }
        return version.substr(position);
    }

    int CompareNaturalVersions(const std::string_view left, const std::string_view right)
    {
        size_t left_pos = 0;
        size_t right_pos = 0;
        while (left_pos < left.size() && right_pos < right.size()) {
            if (std::isdigit(static_cast<unsigned char>(left[left_pos])) && std::isdigit(static_cast<unsigned char>(right[right_pos]))) {
                const auto left_start = left_pos;
                const auto right_start = right_pos;
                while (left_pos < left.size() && std::isdigit(static_cast<unsigned char>(left[left_pos]))) ++left_pos;
                while (right_pos < right.size() && std::isdigit(static_cast<unsigned char>(right[right_pos]))) ++right_pos;

                const auto left_number = left.substr(left_start, left_pos - left_start);
                const auto right_number = right.substr(right_start, right_pos - right_start);
                const auto left_nonzero = left_number.find_first_not_of('0');
                const auto right_nonzero = right_number.find_first_not_of('0');
                const auto normalized_left = left_number.substr(left_nonzero == std::string_view::npos ? left_number.size() : left_nonzero);
                const auto normalized_right = right_number.substr(right_nonzero == std::string_view::npos ? right_number.size() : right_nonzero);
                if (normalized_left.size() != normalized_right.size()) return normalized_left.size() < normalized_right.size() ? -1 : 1;
                if (normalized_left != normalized_right) return normalized_left < normalized_right ? -1 : 1;
                continue;
            }
            if (left[left_pos] != right[right_pos]) return left[left_pos] < right[right_pos] ? -1 : 1;
            ++left_pos;
            ++right_pos;
        }
        if (left_pos == left.size() && right_pos == right.size()) return 0;
        return left_pos == left.size() ? -1 : 1;
    }

    int CompareReleases(const GWToolboxRelease& left, const GWToolboxRelease& right)
    {
        if (const auto result = CompareBaseVersions(left.version, right.version); result != 0) return result;
        if (left.prerelease != right.prerelease) return left.prerelease ? -1 : 1;
        return left.prerelease ? CompareNaturalVersions(VersionSuffix(left.version), VersionSuffix(right.version)) : 0;
    }

    GWToolboxRelease* GetLatestRelease(GWToolboxRelease* release, const unsigned int max_tries = 5)
    {
        std::string response;
        unsigned int tries = 0;
        const auto url = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases";
        bool success = false;
        do {
            success = Resources::Download(url, response);
            tries++;
        } while (!success && tries < max_tries);
        if (!success) {
            Log::Log("Failed to download %s\n%s", url, response.c_str());
            return nullptr;
        }
        std::vector<github_api::Release> releases;
        if (auto ec = glz::read<json_opts>(releases, response); ec) {
            return nullptr;
        }
        for (const auto& js : releases) {
            if (js.tag_name.empty() || js.assets.empty()) {
                continue;
            }
            if (js.prerelease && settings.update_release_type == ReleaseType::Stable) {
                continue;
            }
            const auto version_number_len = js.tag_name.find(js.tag_name.contains("_Release") ? "_Release" : "_Beta", 0);
            if (version_number_len == std::string::npos) {
                continue;
            }
            for (const auto& asset : js.assets) {
                if (asset.name != "GWToolbox.dll" && asset.name != "GWToolboxdll.dll") {
                    continue;
                }
                release->download_url = asset.browser_download_url;
                release->version = js.tag_name.substr(0, version_number_len);
                if (js.prerelease) {
                    release->version += js.tag_name.substr(version_number_len + 1);
                }
                release->prerelease = js.prerelease;
                std::ranges::transform(release->version, release->version.begin(), [](const auto chr) { return static_cast<char>(std::tolower(chr)); });
                release->body = js.body.value_or("");
                release->asset_size = asset.size;
                release->digest = asset.digest;
                release->size = static_cast<uintmax_t>(std::ceil(asset.size / 16.0) * 16);
                return release;
            }
        }
        return nullptr;
    }

    char update_available_text[128];

    const char* UpdateAvailableText()
    {
        int written = 0;
        if (latest_release.version == current_release.version && latest_release.size != current_release.size) {
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s (%.2f kb) is available! You have %s (%.2f kb)",
                               latest_release.version.c_str(), latest_release.size > 0 ? latest_release.size / 1024.f : 0.f,
                               current_release.version.c_str(), current_release.size > 0 ? current_release.size / 1024.f : 0.f);
        }
        else {
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s is available! You have %s", latest_release.version.c_str(), current_release.version.c_str());
        }
        ASSERT(written > 0);
        return update_available_text;
    }

    void ReadUpdaterSettings(SettingsDoc& doc, ToolboxIni* legacy)
    {
        if (legacy) {
            if (!doc.Has("Updater", "update_mode"))
                settings.update_mode = static_cast<Mode>(legacy->GetLongValue("Updater", "update_mode", static_cast<long>(settings.update_mode)));
            if (!doc.Has("Updater", "update_release_type"))
                settings.update_release_type = static_cast<ReleaseType>(legacy->GetLongValue("Updater", "update_release_type", static_cast<long>(settings.update_release_type)));
            if (!doc.Has("Updater", "has_starred"))
                settings.has_starred = legacy->GetBoolValue("Updater", "has_starred", settings.has_starred);
        }
        doc.GetStruct("Updater", settings);
#ifdef _DEBUG
        settings.update_mode = Mode::DontCheckForUpdates;
        settings.update_release_type = ReleaseType::Beta;
#endif
    }

    bool FindUpdate(const bool forced, const HMODULE module = nullptr, const unsigned int max_tries = 5)
    {
        step = Checking;
        if (!forced && settings.update_mode == Mode::DontCheckForUpdates) {
            step = Done;
            return true;
        }
        if (!Updater::GetCurrentVersionInfo(&current_release, module) || !GetLatestRelease(&latest_release, max_tries)) {
            step = Done;
            return false;
        }

        const auto comparison = CompareReleases(latest_release, current_release);
        is_latest_version = comparison < 0 || (comparison == 0 && latest_release.size == current_release.size);
        if (is_latest_version) {
            step = Done;
            return true;
        }

        auto mode = forced ? Mode::CheckAndAsk : settings.update_mode;
        if constexpr (!std::string_view(GWTOOLBOXDLL_VERSION_BETA).empty()) {
            mode = Mode::CheckAndAsk;
        }
        switch (mode) {
            case Mode::CheckAndAsk:
                step = CheckAndAsk;
                break;
            case Mode::CheckAndAutoUpdate:
                step = CheckAndAutoUpdate;
                break;
            case Mode::CheckAndWarn:
                step = CheckAndWarn;
                break;
            case Mode::DontCheckForUpdates:
                step = Done;
                break;
            default:
                step = CheckAndAsk;
                break;
        }
        return true;
    }

    bool MatchesRelease(const std::string_view data, const GWToolboxRelease& release)
    {
        if (data.size() != release.asset_size || !release.digest || !release.digest->starts_with("sha256:")) return false;
        auto expected = release.digest->substr(7);
        if (expected.size() != 64) return false;
        std::ranges::transform(expected, expected.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        unsigned char digest[32];
        if (data.size() > MAXULONG || BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), digest, sizeof(digest)) != 0) return false;
        std::string actual;
        for (const auto byte : digest) actual += std::format("{:02x}", byte);
        return actual == expected;
    }

    bool InstallUpdate(const HMODULE module, const GWToolboxRelease& release, std::wstring& error)
    {
        wchar_t dllfile[MAX_PATH];
        const auto length = GetModuleFileNameW(module, dllfile, _countof(dllfile));
        if (!length || length == _countof(dllfile)) {
            error = L"Cannot find the Toolbox DLL path.";
            return false;
        }
        std::string data;
        if (!Resources::Download(release.download_url, data)) {
            error = TextUtils::StringToWString(data);
            return false;
        }
        if (!MatchesRelease(data, release)) {
            error = L"The download does not match the release size and SHA-256 checksum, or GitHub did not provide a valid checksum.";
            return false;
        }
        if (!BackupModule::CreateAutoBackup()) Log::Log("Failed to create pre-update backup; continuing with update anyway.");

        const auto path = std::filesystem::path(dllfile);
        const auto new_path = std::filesystem::path(path.wstring() + L".new");
        const auto old_path = std::filesystem::path(path.wstring() + L".old");
        const auto verify_file = [&release](const std::filesystem::path& file_path) {
            std::ifstream file(file_path, std::ios::binary);
            const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return file.is_open() && !file.bad() && MatchesRelease(content, release);
        };
        if (!Resources::WriteFile(new_path, data) || !verify_file(new_path)) {
            DeleteFileW(new_path.c_str());
            error = L"Could not write and verify the downloaded DLL. Antivirus software or protected folder access may be blocking the update.";
            return false;
        }
        DeleteFileW(old_path.c_str());
        if (!MoveFileW(path.c_str(), old_path.c_str())) {
            const auto code = GetLastError();
            DeleteFileW(new_path.c_str());
            error = std::format(L"Could not rename the current DLL (Windows error {}).", code);
            return false;
        }
        if (!MoveFileW(new_path.c_str(), path.c_str()) || !verify_file(path)) {
            const auto code = GetLastError();
            DeleteFileW(path.c_str());
            const auto restored = MoveFileW(old_path.c_str(), path.c_str());
            DeleteFileW(new_path.c_str());
            error = std::format(L"Could not install and verify the updated DLL (Windows error {}). {}",
                code, restored ? L"The original DLL was restored." : L"The original DLL remains in the .old file; restore it manually.");
            return false;
        }
        return true;
    }

    void DoUpdate()
    {
        step = Downloading;
        const auto module = GWToolbox::GetDLLModule();
        const auto release = latest_release;
        Resources::EnqueueWorkerTask([module, release] {
            std::wstring error;
            const auto success = InstallUpdate(module, release, error);
            Resources::EnqueueMainTask([success, error] {
                if (success) {
                    step = Success;
                    Log::WarningW(L"Update successful, please restart toolbox.");
                    return;
                }
                Log::ErrorW(L"Updater error - cannot update GWToolbox.dll\n%s", error.c_str());
                step = Done;
            });
        });
    }

    void DrawStarRequest()
    {
        if (!show_star_request) {
            return;
        }
        bool keep_open = true;
        ImGui::SetNextWindowSize(ImVec2(440.0f * ImGui::FontScale(), -1), ImGuiCond_Appearing);
        ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
        if (ImGui::Begin("Thank you for updating GWToolbox++!###gwtoolbox_star_request", &keep_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(
                "GWToolbox++ is built and maintained by a small group of volunteers, in our "
                "spare time, and given away for free. That's never going to change.");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "There is one small thing you can do that genuinely helps us. Because Toolbox "
                "has to inject into Guild Wars and read its memory, antivirus software often "
                "flags it as a false positive. A GitHub project with lots of stars and steady "
                "activity looks far more legitimate to those vendors, and over time that means "
                "fewer false detections for everyone who plays.");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "Stars also help us qualify for the free developer tooling we rely on to keep "
                "improving Toolbox - programs like JetBrains' open-source licences recently "
                "tightened their requirements, and project activity is part of how they decide.");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "So if Toolbox has been useful to you, would you take a few seconds to leave us "
                "a star on GitHub? It is completely free, it helps our reputation with antivirus "
                "software, and honestly - it makes our day every single time.");
            ImGui::Spacing();
            ImGui::TextUnformatted("Thank you for being part of this. <3");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Checkbox("I've already starred###gwtoolbox_has_starred", &settings.has_starred)) {
                show_star_request = false;
            }
            ImGui::Spacing();
            if (ImGui::Button("Star us on GitHub###gwtoolbox_open_star", ImVec2(200.0f * ImGui::FontScale(), 0))) {
                ShellExecute(nullptr, "open", "https://github.com/gwdevhub/GWToolboxpp", nullptr, nullptr, SW_SHOWNORMAL);
                show_star_request = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Maybe later###gwtoolbox_dismiss_star", ImVec2(120.0f * ImGui::FontScale(), 0))) {
                show_star_request = false;
            }
        }
        ImGui::End();
        if (!keep_open) {
            show_star_request = false;
        }
    }
}

const std::string& Updater::GetServerVersion()
{
    return latest_release.version;
}

const GWToolboxRelease* Updater::GetCurrentVersionInfo(GWToolboxRelease* out, const HMODULE module)
{
    wchar_t path[MAX_PATH];
    const auto length = GetModuleFileNameW(module ? module : GWToolbox::GetDLLModule(), path, _countof(path));
    if (!length || length == _countof(path)) {
        return nullptr;
    }
    std::error_code error;
    const auto size_bytes = std::filesystem::file_size(path, error);
    if (error) return nullptr;
    out->size = static_cast<uintmax_t>(std::ceil(size_bytes / 16.0) * 16);
    out->version = GWTOOLBOXDLL_VERSION;
    out->version.append(GWTOOLBOXDLL_VERSION_BETA);
    out->prerelease = !std::string_view(GWTOOLBOXDLL_VERSION_BETA).empty();
    std::ranges::transform(out->version, out->version.begin(), [](const auto chr) {
        return static_cast<char>(std::tolower(chr));
    });
    return out;
}

void Updater::Initialize()
{
    ToolboxUIElement::Initialize();
#ifndef _DEBUG
    SettingsRegistry::Register(this, settings);
#endif
}

void Updater::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxUIElement::LoadSettings(doc, legacy);
    ReadUpdaterSettings(doc, legacy);
#ifndef _DEBUG
    std::string previous_version;
    if (doc.Get(Name(), "dllversion", previous_version) && !previous_version.empty() && previous_version != GWTOOLBOXDLL_VERSION && !settings.has_starred) {
        show_star_request = true;
    }
#endif
    if (!startup_check_complete.exchange(false)) {
        CheckForUpdate();
    }
}

void Updater::SaveSettings(SettingsDoc& doc)
{
    ToolboxUIElement::SaveSettings(doc);
#ifndef _DEBUG
    doc.SetStruct(Name(), settings);
    doc.Set(Name(), "dllversion", std::string(GWTOOLBOXDLL_VERSION));

    const HMODULE module = GWToolbox::GetDLLModule();
    CHAR dllfile[MAX_PATH];
    const DWORD size = GetModuleFileName(module, dllfile, MAX_PATH);
    doc.Set(Name(), "dllpath", std::string(size > 0 ? dllfile : "error"));
#endif
}

void Updater::DrawSettingsInternal()
{
    ImGui::Text("Release channel:");
    const float btnWidth = 180.0f * ImGui::FontScale();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnWidth);
    if (ImGui::Button(step == Checking ? "Checking..." : "Check for updates", ImVec2(btnWidth, 0)) && step != Checking) {
        CheckForUpdate(true);
    }
    ImGui::RadioButton("Stable", (int*)&settings.update_release_type, static_cast<int>(ReleaseType::Stable));
    ImGui::RadioButton("Beta", (int*)&settings.update_release_type, static_cast<int>(ReleaseType::Beta));
    ImGui::Text("Update mode:");
    ImGui::RadioButton("Do not check for updates", (int*)&settings.update_mode, static_cast<int>(Mode::DontCheckForUpdates));
    ImGui::RadioButton("Check and display a message", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndWarn));
    ImGui::RadioButton("Check and ask before updating", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndAsk));
    ImGui::RadioButton("Check and automatically update", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndAutoUpdate));
}

void Updater::CheckForUpdate(const bool forced)
{
    if (step == Checking || step == Downloading) return;
    if (!forced && settings.update_mode == Mode::DontCheckForUpdates) {
        step = Done;
        return;
    }
    step = Checking;
    Resources::EnqueueWorkerTask([forced] {
        if (!FindUpdate(forced)) {
            Log::Flash("Error checking for updates");
        }
        else if (forced && is_latest_version) {
            Log::Flash("GWToolbox++ is up-to-date");
        }
    });
}

bool Updater::CheckBeforeInitialize(const HMODULE module) try
{
    ReadUpdaterSettings(*GWToolbox::GetSettingsDoc(), GWToolbox::OpenSettingsFile());
    startup_check_complete = true;
    if (!FindUpdate(false, module, 1)) {
        return MessageBoxW(nullptr,
            L"Could not check for Toolbox updates. Continue loading the current version?\n\nChoose No to leave Guild Wars running without Toolbox.",
            L"GWToolbox++ Update", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SETFOREGROUND) == IDYES;
    }
    const auto action = step.load();
    step = Done;
    if (action == Done) return true;

    auto message = TextUtils::StringToWString(UpdateAvailableText());
    if (action == CheckAndWarn) {
        message += L"\n\nChoose OK to continue with the current version, or Cancel to skip Toolbox this session.";
        return MessageBoxW(nullptr, message.c_str(), L"GWToolbox++ Update", MB_OKCANCEL | MB_ICONINFORMATION | MB_SETFOREGROUND) == IDOK;
    }
    if (action == CheckAndAsk) {
        message += L"\n\nUpdate before loading Toolbox?\n\nYes: update and unload Toolbox; reload it to use the new version.\nNo: continue with the current version.\nCancel: skip Toolbox this session.";
        const auto choice = MessageBoxW(nullptr, message.c_str(), L"GWToolbox++ Update", MB_YESNOCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND);
        if (choice != IDYES) return choice == IDNO;
    }
    std::wstring error;
    if (!InstallUpdate(module, latest_release, error)) {
        message = error + L"\n\nContinue loading the current version? Choose No to skip Toolbox this session.";
        return MessageBoxW(nullptr, message.c_str(), L"GWToolbox++ Update", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2 | MB_SETFOREGROUND) == IDYES;
    }
    MessageBoxW(nullptr, L"Toolbox was updated successfully. Reload Toolbox to use the new version.\n\nGuild Wars will keep running without Toolbox until you reload it.",
        L"GWToolbox++ Update", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
    return false;
}
catch (const std::exception& error)
{
    const auto message = L"Could not prepare Toolbox startup:\n\n" + TextUtils::StringToWString(error.what())
        + L"\n\nToolbox will not load this session. Guild Wars will keep running.";
    MessageBoxW(nullptr, message.c_str(), L"GWToolbox++ Update", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return false;
}

bool Updater::IsLatestVersion()
{
    return is_latest_version;
}

void Updater::Draw(IDirect3DDevice9*)
{
    DrawStarRequest();
    switch (step) {
        case CheckAndWarn:
            Log::Warning(UpdateAvailableText());
            step = Done;
            break;
        case CheckAndAsk: {
            if (!visible) {
                visible = true;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("Changes:");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::TextUnformatted("\nDo you want to update?");
            if (ImGui::Button("Later###gwtoolbox_dont_update", ImVec2(100, 0))) {
                step = Done;
            }
            ImGui::SameLine();
            if (ImGui::Button("OK###gwtoolbox_do_update", ImVec2(100, 0))) {
                DoUpdate();
            }
            ImGui::End();
            if (!visible) {
                step = Done;
            }
        }
        break;
        case CheckAndAutoUpdate:
            DoUpdate();
            break;
        case Downloading: {
            if (!visible) {
                break;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("Changes:");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::Text("\nDownloading update...");
            if (ImGui::Button("Hide", ImVec2(100, 0))) {
                visible = false;
            }
            ImGui::End();
        }
        break;
        case Success: {
            if (!visible) {
                break;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("Changes:");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::Text("\nUpdate successful, please restart toolbox.");
            if (ImGui::Button("OK", ImVec2(100, 0))) {
                visible = false;
            }
            if (!visible) {
                step = Done;
            }
            ImGui::End();
        }
        break;
    }
}
