#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Context/CharContext.h>

#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>

#include "GuildWarsSettingsModule.h"
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/GameContainers/List.h>
#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Packets/Opcodes.h>

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
        "Territory",
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
        "ClockMode"
    };
    static_assert(_countof(number_pref_names) == (uint32_t)GW::UI::NumberPreference::Count);

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
    static_assert(_countof(enum_pref_names) == (uint32_t)GW::UI::EnumPreference::Count);

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
        "LockCompassRotation"
    };
    static_assert(_countof(flag_pref_names) == (uint32_t)GW::UI::FlagPreference::Count);

    const char* ini_label_numbers = "Preference Values";
    const char* ini_label_enums = "Preference Enums";
    const char* ini_label_flags = "Preference Flags";
    const char* ini_label_key_mappings = "Key Mappings";
    const char* ini_label_windows = "Window Positions";
    std::map<std::wstring, bool> quest_entry_group_visibility;
    
    struct QuestEntryGroup {
        void* vtable;
        wchar_t* encoded_name;
        uint32_t props[0x5];
        bool is_visible;
    };
    static_assert(sizeof(QuestEntryGroup) == 0x20);

    struct QuestEntryGroupContext {
        GW::TList<QuestEntryGroup> quest_entries;
    };
    QuestEntryGroupContext* quest_entry_group_context = nullptr;

    QuestEntryGroup* GetQuestEntryGroup(const wchar_t* quest_entry_encoded_name) {
        if (!(quest_entry_encoded_name && *quest_entry_encoded_name && quest_entry_group_context))
            return nullptr;
        auto& quest_entries = quest_entry_group_context->quest_entries;
        const auto found = std::ranges::find_if(quest_entries, [quest_entry_encoded_name](auto& entry) {
            return wcscmp(entry.encoded_name, quest_entry_encoded_name) == 0;
        });
        return found == std::ranges::end(quest_entries) ? nullptr : &*found;
    }
    QuestEntryGroup* GetQuestEntryGroup(GW::Constants::QuestID quest_id) {
        if (quest_id == static_cast<GW::Constants::QuestID>(0))
            return nullptr;
        wchar_t out[128];
        return GW::QuestMgr::GetQuestEntryGroupName(quest_id, out, _countof(out)) ? GetQuestEntryGroup(out) : nullptr;
    }


    struct PreferencesStruct {
        std::vector<uint32_t> preference_values;
        std::vector<uint32_t> preference_enums;
        std::vector<uint32_t> preference_flags;
        std::vector<uint32_t> key_mappings;
        std::vector<GW::UI::WindowPosition> window_positions;
    };
    // Read preferences from in-game memory to a PreferencesStruct
    void GetPreferences(PreferencesStruct& out) {
        out.preference_values.resize((uint32_t)GW::UI::NumberPreference::Count,0);
        for (uint32_t i = 0; i < (uint32_t)GW::UI::NumberPreference::Count; i++) {
            out.preference_values[i] = GW::UI::GetPreference((GW::UI::NumberPreference)i);
        }
        out.preference_enums.resize((uint32_t)GW::UI::EnumPreference::Count,0);
        for (uint32_t i = 0; i < (uint32_t)GW::UI::EnumPreference::Count; i++) {
            out.preference_enums[i] = GW::UI::GetPreference((GW::UI::EnumPreference)i);
        }
        out.preference_flags.resize((uint32_t)GW::UI::FlagPreference::Count,0);
        for (uint32_t i = 0; i < (uint32_t)GW::UI::FlagPreference::Count; i++) {
            out.preference_flags[i] = GW::UI::GetPreference((GW::UI::FlagPreference)i);
        }
        out.window_positions.resize((uint32_t)GW::UI::WindowID::WindowID_Count, { 0 });
        for (uint32_t i = 0; i < (uint32_t)GW::UI::WindowID::WindowID_Count; i++) {
            out.window_positions[i] = *GW::UI::GetWindowPosition((GW::UI::WindowID)i);
        }
        out.key_mappings.resize(key_mappings_array_length, 0);
        for (uint32_t i = 0; key_mappings_array && i < key_mappings_array_length; i++) {
            out.key_mappings[i] = key_mappings_array[i];
        }
    }
    // Write preferences to the game from a PreferencesStruct. Run this on the game thread.
    void SetPreferences(PreferencesStruct& in) {
        for (uint32_t i = 0; i < in.preference_values.size() && i < (uint32_t)GW::UI::NumberPreference::Count; i++) {
            GW::UI::SetPreference((GW::UI::NumberPreference)i,in.preference_values[i]);
        }
        for (uint32_t i = 0; i < in.preference_enums.size() && i < (uint32_t)GW::UI::EnumPreference::Count; i++) {
            GW::UI::SetPreference((GW::UI::EnumPreference)i,in.preference_enums[i]);
        }
        for (uint32_t i = 0; i < in.preference_flags.size() && i < (uint32_t)GW::UI::FlagPreference::Count; i++) {
            GW::UI::SetPreference((GW::UI::FlagPreference)i,in.preference_flags[i]);
        }
        for (uint32_t i = 0; i < in.window_positions.size() && i < (uint32_t)GW::UI::WindowID::WindowID_Count; i++) {
            GW::UI::SetWindowPosition((GW::UI::WindowID)i, &in.window_positions[i]);
        }
        for (uint32_t i = 0; i < in.key_mappings.size() && i < key_mappings_array_length; i++) {
            key_mappings_array[i] = in.key_mappings[i];
        }
    }

    // Read preferences from an ini file to a PreferencesStruct
    void LoadPreferences(PreferencesStruct& prefs, ToolboxIni& ini) {
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
        for (size_t window_id = 0; window_id < prefs.window_positions.size();window_id++) {
            auto& window_pos = prefs.window_positions[window_id];
            snprintf(key_buf, _countof(key_buf), "0x%02x_state", window_id);
            window_pos.state = ini.GetLongValue(ini_label_windows, key_buf, window_pos.state);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_x", window_id);
            window_pos.p1.x = (float)ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p1.x);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_y", window_id);
            window_pos.p1.y = (float)ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p1.y);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_x", window_id);
            window_pos.p2.x = (float)ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p2.x);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_y", window_id);
            window_pos.p2.y = (float)ini.GetDoubleValue(ini_label_windows, key_buf, window_pos.p2.y);
        }

    }
    // Write preferences to an ini file from a PreferencesStruct
    void SavePreferences(PreferencesStruct& prefs, ToolboxIni& ini) {
        std::string tmp;

        char key_buf[16];
        for (size_t key = 0; key < prefs.preference_values.size(); key++) {
            ini.SetLongValue(ini_label_numbers, number_pref_names[key], prefs.preference_values[key]);
        }
        for (size_t key = 0; key < prefs.preference_enums.size(); key++) {
            ini.SetLongValue(ini_label_enums, enum_pref_names[key], prefs.preference_enums[key]);
        }
        for (size_t key = 0; key < prefs.preference_flags.size(); key++) {
            ini.SetLongValue(ini_label_flags, flag_pref_names[key], prefs.preference_flags[key]);
        }
        for (size_t key = 0; key < prefs.key_mappings.size(); key++) {
            snprintf(key_buf, _countof(key_buf), "0x%02x", key);
            ini.SetLongValue(ini_label_key_mappings, key_buf, prefs.key_mappings[key]);
        }

        for (size_t window_id = 0; window_id < prefs.window_positions.size();window_id++) {
            auto& window_pos = prefs.window_positions[window_id];
            snprintf(key_buf, _countof(key_buf), "0x%02x_state", window_id);
            ini.SetLongValue(ini_label_windows, key_buf, window_pos.state);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_x", window_id);
            ini.SetDoubleValue(ini_label_windows, key_buf, window_pos.p1.x);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p1_y", window_id);
            ini.SetDoubleValue(ini_label_windows, key_buf, window_pos.p1.y);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_x", window_id);
            ini.SetDoubleValue(ini_label_windows, key_buf, window_pos.p2.x);
            snprintf(key_buf, _countof(key_buf), "0x%02x_p2_y", window_id);
            ini.SetDoubleValue(ini_label_windows, key_buf, window_pos.p2.y);
        }
    }

    void OnPreferencesLoadFileChosen(const char* result) {
        if (!result)
            return;
        std::filesystem::path* filename_cpy = new std::filesystem::path(result);
        GW::GameThread::Enqueue([filename_cpy]() {
            int err = SI_OK;
            PreferencesStruct prefs;
            ToolboxIni ini;
            if (!std::filesystem::exists(*filename_cpy)) {
                Log::Error("File name %s doesn't exist",filename_cpy->string().c_str());
                goto cleanup;
            }

            err = ini.LoadFile(filename_cpy->string().c_str());
            if (err != SI_OK) {
                Log::Error("Failed to load ini file %s - error code %d",filename_cpy->string().c_str(),err);
                goto cleanup;
            }

            LoadPreferences(prefs, ini);
            SetPreferences(prefs);

            Log::Info("Preferences loaded from %s", filename_cpy->filename().string().c_str());

            cleanup:
            delete filename_cpy;
            });
    }

    std::filesystem::path GetDefaultFilename() {
        return std::format(L"{}_GuildWarsSettings.ini", GW::GetCharContext()->player_email);
    }

    void OnPreferencesSaveFileChosen(const char* result) {
        if (!result)
            return;
        std::filesystem::path* filename_cpy = new std::filesystem::path(result);
        GW::GameThread::Enqueue([filename_cpy]() {
            PreferencesStruct current_prefs;
            GetPreferences(current_prefs);
            ToolboxIni ini;
            SavePreferences(current_prefs, ini);
            auto err = ini.SaveFile(filename_cpy->string().c_str());
            if (err == SI_OK) {
                Log::Info("Preferences saved to %s", filename_cpy->filename().string().c_str());
            }
            else {
                Log::Error("Failed to save ini file %s - error code %d",filename_cpy->string().c_str(),err);
            }
            delete filename_cpy;
            });
    }
    void CmdSave(const wchar_t*, int argc, LPWSTR* argv) {
        std::filesystem::path filename = GetDefaultFilename();
        if (argc > 1)
            filename = argv[1];
        if (filename.extension() != L".ini")
            filename.append(L".ini");
        filename = GuiUtils::SanitiseFilename(filename.string());
        filename = Resources::GetPath(filename);

        OnPreferencesSaveFileChosen(filename.string().c_str());
    }

    void CmdLoad(const wchar_t*, int argc, LPWSTR* argv) {
        std::filesystem::path filename = GetDefaultFilename();
        if (argc > 1)
            filename = argv[1];
        if (filename.extension() != L".ini")
            filename.append(L".ini");
        filename = GuiUtils::SanitiseFilename(filename.string());
        filename = Resources::GetPath(filename);

        OnPreferencesLoadFileChosen(filename.string().c_str());
    }

    enum class PendingAction {
        NONE,
        EXPAND_ALL,
        COLLAPSE_ALL,
        WAIT_REFRESH_LOG,
        REFRESH_LOG,
        OPEN_LOG
    } pending_action = PendingAction::NONE;

    void ToggleQuestEntrySection(QuestEntryGroup* entry, bool is_visible, bool refresh_log = true) {
        if (!(entry && entry->is_visible != is_visible))
            return;
        entry->is_visible = is_visible;
        quest_entry_group_visibility[entry->encoded_name] = is_visible;
        if(refresh_log)
            pending_action = PendingAction::WAIT_REFRESH_LOG;
    }

    typedef QuestEntryGroup*(__fastcall* GetOrCreateQuestEntryGroup_pt)(QuestEntryGroupContext* context, void* edx, wchar_t* quest_entry_group_name);
    GetOrCreateQuestEntryGroup_pt GetOrCreateQuestEntryGroup_Func = 0;
    GetOrCreateQuestEntryGroup_pt GetOrCreateQuestEntryGroup_Ret = 0;
    // NB: Don't call this from gwtoolbox; it'll create a quest entry group if it doesn't find one. Instead use GetQuestEntryGroup
    QuestEntryGroup* __fastcall OnGetOrCreateQuestEntryGroup(QuestEntryGroupContext* context, void* edx, wchar_t* quest_entry_group_name) {
        GW::Hook::EnterHook();
        bool is_creating = GetQuestEntryGroup(quest_entry_group_name) == nullptr;
        QuestEntryGroup* group = GetOrCreateQuestEntryGroup_Ret(context, edx, quest_entry_group_name);
        auto current_quest_group = GetQuestEntryGroup(GW::QuestMgr::GetActiveQuestId());
        if (!is_creating)
            goto ret;
        
        if (group == current_quest_group) {
            // Current quest group, leave open
            goto ret;
        }
        if (quest_entry_group_visibility.contains(group->encoded_name)) {
            // Override collapsed state, GW has just created this quest entry group and defaults to open
            group->is_visible = quest_entry_group_visibility[group->encoded_name];
            goto ret;
        }
    ret:
        GW::Hook::LeaveHook();
        return group;
    }

    typedef void(__fastcall* OnQuestEntryGroupInteract_pt)(void* context, void* edx, uint32_t* wparam);
    OnQuestEntryGroupInteract_pt OnQuestEntryGroupInteract_Func = 0;
    OnQuestEntryGroupInteract_pt OnQuestEntryGroupInteract_Ret = 0;
    void __fastcall OnQuestEntryGroupInteract(uint32_t* context, void* edx, uint32_t* wparam) {
        GW::HookBase::EnterHook();
        if (wparam[1] == 0 && wparam[2] == 6) {
            // Player manually toggled visibility
            bool is_visible = (bool)wparam[3];
            wchar_t* quest_entry_group_name = (wchar_t*)(context[2]);
            quest_entry_group_visibility[quest_entry_group_name] = is_visible;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                // Collapse or expand all
                if (is_visible) {
                    pending_action = PendingAction::EXPAND_ALL;
                }
                else {
                    pending_action = PendingAction::COLLAPSE_ALL;
                }
            }
        }
        OnQuestEntryGroupInteract_Ret(context, edx, wparam);
        GW::HookBase::LeaveHook();
    }
    const char* section_name = "GuildWarsSettingsModule:Quest Log Entries Visible";
}

