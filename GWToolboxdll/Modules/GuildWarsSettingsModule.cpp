#include "stdafx.h"

#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <GWCA/Utilities/Scanner.h>

#include <d3d9on12.h>
#include <Defines.h>
#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include "GuildWarsSettingsModule.h"

#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

// JSON preset DTOs; in a NAMED namespace because glaze reflection can't bind anonymous-namespace types.
namespace guild_wars_settings_json {
    struct WindowPositionJson {
        uint32_t state = 0;
        float p1_x = 0.f;
        float p1_y = 0.f;
        float p2_x = 0.f;
        float p2_y = 0.f;
    };

    struct PreferencesJson {
        std::map<std::string, uint32_t> preference_values{};
        std::map<std::string, uint32_t> preference_enums{};
        std::map<std::string, uint32_t> preference_flags{};
        std::vector<uint32_t> key_mappings{};
        std::vector<WindowPositionJson> window_positions{};
        int32_t gw_window_top = 0;
        int32_t gw_window_left = 0;
        int32_t gw_window_right = 0;
        int32_t gw_window_bottom = 0;
    };
}

namespace {
    uint32_t* key_mappings_array = nullptr;
    uint32_t key_mappings_array_length = 0x75;

    const char* number_pref_names[] = {
        "AutoTournPartySort",
        "ChatState",
        "ChatTab",
        "DistrictLastVisitedLanguage",
        "DistrictLastVisitedLanguage2",
        "DistrictLastVisitedNonInternationalLanguage",
        "DistrictLastVisitedNonInternationalLanguage2",
        "FloaterScale",
        "FullscreenGamma",
        "InventoryBag",
        "Language",
        "LanguageAudio",
        "LastDevice",
        "Refresh",
        "ScreenSizeX",
        "ScreenSizeY",
        "SkillListFilterRarity",
        "SkillListSortMethod",
        "SkillListViewMode",
        "SoundQuality",
        "StorageBagPage",
        "TextureLod",
        "TexFilterMode",
        "VolBackground",
        "VolDialog",
        "VolEffect",
        "VolMusic",
        "VolUi",
        "Vote",
        "WindowPosX",
        "WindowPosY",
        "WindowSizeX",
        "WindowSizeY",
        "SealedSeed",
        "SealedCount",
        "CameraFov",
        "CameraRotationSpeed",
        "ScreenBorderless",
        "VolMaster",
        "ClockMode",
        "MobileUiScale",
        "GamepadCursorSpeed",
        "LastLoginMethod"
    };
    static_assert(_countof(number_pref_names) == std::to_underlying(GW::UI::NumberPreference::Count));

    const char* enum_pref_names[] = {
        "CharSortOrder",
        "MultiSample",
        "Reflections",
        "ShaderQuality",
        "ShadowQuality",
        "TerrainQuality",
        "UiScale",
        "FpsLimit"
    };
    static_assert(_countof(enum_pref_names) == std::to_underlying(GW::UI::EnumPreference::Count));

