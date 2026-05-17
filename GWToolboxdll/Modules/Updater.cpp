#include "stdafx.h"

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Modules/Updater.h>

namespace github_api {
    struct ReleaseAsset {
        std::string name;
        std::string browser_download_url;
        double size = 0.0; // bytes (double to survive 53-bit JSON precision)
    };

    struct Release {
        std::string tag_name;
        std::string body;
        bool prerelease = false;
        std::vector<ReleaseAsset> assets;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};
    // 0=none, 1=check and warn, 2=check and ask, 3=check and do
    enum class ReleaseType : int {
        Stable,
        Beta
    };
    enum class Mode : int {
        DontCheckForUpdates,
        CheckAndWarn,
        CheckAndAsk,
        CheckAndAutoUpdate
    };

    ReleaseType release_type = ReleaseType::Stable;
    Mode mode = Mode::CheckAndAsk;

    // 0=checking, 1=asking, 2=downloading, 3=done
    enum Step {
        Checking,
        CheckAndAsk,
        CheckAndWarn,
        CheckAndAutoUpdate,
        Downloading,
        Success,
        Done
    };

    Step step = Checking;

    bool is_latest_version = true;
    bool notified = false;
    bool forced_ask = false;
    clock_t last_check = 0;

    GWToolboxRelease latest_release;
    GWToolboxRelease current_release;

    GWToolboxRelease* GetLatestRelease(GWToolboxRelease* release)
    {
        // Get list of releases
        std::string response;
        unsigned int tries = 0;
        const auto url = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases";
        bool success = false;
        do {
            success = Resources::Instance().Download(url, response);
            tries++;
        } while (!success && tries < 5);
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
            if (js.prerelease && release_type == ReleaseType::Stable) {
                continue;
            }
            const auto version_number_len = js.tag_name.find(js.tag_name.contains("_Release") ? "_Release" : "_Beta", 0);
            if (version_number_len == std::string::npos) {
                continue;
            }
            for (const auto& asset : js.assets) {
                if (asset.name != "GWToolbox.dll" && asset.name != "GWToolboxdll.dll") {
                    continue; // This release doesn't have a dll download.
                }
                release->download_url = asset.browser_download_url;
                release->version = js.tag_name.substr(0, version_number_len);
                if (js.prerelease) {
                    release->version += js.tag_name.substr(version_number_len + 1);
                }
                std::ranges::transform(release->version, release->version.begin(), [](const auto chr) { return static_cast<char>(std::tolower(chr)); });
                release->body = js.body;
                const auto size_bytes = static_cast<uintmax_t>(asset.size); // Slight rounding, GitHub isn't always correct down to the byte.
                release->size = static_cast<uintmax_t>(std::ceil(size_bytes / 16.0) * 16);
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
            // Version matches, but file size is different
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s (%.2f kb) is available! You have %s (%.2f kb)",
                               latest_release.version.c_str(), latest_release.size > 0 ? latest_release.size / 1024.f : 0.f,
                               current_release.version.c_str(), current_release.size > 0 ? current_release.size / 1024.f : 0.f);
        }
        else {
            // Version mismatch
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s is available! You have %s", latest_release.version.c_str(), current_release.version.c_str());
        }
        ASSERT(written > 0);
        return update_available_text;
    }

    void DoUpdate()
    {
        Log::Warning("Downloading update...");

        step = Downloading;

        // 0. find toolbox dll path
        const HMODULE module = GWToolbox::GetDLLModule();
        WCHAR dllfile[MAX_PATH];
        const DWORD size = GetModuleFileNameW(module, dllfile, MAX_PATH);
        if (size == 0) {
            Log::Error("Updater error - cannot find GWToolbox.dll path");
            step = Done;
            return;
        }
        Log::Log("dll file name is %s\n", dllfile);

        // Get name of dll from path
        const std::wstring dll_path(dllfile);
        std::wstring dll_name;
        wchar_t sep = '/';
#ifdef _WIN32
        sep = '\\';
#endif

        const size_t i = dll_path.rfind(sep, dll_path.length());
        if (i != std::wstring::npos) {
            dll_name = dll_path.substr(i + 1, dll_path.length() - i);
        }
        if (dll_name.empty()) {
            Log::Error("Updater error - failed to extract dll name from path");
            step = Done;
            return;
        }

        // 1. rename toolbox dll
        const auto dllold = std::wstring(dllfile) + L".old";
        Log::Log("moving to %s\n", dllold.c_str());
        DeleteFileW(dllold.c_str());
        MoveFileW(dllfile, dllold.c_str());

        // 2. download new dll
        Resources::Instance().Download(
            dllfile, latest_release.download_url,
            [wdll = std::wstring(dllfile), dllold](const bool success, const std::wstring& error) -> void {
                if (success) {
                    step = Success;
                    Log::WarningW(L"Update successful, please restart toolbox.");
                }
                else {
                    Log::ErrorW(L"Updated error - cannot download GWToolbox.dll\n%s", error.c_str());
                    MoveFileW(dllold.c_str(), wdll.c_str());
                    step = Done;
                }
            });
    }
}