void GuildWarsSettingsModule::Initialize() {
    ToolboxModule::Initialize();
    // NB: This address is fond twice, we only care about the first.
    uintptr_t address = GW::Scanner::FindAssertion("p:\\code\\engine\\frame\\frkey.cpp","count == arrsize(s_remapTable)",0x13);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address))
        key_mappings_array = *(uint32_t**)address;

    address = GW::Scanner::Find("\x89\x77\x1c\x5f\x5e\x8b\xe5\x5d\xc2\x04\x00", "xxxxxxxxxxx", -0x155);
    if (address) {
        OnQuestEntryGroupInteract_Func = (OnQuestEntryGroupInteract_pt)address;
        GW::Hook::CreateHook(OnQuestEntryGroupInteract_Func, OnQuestEntryGroupInteract, (void**)&OnQuestEntryGroupInteract_Ret);
        GW::Hook::EnableHooks(OnQuestEntryGroupInteract_Func);

        quest_entry_group_context = *(QuestEntryGroupContext**)(address + 0x12f);

        GetOrCreateQuestEntryGroup_Func = (GetOrCreateQuestEntryGroup_pt) GW::Scanner::FunctionFromNearCall(address + 0x136);
        if (GetOrCreateQuestEntryGroup_Func) {
            GW::Hook::CreateHook(GetOrCreateQuestEntryGroup_Func, OnGetOrCreateQuestEntryGroup, (void**)&GetOrCreateQuestEntryGroup_Ret);
            GW::Hook::EnableHooks(GetOrCreateQuestEntryGroup_Func);
        }
    }



    GW::Chat::CreateCommand(L"saveprefs", CmdSave);

    GW::Chat::CreateCommand(L"loadprefs", CmdLoad);

}

