#include "stdafx.h"

#include "SteamModule.h"

#pragma warning(disable : 4828)
#include <steamworks_sdk/public/steam/steam_api.h>
#include "Resources.h"

#include <GWCA/Managers/RenderMgr.h>

#include <ImGuiAddons.h>

#define STEAM_APP_ID 29720

namespace {
    bool steam_initialised_before_dx9 = false;
    bool steam_api_loaded = false;
    HMODULE steam_api_module = 0;

    ImVec4 green = ImVec4(0, 1, 0, 1);
    ImVec4 red = ImVec4(1, 0, 0, 1);
    ImVec4 yellow = ImVec4(1, 1, 0, 1);

    SteamErrMsg steam_connection_error;

    std::string last_error;

    bool EnsureSteamAppIdFile()
    {
        const auto steam_appid_path = Resources::GetExePath().parent_path() / "steam_appid.txt";
        if (std::filesystem::exists(steam_appid_path)) return true;
        std::ofstream file(steam_appid_path);
        if (!file.is_open()) return false;
        file << STEAM_APP_ID;
        return true;
    }
    bool DeleteSteamAppIdFile() {
        const auto steam_appid_path = Resources::GetExePath().parent_path() / "steam_appid.txt";
        if (std::filesystem::exists(steam_appid_path)) {
            std::filesystem::remove(steam_appid_path);
        }
        return !std::filesystem::exists(steam_appid_path);
    }

    HMODULE steam_api_dll = 0;

    bool InitializeSteam()
    {
        if (steam_api_loaded) return true;
        EnsureSteamAppIdFile();
        steam_api_module = LoadLibraryA("steam_api.dll");
        if (!steam_api_module) {
            DeleteSteamAppIdFile();
            Log::Error("Failed to load steam_api.dll");
            return false;
        }
        *steam_connection_error = 0;
        if (SteamAPI_InitFlat(&steam_connection_error) != k_ESteamAPIInitResult_OK) {
            last_error = std::format("Failed to init Steam. {}", steam_connection_error);
            DeleteSteamAppIdFile();
            return false;
        }
        DeleteSteamAppIdFile();
        steam_api_loaded = true;
        last_error = "";
        return true;
    }

} // namespace

void SteamModule::Initialize()
{
    ToolboxModule::Initialize();
    InitializeSteam();
}

void SteamModule::DrawSettingsInternal()
{
    ToolboxModule::DrawSettingsInternal();
    ImGui::TextUnformatted("Status: ");
    if (*steam_connection_error) {
        ImGui::TextColored(red, "Failed to init Steam: %s", steam_connection_error);
    }
    else if (!steam_api_loaded) {
        ImGui::TextColored(yellow, "Not Connected");
    }
    else if (!SteamUtils()->IsOverlayEnabled()) {
        ImGui::TextColored(yellow, "Steam connected, but overlay not working");
        ImGui::ShowHelp("Steam connection was intialised after DirectX device was created.\nThis means that steam isn't able to hook into the game\nto be able to draw the steam overlay.\n\nLaunch toolbox through gwlauncher to make it work!");
    }
    else {
        ImGui::TextColored(red, "Steam connected");
    }
    if (!steam_api_loaded && ImGui::Button("Retry")) {
        InitializeSteam();
    }
    if (ImGui::Button("Disconnect Steam") && steam_api_loaded) {
        SteamAPI_Shutdown();
        steam_api_loaded = false;
    }
}
