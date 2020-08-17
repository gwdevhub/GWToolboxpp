#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
#include <GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Modules/Updater.h>

void Updater::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
#ifdef _DEBUG
    mode = 0;
#else
    mode = ini->GetLongValue(Name(), "update_mode", mode);
#endif
    CheckForUpdate();
}

void Updater::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
#ifdef _DEBUG
    return;
#else
    ini->SetLongValue(Name(), "update_mode", mode);
    ini->SetValue(Name(), "dllversion", GWTOOLBOXDLL_VERSION);

    HMODULE module = GWToolbox::GetDLLModule();
    CHAR dllfile[MAX_PATH];
    DWORD size = GetModuleFileName(module, dllfile, MAX_PATH);
    ini->SetValue(Name(), "dllpath", size > 0 ? dllfile : "error");
#endif
}

void Updater::Initialize() {
    ToolboxUIElement::Initialize();

    CheckForUpdate();
}

void Updater::DrawSettingInternal() {
    ImGui::Text("Update mode:");
    const float btnWidth = 180.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - btnWidth);
    if (ImGui::Button(step == Checking ? "Checking..." : "Check for updates",ImVec2(btnWidth,0)) && step != Checking) {
        CheckForUpdate(true);
    }
    ImGui::RadioButton("Do not check for updates", &mode, 0);
    ImGui::RadioButton("Check and display a message", &mode, 1);
    ImGui::RadioButton("Check and ask before updating", &mode, 2);
    ImGui::RadioButton("Check and automatically update", &mode, 3);
}

void Updater::GetLatestRelease(GWToolboxRelease* release) {
    // Get list of releases
    std::string releases_str = "";
    unsigned int tries = 0;
    while (tries < 5 && releases_str.empty()) {
        releases_str = Resources::Instance().Download(L"https://api.github.com/repos/HasKha/GWToolboxpp/releases");
        tries++;
    }
    if (releases_str.empty()) {
        return;
    }
    using Json = nlohmann::json;
    Json json = Json::parse(releases_str.c_str(), nullptr, false);
    if (json == Json::value_t::discarded || !json.is_array() || !json.size())
        return;
    for (unsigned int i = 0; i < json.size(); i++) {
        if (!(json[i].contains("tag_name") && json[i]["tag_name"].is_string()))
            continue;
        std::string tag_name = json[i]["tag_name"].get<std::string>();
        size_t version_number_len = tag_name.find("_Release", 0);
        if (version_number_len == std::string::npos)
            continue;
        if (!(json[i].contains("assets") && json[i]["assets"].is_array() && json[i]["assets"].size() > 0))
            continue;
        if (!(json[i].contains("body") && json[i]["body"].is_string()))
            continue;
        for (unsigned int j = 0; j < json[i]["assets"].size(); j++) {
            const Json& asset = json[i]["assets"][j];
            if (!(asset.contains("name") && asset["name"].is_string())
                || !(asset.contains("browser_download_url") && asset["browser_download_url"].is_string()))
                continue;
            std::string asset_name = asset["name"].get<std::string>();
            if (asset_name != "GWToolbox.dll" && asset_name != "GWToolboxdll.dll")
                continue; // This release doesn't have a dll download.
            release->download_url = asset["browser_download_url"].get<std::string>();
            release->version = tag_name.substr(0, version_number_len);
            release->body = json[i]["body"].get<std::string>();
            return;
        }
    }
}

void Updater::CheckForUpdate(const bool forced) {
    step = Checking;
    last_check = clock();
    if (!forced && mode == 0) {
        step = Done;
        return;
    }

    Resources::Instance().EnqueueWorkerTask([this,forced]() {
        // Here we are in the worker thread and can do blocking operations
        // Reminder: do not send stuff to gw chat from this thread!
        GWToolboxRelease release;
        GetLatestRelease(&release);
        if (release.version.empty()) {
            // Error getting server version. Server down? We can do nothing.
            Log::Info("Error checking for updates");
            step = Done;
            return;
        }
        if (release.version.compare(GWTOOLBOXDLL_VERSION) == 0) {
            // server and client versions match
            step = Done;
            if (forced) {
                Log::Info("GWToolbox++ is up-to-date");
            }
            return;
        }
        // we have a new version!
        if (latest_release.version.compare(release.version) != 0 || forced) {
            notified = false;
            visible = true;
        }
        latest_release = release;
        forced_ask = forced;
        step = Asking;
    });
}

