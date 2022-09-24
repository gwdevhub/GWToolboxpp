#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Windows/GuildWarsPreferencesWindow.h>


namespace {
    uint32_t GetWindowNameID(GW::UI::WindowID window_id) {
        using namespace GW::UI;
        switch (window_id) {
        case WindowID_Dialogue1: return 0x2db;
        case WindowID_Dialogue2: return 0x2dc;
        case WindowID_MissionGoals: return 0x2dd;
        case WindowID_DropBundle: return 0x2de;
        case WindowID_Compass: return 0x2df;
        case WindowID_EffectsMonitor: return 0x2e1;
        case WindowID_EnergyBar: return 0x2e2;
        case WindowID_ExperienceBar: return 0x2e3;
        case WindowID_HealthBar: return 0x2e4;
        case WindowID_Hints: return 0x2e5;
        case WindowID_MissionProgress: return 0x2e6;
        case WindowID_Skillbar: return 0x2e9;
        case WindowID_SkillMonitor: return 0x2ea;
        case WindowID_SkillWarmup: return 0x2eb;
        case WindowID_UpkeepMonitor: return 0x2ec;
        case WindowID_Menu: return 0x2ed;
        case WindowID_TargetDisplay: return 0x2ee;
        case WindowID_WeaponBar: return 0x2f0;
        case WindowID_PartySearch: return 0xcc2a;
        case WindowID_InventoryBags: return 0x34b;
        case WindowID_Hero: return 0x34c;
        case WindowID_Chat: return 0x34d;
        case WindowID_Friends: return 0x34f;
        case WindowID_Guild: return 0x350;
        case WindowID_Inventory: return 0x351;
        case WindowID_MissionMap: return 0x353;
        case WindowID_Observe: return 0x354;
        case WindowID_Options: return 0x356;
        case WindowID_PartyWindow: return 0x357;
        case WindowID_QuestLog: return 0x358;
        case WindowID_TradeButton: return 0x99c8;
        case WindowID_VaultBox: return 0x31b;
        case WindowID_Merchant: return 0xbdf; // Game uses NPC name, default to "Merchant"
        case WindowID_InGameClock: return 0x13418;

        default:
            return 0;
        }
    }
    /*uint32_t GetPreferenceNameID(GW::UI::Preference pref_id) {
        using namespace GW::UI;
        switch (pref_id) {
        case Preference_TextLanguage: return 0x561;
        case Preference_ChatFilterLevel: return 0x562;
        case Preference_ClockMode: return 0x13419;

        case Preference_AudioLanguage: return 0xdbe2;

        case Preference_DisableMouseWalking: return 0x571;
        case Preference_InvertMouseControlOfCamera: return 0x572;
        case Preference_InterfaceSize: return 0x573;
        case Preference_ShadowQuality: return 0x578;
        case Preference_CameraRotationSpeed: return 0x187b1;
        case Preference_FieldOfView: return 0x187b0;
        case Preference_DamageTextSize: return 0x7bc6;

        case Preference_ShowTextInSkillFloaters: return 0xdbe1;
        case Preference_DoNotShowSkillTipsOnEffectMonitor: return 0xd801;
        case Preference_FadeDistantNameTags: return 0x13191;
        case Preference_AlwaysShowFoeNames: return 0x5c6;
        case Preference_AlwaysShowAllyNames: return 0x5c4;
        case Preference_ItemRarityBorder: return 0x187bf;
        case Preference_ShowCollapsedBags: return 0x187be;
        case Preference_ShowChatTimestamps: return 0x187af;
        case Preference_WhispersFromFriendsEtcOnly: return 0x187ad;
        case Preference_ConciseSkillDescriptions: return 0x12c3f;
        case Preference_DoNotShowSkillTipsOnSkillBars: return 0xd746;
        case Preference_DoNotCloseWindowsOnEscape: return 0xd747;
        case Preference_AlwaysShowNearbyNamesPvP: return 0x57d;
        default:
            return 0;
        }
    }*/

