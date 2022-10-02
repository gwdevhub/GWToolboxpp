#include "Armory.h"

#include "stl.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include <imgui.h>

#include "ArmorsDatabase.h"
#include "Helpers.h"
#include "ImGuiAddons.h"

HMODULE plugin_handle;
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Armory instance;
    return &instance;
}

void Armory::Draw(IDirect3DDevice9* device)
{
    if (!toolbox_handle)
        return;
    if (!visible)
        return;
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    if (!player_agent)
        return;

    bool update_data = false;
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);
    if (prof != current_profession)
        update_data = true;

    static int filter_index = Campaign_All;

    // constexpr ImVec2 window_size(330.f, 208.f);
    // ImGui::SetNextWindowSize(window_size);
    if (ImGui::Begin("Armory", &visible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Profession: %s", GetProfessionName(prof));
        ImGui::SameLine(ImGui::GetWindowWidth() - 65.f);

        if (ImGui::Button("Refresh")) {
            if (player_agent->equip && player_agent->equip[0]) {
                GW::Equipment* equip = player_agent->equip[0];
                InitItemPiece(&player_armor.head, &equip->head);
                InitItemPiece(&player_armor.chest, &equip->chest);
                InitItemPiece(&player_armor.hands, &equip->hands);
                InitItemPiece(&player_armor.legs, &equip->legs);
                InitItemPiece(&player_armor.feets, &equip->feet);
            }

            update_data = true;
        }

        if (ImGui::MyCombo("##filter", "All", &filter_index, armor_filter_array_getter, nullptr, 5))
            update_data = true;
        if (update_data)
            UpdateArmorsFilter(prof, static_cast<Campaign>(filter_index));

        if (DrawArmorPiece("##head", &player_armor.head, &head))
            SetArmorItem(&player_armor.head);
        if (DrawArmorPiece("##chest", &player_armor.chest, &chest))
            SetArmorItem(&player_armor.chest);
        if (DrawArmorPiece("##hands", &player_armor.hands, &hands))
            SetArmorItem(&player_armor.hands);
        if (DrawArmorPiece("##legs", &player_armor.legs, &legs))
            SetArmorItem(&player_armor.legs);
        if (DrawArmorPiece("##feets", &player_armor.feets, &feets))
            SetArmorItem(&player_armor.feets);
    }
    ImGui::End();
}

void Armory::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    ImGui::Checkbox("Visible", &visible);
}

void Armory::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::Scanner::Initialize();
    SetItem_Func = reinterpret_cast<SetItem_pt>(GW::Scanner::Find("\x83\xC4\x04\x8B\x08\x8B\xC1\xC1", "xxxxxxxx", -0x24));
    if (!SetItem_Func) {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, L"Armory: Failed to find the SetItem function");
        return;
    }

    const auto player_agent = GW::Agents::GetPlayerAsAgentLiving();
    if (player_agent && player_agent->equip && player_agent->equip[0]) {
        GW::Equipment* equip = player_agent->equip[0];
        InitItemPiece(&player_armor.head, &equip->head);
        InitItemPiece(&player_armor.chest, &equip->chest);
        InitItemPiece(&player_armor.hands, &equip->hands);
        InitItemPiece(&player_armor.legs, &equip->legs);
        InitItemPiece(&player_armor.feets, &equip->feet);
    }

    GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, L"Armory: Initialized");
}

void Armory::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);

    UpdateArmorsFilter(prof, Campaign_All);
}