    const char* flag_pref_names[] = {
        "FlagPref_0x0",
        "FlagPref_0x1",
        "FlagPref_0x2",
        "FlagPref_0x3",
        "ChannelAlliance",
        "FlagPref_0x5",
        "ChannelEmotes",
        "ChannelGuild",
        "ChannelLocal",
        "ChannelGroup",
        "ChannelTrade",
        "FlagPref_0xB",
        "FlagPref_0xC",
        "FlagPref_0xD",
        "FlagPref_0xE",
        "FlagPref_0xF",
        "FlagPref_0x10",
        "ShowTextInSkillFloaters",
        "FlagPref_0x12",
        "FlagPref_0x13",
        "FlagPref_0x14",
        "FlagPref_0x15",
        "InvertMouseControlOfCamera",
        "DisableMouseWalking",
        "FlagPref_0x18",
        "FlagPref_0x19",
        "FlagPref_0x1A",
        "FlagPref_0x1B",
        "FlagPref_0x1C",
        "FlagPref_0x1D",
        "FlagPref_0x1E",
        "FlagPref_0x1F",
        "FlagPref_0x20",
        "FlagPref_0x21",
        "FlagPref_0x22",
        "FlagPref_0x23",
        "FlagPref_0x24",
        "FlagPref_0x25",
        "FlagPref_0x26",
        "FlagPref_0x27",
        "FlagPref_0x28",
        "FlagPref_0x29",
        "FlagPref_0x2A",
        "FlagPref_0x2B",
        "FlagPref_0x2C",
        "RememberAccountName",
        "FlagPref_0x2E",
        "FlagPref_0x2F",
        "FlagPref_0x30",
        "ShowSpendAttributesButton",
        "ConciseSkillDescriptions",
        "DoNotShowSkillTipsOnEffectMonitor",
        "DoNotShowSkillTipsOnSkillBars",
        "FlagPref_0x35",
        "FlagPref_0x36",
        "MuteWhenGuildWarsIsInBackground",
        "FlagPref_0x38",
        "AutoTargetFoes",
        "AutoTargetNPCs",
        "AlwaysShowNearbyNamesPvP",
        "FadeDistantNameTags",
        "FlagPref_0x3D",
        "FlagPref_0x3E",
        "FlagPref_0x3F",
        "FlagPref_0x40",
        "FlagPref_0x41",
        "FlagPref_0x42",
        "FlagPref_0x43",
        "FlagPref_0x44",
        "DoNotCloseWindowsOnEscape",
        "FlagPref_0x46",
        "FlagPref_0x47",
        "FlagPref_0x48",
        "FlagPref_0x49",
        "FlagPref_0x4A",
        "FlagPref_0x4B",
        "FlagPref_0x4C",
        "FlagPref_0x4D",
        "FlagPref_0x4E",
        "FlagPref_0x4F",
        "FlagPref_0x50",
        "FlagPref_0x51",
        "FlagPref_0x52",
        "FlagPref_0x53",
        "WaitForVSync",
        "WhispersFromFriendsEtcOnly",
        "ShowChatTimestamps",
        "ShowCollapsedBags",
        "ItemRarityBorder",
        "AlwaysShowAllyNames",
        "AlwaysShowFoeNames",
        "FlagPref_0x5B",
        "LockCompassRotation",
        "EnableGamepad",
        "FlagPref_0x5E",
        "FlagPref_0x5F",
        "FlagPref_0x60",
        "FlagPref_0x61",
        "FlagPref_0x62",
        "FlagPref_0x63",
        "FlagPref_0x64",
        "FlagPref_0x65",
        "FlagPref_0x66",
        "FlagPref_0x67",
        "FlagPref_0x68",
        "FlagPref_0x69",
        "FlagPref_0x6A",
        "FlagPref_0x6B",
        "FlagPref_0x6c",
        "FlagPref_0x6d",
        "FlagPref_0x6e",
        "FlagPref_0x6f",
        "FlagPref_0x70",
        "FlagPref_0x71"
    };
    static_assert(_countof(flag_pref_names) == std::to_underlying(GW::UI::FlagPreference::Count));

    const char* ini_label_numbers = "Preference Values";
    const char* ini_label_enums = "Preference Enums";
    const char* ini_label_flags = "Preference Flags";
    const char* ini_label_key_mappings = "Key Mappings";
    const char* ini_label_windows = "Window Positions";
    std::map<std::wstring, bool> quest_entry_group_visibility;

    GW::HookEntry ChatCmd_HookEntry;

    struct PreferencesStruct {
        std::vector<uint32_t> preference_values{};
        std::vector<uint32_t> preference_enums{};
        std::vector<uint32_t> preference_flags{};
        std::vector<uint32_t> key_mappings{};
        std::vector<GW::UI::WindowPosition> window_positions{};
        RECT gw_window_pos = { 0 };
    };