void Updater::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (step == Asking && !latest_release.version.empty()) {
        
        if (!notified) {
            notified = true;
            Log::Warning("GWToolbox++ version %s is available! You have %s%s.",
                latest_release.version.c_str(), GWTOOLBOXDLL_VERSION, GWTOOLBOXDLL_VERSION_BETA);
        }

        int iMode = forced_ask ? 2 : mode;
        
        switch (iMode) {
        case 0: // no updating
            step = Done;
            break;

        case 1: // check and warn

            step = Done;
            break;

        case 2: { // check and ask
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing, ImVec2(.5f, .5f));
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::Text("GWToolbox++ version %s is available! You have %s%s",
                latest_release.version.c_str(), GWTOOLBOXDLL_VERSION, GWTOOLBOXDLL_VERSION_BETA);
            ImGui::Text("Changes:");
            ImGui::Text(latest_release.body.c_str());

            ImGui::Text("");
            ImGui::Text("Do you want to update?");
            if (ImGui::Button("Later", ImVec2(100, 0))) {
                step = Done;
            }
            ImGui::SameLine();
            if (ImGui::Button("OK", ImVec2(100, 0))) {
                DoUpdate();
            }
            ImGui::End();
            if (!visible) {
                step = Done;
            }
            break;
        }

        case 3: // check and do
            DoUpdate();
            break;

        default:
            break;
        }

    } else if (step == Downloading) {
        if (visible) {
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing, ImVec2(.5f, .5f));
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::Text("GWToolbox++ version %s is available! You have %s",
                latest_release.version.c_str(), GWTOOLBOXDLL_VERSION);
            ImGui::Text("Changes:");
            ImGui::Text(latest_release.body.c_str());

            ImGui::Text("");
            ImGui::Text("Downloading update...");
            if (ImGui::Button("Hide", ImVec2(100, 0))) {
                visible = false;
            }
            ImGui::End();
        }
    } else if (step == Success) {
        if (visible) {
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing, ImVec2(.5f, .5f));
            ImGui::Begin("Toolbox Update!", &visible);
            ImGui::Text("GWToolbox++ version %s is available! You have %s",
                latest_release.version.c_str(), GWTOOLBOXDLL_VERSION);
            ImGui::Text("Changes:");
            ImGui::Text(latest_release.body.c_str());

            ImGui::Text("");
            ImGui::Text("Update successful, please restart toolbox.");
            if (ImGui::Button("OK", ImVec2(100, 0))) {
                visible = false;
            }
            ImGui::End();
        }
    }
    else {
        forced_ask = false;
    }
    // if step == Done do nothing
}

void Updater::DoUpdate() {
    Log::Warning("Downloading update...");

    step = Downloading;

    // 0. find toolbox dll path
    HMODULE module = GWToolbox::GetDLLModule();
    WCHAR dllfile[MAX_PATH];
    DWORD size = GetModuleFileNameW(module, dllfile, MAX_PATH);
    if (size == 0) {
        Log::Error("Updater error - cannot find GWToolbox.dll path");
        step = Done;
        return;
    }
    Log::Log("dll file name is %s\n", dllfile);

    // Get name of dll from path
    std::wstring dll_path(dllfile);
    std::wstring dll_name;
    wchar_t sep = '/';
    #ifdef _WIN32
        sep = '\\';
    #endif

    size_t i = dll_path.rfind(sep, dll_path.length());
    if (i != std::wstring::npos) {
        dll_name = dll_path.substr(i + 1, dll_path.length() - i);
    }
    if (dll_name.empty()) {
        Log::Error("Updater error - failed to extract dll name from path");
        step = Done;
        return;
    }


    // 1. rename toolbox dll
    WCHAR dllold[MAX_PATH];
    wcsncpy(dllold, dllfile, MAX_PATH);
    wcsncat(dllold, L".old", MAX_PATH);
    Log::Log("moving to %s\n", dllold);
    DeleteFileW(dllold);
    MoveFileW(dllfile, dllold);

    // @Fix: Visual Studio 2015 doesn't seem to accept to capture c-style arrays
    std::wstring wdllfile(dllfile);
    std::wstring wdllold(dllold);

    // 2. download new dll
    Resources::Instance().Download(
        dllfile, GuiUtils::StringToWString(latest_release.download_url),
        [this, wdllfile, wdllold](bool success) -> void {
        if (success) {
            step = Success;
            Log::Warning("Update successful, please restart toolbox.");
        } else {
            Log::Error("Updated error - cannot download GWToolbox.dll");
            MoveFileW(wdllold.c_str(), wdllfile.c_str());
            step = Done;
        }
    });
}