    struct WindowPreference {
        GW::UI::WindowID window_id;
        GW::UI::WindowPosition position;
        GuiUtils::EncString name;
        WindowPreference(GW::UI::WindowID _window_id, GW::UI::WindowPosition* _position)
        : window_id(_window_id), position(*_position) {
            name.reset(GetWindowNameID(window_id));
        }
    };
    struct StringPreference {
        GW::UI::StringPreference pref_id;
        std::wstring value;
        StringPreference(GW::UI::StringPreference _pref_id, wchar_t* _value)
            : pref_id(_pref_id), value(_value) {};
    };

    struct GWPreferences {
        char name[128];
        RECT window_rect;
        bool reordered = false;
        std::vector<WindowPreference*> window_positions;
        std::vector<StringPreference*> preferences;
        void Draw();
    };

    void GetCurrentPreferences(GWPreferences* out) {
        for (const auto it : out->window_positions) {
            delete it;
        }
        out->window_positions.clear();
        for (GW::UI::WindowID i = GW::UI::WindowID_Dialogue1; i < GW::UI::WindowID_Count; i = (GW::UI::WindowID)(i+1)) {
            if (!GetWindowNameID(i))
                continue;
            out->window_positions.push_back(new WindowPreference(i,GW::UI::GetWindowPosition(i)));
        }
        for (const auto it : out->preferences) {
            delete it;
        }
        out->preferences.clear();
        // TODO: Read preferences of all types...
        out->reordered = false;
        ASSERT(GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(),&out->window_rect));
    }
    GWPreferences current_preferences;
}
void GWPreferences::Draw() {
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    char id[11];
    snprintf(id,sizeof(id),"%p", this);
    if (!ImGui::Begin(id)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Get current")) {
        GetCurrentPreferences(&current_preferences);
    }
    if (!current_preferences.reordered) {
        bool ok = true;
        for (auto* it : current_preferences.window_positions) {
            if (!it->name.string().length()) {
                ok = false;
                break;
            } 
        }
        if (ok) {
            std::ranges::sort(current_preferences.window_positions, [](WindowPreference* a, WindowPreference* b) {
                    return a->name.string() > b->name.string();
                });
            current_preferences.reordered = true;
        }
    }
    if (ImGui::TreeNodeEx("In-Game Preferences", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        const float avail_width = ImGui::GetContentRegionAvail().x;
        const float font_scale = ImGui::GetIO().FontGlobalScale;

        const float name_width = avail_width - 80.f * font_scale;
        float offset = 0.f;
        ImGui::Text(" ");
        ImGui::SameLine(name_width); ImGui::Text("Value");
        for (auto* pref : current_preferences.preferences) {
            offset = 0.f;
            ImGui::Text("find the name for this!");
            ImGui::SameLine(name_width);
            ImGui::Text("%ls", pref->value.c_str());
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("GUI positions", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        const float avail_width = ImGui::GetContentRegionAvail().x;
        const float font_scale = ImGui::GetIO().FontGlobalScale;

        const float atts_width = 80.f * font_scale;
        const float name_width = avail_width - (atts_width * 3);
        float offset = 0.f;
        ImGui::Text(" ");
        ImGui::SameLine(offset += name_width); ImGui::Text("Visible?");
        ImGui::SameLine(offset += atts_width); ImGui::Text("Position");
        ImGui::SameLine(offset += atts_width); ImGui::Text("Size");
        const float gw_scale = GuiUtils::GetGWScaleMultiplier();
        for (auto* pref : current_preferences.window_positions) {
            offset = 0.f;
            ImGui::Text(pref->name.string().c_str());
            ImGui::SameLine(offset += name_width); ImGui::Text(reinterpret_cast<const char*>(pref->position.visible() ? ICON_FA_CHECK : ICON_FA_TIMES));
            ImGui::SameLine(offset += atts_width); ImGui::Text("%.f / %.f", pref->position.left(gw_scale), pref->position.top(gw_scale));
            ImGui::SameLine(offset += atts_width); ImGui::Text("%.f / %.f", pref->position.width(gw_scale), pref->position.height(gw_scale));
        }
        ImGui::TreePop();
    }
    ImGui::End();
}
void GuildWarsPreferencesWindow::Initialize() {
    ToolboxWindow::Initialize();
    GetCurrentPreferences(&current_preferences);
}

void GuildWarsPreferencesWindow::Draw(IDirect3DDevice9*) {
    if (!visible)
        return;
    current_preferences.Draw();

}