    // Read preferences from in-game memory to a PreferencesStruct
    void GetPreferences(PreferencesStruct& out)
    {
        out.preference_values.resize(std::to_underlying(GW::UI::NumberPreference::Count), 0);
        for (auto i = 0u; i < std::to_underlying(GW::UI::NumberPreference::Count); i++) {
            out.preference_values[i] = GetPreference(static_cast<GW::UI::NumberPreference>(i));
        }
        out.preference_enums.resize(std::to_underlying(GW::UI::EnumPreference::Count), 0);
        for (auto i = 0u; i < std::to_underlying(GW::UI::EnumPreference::Count); i++) {
            out.preference_enums[i] = GetPreference(static_cast<GW::UI::EnumPreference>(i));
        }
        out.preference_flags.resize(std::to_underlying(GW::UI::FlagPreference::Count), 0);
        for (auto i = 0u; i < std::to_underlying(GW::UI::FlagPreference::Count); i++) {
            out.preference_flags[i] = GetPreference(static_cast<GW::UI::FlagPreference>(i));
        }
        out.window_positions.resize(GW::UI::WindowID::WindowID_Count, {0});
        for (auto i = 0u; i < std::to_underlying(GW::UI::WindowID::WindowID_Count); i++) {
            out.window_positions[i] = *GetWindowPosition(static_cast<GW::UI::WindowID>(i));
        }
        out.key_mappings.resize(key_mappings_array_length, 0);
        for (auto i = 0u; key_mappings_array && i < key_mappings_array_length; i++) {
            out.key_mappings[i] = key_mappings_array[i];
        }
        ASSERT(GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), &out.gw_window_pos));
    }

    // Write preferences to the game from a PreferencesStruct. Run this on the game thread.
    void SetPreferences(PreferencesStruct& in)
    {
        for (auto i = 0u; i < in.preference_values.size() && i < std::to_underlying(GW::UI::NumberPreference::Count); i++) {
            SetPreference(static_cast<GW::UI::NumberPreference>(i), in.preference_values[i]);
        }
        for (auto i = 0u; i < in.preference_enums.size() && i < std::to_underlying(GW::UI::EnumPreference::Count); i++) {
            SetPreference(static_cast<GW::UI::EnumPreference>(i), in.preference_enums[i]);
        }
        for (auto i = 0u; i < in.preference_flags.size() && i < std::to_underlying(GW::UI::FlagPreference::Count); i++) {
            SetPreference(static_cast<GW::UI::FlagPreference>(i), in.preference_flags[i]);
        }
        for (auto i = 0u; i < in.window_positions.size() && i < std::to_underlying(GW::UI::WindowID::WindowID_Count); i++) {
            SetWindowPosition(static_cast<GW::UI::WindowID>(i), &in.window_positions[i]);
        }
        for (auto i = 0u; i < in.key_mappings.size() && i < key_mappings_array_length; i++) {
            key_mappings_array[i] = in.key_mappings[i];
        }

        if (GW::UI::GetPreference(GW::UI::NumberPreference::ScreenBorderless) == 0) {
            const auto whnd = GW::MemoryMgr::GetGWWindowHandle();
            const auto w = in.gw_window_pos.right - in.gw_window_pos.left;
            const auto h = in.gw_window_pos.bottom - in.gw_window_pos.top;
            if (w > 10 && h > 10) {
                SetWindowPos(whnd, 0, in.gw_window_pos.left, in.gw_window_pos.top, w, h, SWP_ASYNCWINDOWPOS | SWP_NOZORDER);
            }
        }
    }

    // Read preferences from an ini file to a PreferencesStruct
    void LoadPreferences(PreferencesStruct& prefs, const ToolboxIni& ini)
    {
        GetPreferences(prefs); // Use current settings as a default

        for (size_t key = 0; key < prefs.preference_values.size(); key++) {
            prefs.preference_values[key] = ini.GetLongValue(ini_label_numbers, number_pref_names[key], prefs.preference_values[key]);
        }
        for (size_t key = 0; key < prefs.preference_enums.size(); key++) {
            prefs.preference_enums[key] = ini.GetLongValue(ini_label_enums, enum_pref_names[key], prefs.preference_enums[key]);
        }
        for (size_t key = 0; key < prefs.preference_flags.size(); key++) {
            prefs.preference_flags[key] = ini.GetLongValue(ini_label_flags, flag_pref_names[key], prefs.preference_flags[key]);
        }
        char key_buf[16];
        for (size_t key = 0; key < prefs.key_mappings.size(); key++) {
            snprintf(key_buf, _countof(key_buf), "0x%02x", key);
            prefs.key_mappings[key] = ini.GetLongValue(ini_label_key_mappings, key_buf, prefs.key_mappings[key]);
        }
        for (size_t window_id = 0; window_id < prefs.window_positions.size(); window_id++) {
            auto& window_pos = prefs.window_positions[window_id];
            snprintf(key_buf, _countof(key_buf), "0x%02x_state", window_id);
            window_pos.state = ini.GetLongValue(ini_label_windows, key_buf, window_pos.state);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_x", window_id);
            window_pos.p1.x = static_cast<float>(ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p1.x));
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_y", window_id);
            window_pos.p1.y = static_cast<float>(ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p1.y));
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_x", window_id);
            window_pos.p2.x = static_cast<float>(ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p2.x));
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_y", window_id);
            window_pos.p2.y = static_cast<float>(ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p2.y));
        }
        prefs.gw_window_pos.top = ini.GetLongValue(ini_label_windows, "gw_window_top", 0);
        prefs.gw_window_pos.left = ini.GetLongValue(ini_label_windows, "gw_window_left", 0);
        prefs.gw_window_pos.right = ini.GetLongValue(ini_label_windows, "gw_window_right", 0);
        prefs.gw_window_pos.bottom = ini.GetLongValue(ini_label_windows, "gw_window_bottom", 0);
    }

    // Read preferences from a JSON preset to a PreferencesStruct
    void LoadPreferences(PreferencesStruct& prefs, const guild_wars_settings_json::PreferencesJson& json)
    {
        GetPreferences(prefs); // Use current settings as a default

        const auto get_or = [](const std::map<std::string, uint32_t>& values, const char* name, const uint32_t current) {
            const auto found = values.find(name);
            return found != values.end() ? found->second : current;
        };
        for (size_t key = 0; key < prefs.preference_values.size(); key++) {
            prefs.preference_values[key] = get_or(json.preference_values, number_pref_names[key], prefs.preference_values[key]);
        }
        for (size_t key = 0; key < prefs.preference_enums.size(); key++) {
            prefs.preference_enums[key] = get_or(json.preference_enums, enum_pref_names[key], prefs.preference_enums[key]);
        }
        for (size_t key = 0; key < prefs.preference_flags.size(); key++) {
            prefs.preference_flags[key] = get_or(json.preference_flags, flag_pref_names[key], prefs.preference_flags[key]);
        }
        for (size_t key = 0; key < prefs.key_mappings.size() && key < json.key_mappings.size(); key++) {
            prefs.key_mappings[key] = json.key_mappings[key];
        }
        for (size_t window_id = 0; window_id < prefs.window_positions.size() && window_id < json.window_positions.size(); window_id++) {
            const auto& in = json.window_positions[window_id];
            auto& out = prefs.window_positions[window_id];
            out.state = in.state;
            out.p1 = {in.p1_x, in.p1_y};
            out.p2 = {in.p2_x, in.p2_y};
        }
        prefs.gw_window_pos = {json.gw_window_left, json.gw_window_top, json.gw_window_right, json.gw_window_bottom};
    }

    // Write preferences from a PreferencesStruct to a JSON preset
    void SavePreferences(const PreferencesStruct& prefs, guild_wars_settings_json::PreferencesJson& json)
    {
        for (size_t key = 0; key < prefs.preference_values.size(); key++) {
            json.preference_values[number_pref_names[key]] = prefs.preference_values[key];
        }
        for (size_t key = 0; key < prefs.preference_enums.size(); key++) {
            json.preference_enums[enum_pref_names[key]] = prefs.preference_enums[key];
        }
        for (size_t key = 0; key < prefs.preference_flags.size(); key++) {
            json.preference_flags[flag_pref_names[key]] = prefs.preference_flags[key];
        }
        json.key_mappings = prefs.key_mappings;
        json.window_positions.resize(prefs.window_positions.size());
        for (size_t window_id = 0; window_id < prefs.window_positions.size(); window_id++) {
            const auto& in = prefs.window_positions[window_id];
            auto& out = json.window_positions[window_id];
            out.state = in.state;
            out.p1_x = in.p1.x;
            out.p1_y = in.p1.y;
            out.p2_x = in.p2.x;
            out.p2_y = in.p2.y;
        }
        json.gw_window_top = prefs.gw_window_pos.top;
        json.gw_window_left = prefs.gw_window_pos.left;
        json.gw_window_right = prefs.gw_window_pos.right;
        json.gw_window_bottom = prefs.gw_window_pos.bottom;
    }

    void OnPreferencesLoadFileChosen(const char* result)
    {
        if (!result) {
            return;
        }
        GW::GameThread::Enqueue([filename_cpy = std::filesystem::path(result)] {
            PreferencesStruct prefs;
            if (!exists(filename_cpy)) {
                Log::Error("File name %s doesn't exist", filename_cpy.string().c_str());
                return;
            }

            if (filename_cpy.extension() == L".ini") {
                // Legacy preset format; still readable, but presets are only ever written as .json now
                ToolboxIni ini;
                const auto err = ini.LoadFile(filename_cpy.string().c_str());
                if (err != SI_OK) {
                    Log::Error("Failed to load ini file %s - error code %d", filename_cpy.string().c_str(), err);
                    return;
                }
                LoadPreferences(prefs, ini);
            }
            else {
                std::ifstream file(filename_cpy, std::ios::binary);
                const std::string buffer{std::istreambuf_iterator(file), {}};
                guild_wars_settings_json::PreferencesJson json;
                if (!file || glz::read<glz::opts{.error_on_unknown_keys = false}>(json, buffer)) {
                    Log::Error("Failed to load json file %s", filename_cpy.string().c_str());
                    return;
                }
                LoadPreferences(prefs, json);
            }
            SetPreferences(prefs);

            Log::Info("Preferences loaded from %s", filename_cpy.filename().string().c_str());
        });
    }

    std::filesystem::path GetDefaultFilename()
    {
        const auto uuid = GW::AccountMgr::GetAccountUuid();
        return std::filesystem::path(TextUtils::StringToWString(TextUtils::GuidToString(&uuid)) + L"_GuildWarsSettings.json");
    }

    // Prefer the .json preset; fall back to a legacy .ini preset with the same basename
    std::filesystem::path ResolveExistingPreset(std::filesystem::path filename)
    {
        if (filename.extension() == L".json" && !std::filesystem::exists(filename)) {
            auto legacy = filename;
            legacy.replace_extension(L".ini");
            if (std::filesystem::exists(legacy)) {
                return legacy;
            }
        }
        return filename;
    }

    void OnPreferencesSaveFileChosen(const char* result)
    {
        if (!result) {
            return;
        }
        auto filename = std::filesystem::path(result);
        // Presets are only ever written as .json
        if (filename.extension() != L".json") {
            filename.replace_extension(L".json");
        }
        GW::GameThread::Enqueue([filename_cpy = std::move(filename)] {
            PreferencesStruct current_prefs;
            GetPreferences(current_prefs);
            guild_wars_settings_json::PreferencesJson json;
            SavePreferences(current_prefs, json);
            std::string buffer;
            if (glz::write<glz::opts{.prettify = true}>(json, buffer)) {
                Log::Error("Failed to serialise preferences for %s", filename_cpy.string().c_str());
                return;
            }
            std::ofstream file(filename_cpy, std::ios::binary | std::ios::trunc);
            file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            if (file.good()) {
                Log::Info("Preferences saved to %s", filename_cpy.filename().string().c_str());
            }
            else {
                Log::Error("Failed to save json file %s", filename_cpy.string().c_str());
            }
        });
    }

    void CHAT_CMD_FUNC(CmdSave)
    {
        std::filesystem::path filename = GetDefaultFilename();
        if (argc > 1) {
            filename = argv[1];
        }
        if (filename.extension() != L".json") {
            filename.replace_extension(L".json");
        }
        filename = TextUtils::SanitiseFilename(filename.string());
        filename = Resources::GetPath(filename);

        OnPreferencesSaveFileChosen(filename.string().c_str());
    }

    void CHAT_CMD_FUNC(CmdLoad)
    {
        std::filesystem::path filename = GetDefaultFilename();
        if (argc > 1) {
            filename = argv[1];
        }
        if (filename.extension() != L".json" && filename.extension() != L".ini") {
            filename.replace_extension(L".json");
        }
        filename = TextUtils::SanitiseFilename(filename.string());
        filename = ResolveExistingPreset(Resources::GetPath(filename));

        OnPreferencesLoadFileChosen(filename.string().c_str());
    }

    const char* section_name = "GuildWarsSettingsModule:Quest Log Entries Visible";

    bool DoesDeviceSupportInterface(IDirect3DDevice9* d3d9Device, const GUID interface_iid) {
        if (!d3d9Device)
            return false;
        IUnknown* dxvkInterface = nullptr;

        // Use the locally defined GUID
        HRESULT hr = d3d9Device->QueryInterface(interface_iid, (void**)&dxvkInterface);

        if (SUCCEEDED(hr)) {
            dxvkInterface->Release();  // Release the DXVK-specific interface.
            return true;
        }

        // The device does not support the DXVK-specific interface.
        return false;
    }

    bool GetModuleFileInfo(HMODULE hModule, std::string& productName, std::string& productVersion, std::string& baseName) {
        productName.clear();
        productVersion.clear();
        baseName.clear();
        if (hModule == NULL) {
            return false;
        }

        char dllPath[MAX_PATH];
        if (GetModuleFileNameA(hModule, dllPath, MAX_PATH) == 0) {
            return false;
        }

        // Extract the base name using std::filesystem::path directly
        baseName = std::filesystem::path(dllPath).filename().string();

        DWORD dummy;
        DWORD versionSize = GetFileVersionInfoSizeA(dllPath, &dummy);

        if (versionSize == 0) {
            return false;
        }

        LPVOID versionInfo = malloc(versionSize);

        if (!GetFileVersionInfoA(dllPath, 0, versionSize, versionInfo)) {
            free(versionInfo);
            return false;
        }

        UINT langCodepageSize;
        LPVOID langCodepageInfo;
        if (VerQueryValueA(versionInfo, "\\VarFileInfo\\Translation", &langCodepageInfo, &langCodepageSize)) {
            DWORD* langCodepage = static_cast<DWORD*>(langCodepageInfo);

            // Get Product Name
            char nameQueryPath[256];
            snprintf(nameQueryPath, sizeof(nameQueryPath) / sizeof(nameQueryPath[0]), "\\StringFileInfo\\%04X%04X\\ProductName", LOWORD(*langCodepage), HIWORD(*langCodepage));

            LPVOID productNameValue;
            UINT productNameValueSize;

            if (VerQueryValueA(versionInfo, nameQueryPath, &productNameValue, &productNameValueSize)) {
                productName.assign(static_cast<const char*>(productNameValue), productNameValueSize - 1);  // Exclude the null terminator
            }

            // Get Product Version
            char versionQueryPath[256];
            snprintf(versionQueryPath, sizeof(versionQueryPath) / sizeof(versionQueryPath[0]), "\\StringFileInfo\\%04X%04X\\ProductVersion", LOWORD(*langCodepage), HIWORD(*langCodepage));

            LPVOID productVersionValue;
            UINT productVersionValueSize;

            if (VerQueryValueA(versionInfo, versionQueryPath, &productVersionValue, &productVersionValueSize)) {
                productVersion.assign(static_cast<const char*>(productVersionValue), productVersionValueSize - 1);  // Exclude the null terminator
            }
        }

        free(versionInfo);
        return !productName.empty() && !productVersion.empty();
    }

    const wchar_t* GetGraphicsAPIName() {
        const auto device = GW::Render::GetDevice();

        // IID_IDirect3DDevice9On12
        const bool d3d9on12_support = DoesDeviceSupportInterface(device, {0xe7fda234, 0xb589, 0x4049, {0x94, 0x0d, 0x88, 0x78, 0x97, 0x75, 0x31, 0xc8}});
        if (d3d9on12_support) {
            return L"D3D9On12";
        }
        // IID_DXVKInterface
        const bool dxvk_support = DoesDeviceSupportInterface(device, {0x2eaa4b89, 0x0107, 0x4bdb, {0x87, 0xf7, 0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0}});
        if (dxvk_support) {
            return L"DXVK";
        }
        return L"DirectX 9";
    }

    uint32_t OnCreateUIComponent_GraphicsVersion( int ,int ,int ,void *,wchar_t *, wchar_t *) {
        std::string dll_product_name;
        std::string dll_product_version;
        std::string dll_base_name;

        std::wstring new_name_enc;
        new_name_enc += L"\x108\x107Renderer: ";
        new_name_enc += GetGraphicsAPIName();
        new_name_enc += L"\x1\x2\x102\x2\x108\x107";

        GetModuleFileInfo(GetModuleHandle("d3d9.dll"), dll_product_name, dll_product_version, dll_base_name);

        if (dll_product_name.size()) {
            new_name_enc += L" (";
            new_name_enc += TextUtils::StringToWString(dll_base_name);
            new_name_enc += L", ";
            new_name_enc += TextUtils::StringToWString(dll_product_name);
            if (dll_product_version.size()) {
                new_name_enc += L", ";
                new_name_enc += TextUtils::StringToWString(dll_product_version);
            }
            new_name_enc += L")";
        }

        GetModuleFileInfo(GetModuleHandle("dxgi.dll"), dll_product_name, dll_product_version, dll_base_name);
        if (dll_product_name.size()) {
            new_name_enc += L" (";
            new_name_enc += TextUtils::StringToWString(dll_base_name);
            new_name_enc += L", ";
            new_name_enc += TextUtils::StringToWString(dll_product_name);
            if (dll_product_version.size()) {
                new_name_enc += L", ";
                new_name_enc += TextUtils::StringToWString(dll_product_version);
            }
            new_name_enc += L")";
        }
        new_name_enc += L"\x1";

        return 0;
        //CreateUIComponent(frame_id, behavior, child_frame_id, ui_callback, new_name_enc.data(), label);
    }
}