const std::string& Updater::GetServerVersion()
{
    return latest_release.version;
}

const GWToolboxRelease* Updater::GetCurrentVersionInfo(GWToolboxRelease* out)
{
    // server and client versions match
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(GWToolbox::GetDLLModule(), path, _countof(path)) == 0) {
        return nullptr;
    }
    auto size_bytes = std::filesystem::file_size(path);
    out->size = static_cast<uintmax_t>(std::ceil(size_bytes / 16.0) * 16);
    out->version = GWTOOLBOXDLL_VERSION;
    out->version.append(GWTOOLBOXDLL_VERSION_BETA);
    std::ranges::transform(out->version, out->version.begin(), [](const auto chr) {
        return static_cast<char>(std::tolower(chr));
    });
    return out;
}

void Updater::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
#ifdef _DEBUG
    mode = Mode::DontCheckForUpdates;
    release_type = ReleaseType::Beta;
#else
    mode = static_cast<Mode>(ini->GetLongValue(Name(), "update_mode", static_cast<int>(mode)));
    release_type = static_cast<ReleaseType>(ini->GetLongValue(Name(), "update_release_type", static_cast<int>(release_type)));
#endif
    CheckForUpdate();
}

void Updater::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
#ifdef _DEBUG
#else
    ini->SetLongValue(Name(), "update_mode", static_cast<int>(mode));
    ini->SetLongValue(Name(), "update_release_type", static_cast<int>(release_type));
    ini->SetValue(Name(), "dllversion", GWTOOLBOXDLL_VERSION);

    const HMODULE module = GWToolbox::GetDLLModule();
    CHAR dllfile[MAX_PATH];
    const DWORD size = GetModuleFileName(module, dllfile, MAX_PATH);
    ini->SetValue(Name(), "dllpath", size > 0 ? dllfile : "error");
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
    ImGui::RadioButton("Stable", (int*)&release_type, static_cast<int>(ReleaseType::Stable));
    ImGui::RadioButton("Beta", (int*)&release_type, static_cast<int>(ReleaseType::Beta));
    ImGui::Text("Update mode:");
    ImGui::RadioButton("Do not check for updates", (int*)&mode, static_cast<int>(Mode::DontCheckForUpdates));
    ImGui::RadioButton("Check and display a message", (int*)&mode, static_cast<int>(Mode::CheckAndWarn));
    ImGui::RadioButton("Check and ask before updating", (int*)&mode, static_cast<int>(Mode::CheckAndAsk));
    ImGui::RadioButton("Check and automatically update", (int*)&mode, static_cast<int>(Mode::CheckAndAutoUpdate));
}

void Updater::CheckForUpdate(const bool forced)
{
    if (!GetCurrentVersionInfo(&current_release)) {
        Log::Error("Failed to get current toolbox version info");
    }
    step = Checking;
    last_check = clock();
    Resources::EnqueueWorkerTask([forced] {
        // Here we are in the worker thread and can do blocking operations
        // Reminder: do not send stuff to gw chat from this thread!
        if (!GetLatestRelease(&latest_release)) {
            // Error getting server version. Server down? We can do nothing.
            Log::Flash("Error checking for updates");
            step = Done;
            return;
        }

        if (latest_release.version == current_release.version
            && latest_release.size == current_release.size) {
            // Version and size match
            step = Done;
            is_latest_version = true;
            if (forced) {
                Log::Flash("GWToolbox++ is up-to-date");
            }
            return;
        }
        is_latest_version = false;
        if (!forced && mode == Mode::DontCheckForUpdates) {
            step = Done;
            return; // Do not check for updates
        }

        // we have a new version!
        Mode iMode = forced ? Mode::CheckAndAsk : mode;
        if constexpr (!std::string_view(GWTOOLBOXDLL_VERSION_BETA).empty()) {
            iMode = Mode::CheckAndAsk;
        }
        switch (iMode) {
            case Mode::CheckAndAsk:
                step = CheckAndAsk;
                break;
            case Mode::CheckAndAutoUpdate:
                step = CheckAndAutoUpdate;
                break;
            case Mode::CheckAndWarn:
                step = CheckAndWarn;
                break;
        }
    });
}

bool Updater::IsLatestVersion()
{
    return is_latest_version;
}

void Updater::Draw(IDirect3DDevice9*)
{
    switch (step) {
        case CheckAndWarn:
            Log::Warning(UpdateAvailableText());
            step = Done;
            break;
        case CheckAndAsk: {
            // check and ask
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
    // if step == Done do nothing
}