void GuildWarsSettingsModule::Update(float) {
    switch (pending_action) {
    case PendingAction::WAIT_REFRESH_LOG:
        // In some cases, we have to wait a frame before toggling the log e.g. taking a new quest
        pending_action = PendingAction::REFRESH_LOG;
        break;
    case PendingAction::REFRESH_LOG: {
        auto window_pos = GW::UI::GetWindowPosition(GW::UI::WindowID_QuestLog);
        if (window_pos && window_pos->visible()) {
            GW::UI::Keypress(GW::UI::ControlAction_OpenQuestLog);
            pending_action = PendingAction::OPEN_LOG;
        }
    } break;
    case PendingAction::OPEN_LOG:
        GW::UI::Keypress(GW::UI::ControlAction_OpenQuestLog);
        pending_action = PendingAction::NONE;
        break;
    case PendingAction::EXPAND_ALL:
        if (quest_entry_group_context) {
            auto& quest_entries = quest_entry_group_context->quest_entries;
            for (auto& entry : quest_entries) {
                ToggleQuestEntrySection(&entry, true);
            }
        }
        pending_action = PendingAction::REFRESH_LOG;
        break;
    case PendingAction::COLLAPSE_ALL:
        if (quest_entry_group_context) {
            auto& quest_entries = quest_entry_group_context->quest_entries;
            for (auto& entry : quest_entries) {
                ToggleQuestEntrySection(&entry, false);
            }
        }
        pending_action = PendingAction::REFRESH_LOG;
        break;
    }
}

void GuildWarsSettingsModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    CSimpleIni::TNamesDepend keys;

    const auto current_quest_group = GetQuestEntryGroup(GW::QuestMgr::GetActiveQuestId());
    if (ini->GetAllKeys(section_name, keys)) {
        std::wstring quest_entry_group_name;
        for (const auto& key : keys) {
            if (!GuiUtils::IniToArray(key.pItem, quest_entry_group_name))
                continue; // @Cleanup: error handling
            const bool is_visible = ini->GetBoolValue(section_name, key.pItem, true);
            quest_entry_group_visibility[quest_entry_group_name] = is_visible;
            const auto group = GetQuestEntryGroup(quest_entry_group_name.c_str());
            if (current_quest_group != group) {
                ToggleQuestEntrySection(group, is_visible);
            }
        }
    }
}
void GuildWarsSettingsModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    CSimpleIni::TNamesDepend keys;
    
    std::string tmp;
    for (const auto& it : quest_entry_group_visibility) {
        if (!GuiUtils::ArrayToIni(it.first, &tmp))
            continue;// @Cleanup: error handling
        ini->SetBoolValue(section_name, tmp.c_str(), it.second);
    }
}

void GuildWarsSettingsModule::Terminate() {
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(L"saveprefs");
    GW::Chat::DeleteCommand(L"loadprefs");
    if (OnQuestEntryGroupInteract_Func) {
        GW::Hook::RemoveHook(OnQuestEntryGroupInteract_Func);
    }
    if (GetOrCreateQuestEntryGroup_Func) {
        GW::Hook::RemoveHook(GetOrCreateQuestEntryGroup_Func);
    }
}
void GuildWarsSettingsModule::DrawSettingInternal() {
    ImGui::TextUnformatted("Choose a file from your computer to load Guild Wars settings");
    if (ImGui::Button("Load from disk...")) {
        std::filesystem::path filename = GetDefaultFilename();
        filename = Resources::GetPath(filename);
        Resources::OpenFileDialog(OnPreferencesLoadFileChosen,"ini",filename.string().c_str());
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Choose a file from your computer to save Guild Wars settings");
    if (ImGui::Button("Save to disk...")) {
        std::filesystem::path filename = GetDefaultFilename();
        filename = Resources::GetPath(filename);
        Resources::SaveFileDialog(OnPreferencesSaveFileChosen,"ini",filename.string().c_str());
    }
    (quest_entry_group_context);

}