void GuildWarsSettingsModule::Initialize()
{
    ToolboxModule::Initialize();
    // NB: This address is fond twice, we only care about the first.
    uintptr_t address = GW::Scanner::FindAssertion("FrKey.cpp", "count == arrsize(s_remapTable)", 0, 0x13);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address)) {
        key_mappings_array = *(uint32_t**)address;
    }
#ifdef _DEBUG
    ASSERT(key_mappings_array);
#endif

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"saveprefs", CmdSave);

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"loadprefs", CmdLoad);
}

void GuildWarsSettingsModule::Terminate()
{
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void GuildWarsSettingsModule::DrawSettingsInternal()
{
    ImGui::TextUnformatted("Choose a file from your computer to load Guild Wars settings");
    if (ImGui::Button("Load from disk...")) {
        const auto filename = ResolveExistingPreset(Resources::GetPath(GetDefaultFilename()));
        Resources::OpenFileDialog(OnPreferencesLoadFileChosen, "json,ini", filename.string().c_str());
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Choose a file from your computer to save Guild Wars settings");
    if (ImGui::Button("Save to disk...")) {
        const auto filename = Resources::GetPath(GetDefaultFilename());
        Resources::SaveFileDialog(OnPreferencesSaveFileChosen, "json", filename.string().c_str());
    }
}
