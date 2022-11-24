module;

#include "stdafx.h"

#include "ToolboxWindow.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Utilities/Scanner.h>
#include <Timer.h>
#include <ImGuiAddons.h>

export module ArmoryWindow;
import :helpers;
import :database;

export class ArmoryWindow : public ToolboxWindow {
    ArmoryWindow() = default;
    ~ArmoryWindow() = default;

public:
    static ArmoryWindow& Instance()
    {
        static ArmoryWindow instance;
        return instance;
    }

    const char* Name() const override { return "Armory"; }
    const char* Icon() const override { return ICON_FA_VEST; }

    void DrawSettingInternal() override;
    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini);
    void SaveSettings(CSimpleIni* ini);
};

using namespace Helpers;

bool reset_helm_visibility = false;
bool head_item_set = false;
void ArmoryWindow::Update(const float delta)
{
    ToolboxWindow::Update(delta);
    if (!head_item_set) {
        reset_helm_visibility = false;
        return;
    }
    static clock_t timer = 0;

    const auto is_visible = GW::Items::GetEquipmentVisibility(GW::EquipmentType::Helm);
    static auto wanted_visibility = is_visible;

    const auto toggle_visibility = [is_visible] {
        if (is_visible == GW::EquipmentStatus::AlwaysHide) {
            GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
        }
        else if (is_visible == GW::EquipmentStatus::HideInCombatAreas) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
            }
        }
        else if (is_visible == GW::EquipmentStatus::HideInTownsAndOutposts) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
                GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
            }
        }
    };

    if (reset_helm_visibility) {
        timer = TIMER_INIT();
        wanted_visibility = is_visible;
        reset_helm_visibility = false;
        toggle_visibility();
    }

    if (timer != 0 && TIMER_DIFF(timer) > 300) {
        GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, wanted_visibility);
        timer = 0;
        head_item_set = false;
    }
}

void ArmoryWindow::Draw(IDirect3DDevice9*)
{
    if (!visible)
        return;
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    if (!player_agent)
        return;

    bool update_data = false;
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);
    if (prof != current_profession)
        update_data = true;

    static int filter_index = Database::Campaign_All;

    // constexpr ImVec2 window_size(330.f, 208.f);
    // ImGui::SetNextWindowSize(window_size);
    if (ImGui::Begin("Armory", &visible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Profession: %s", GetProfessionName(prof));
        ImGui::SameLine(ImGui::GetWindowWidth() - 65.f);

        if (ImGui::Button("Refresh")) {
            reset_helm_visibility = true;
            head.current_piece = nullptr;
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
        if (update_data) {
            UpdateArmorsFilter(prof, static_cast<Database::Campaign>(filter_index));
        }

        if (DrawArmorPiece("##head", &player_armor.head, &head)) {
            SetArmorItem(&player_armor.head);
            head_item_set = true;
        }
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

void ArmoryWindow::DrawSettingInternal()
{
    ToolboxWindow::DrawSettingInternal();

    ImGui::Checkbox("Visible", &visible);
}

void ArmoryWindow::LoadSettings(CSimpleIniA* ini) {
    ToolboxWindow::LoadSettings(ini);
}

void ArmoryWindow::SaveSettings(CSimpleIniA* ini) {
    ToolboxWindow::SaveSettings(ini); 
}

void ArmoryWindow::Initialize()
{
    ToolboxWindow::Initialize();

    GW::Scanner::Initialize();
    const auto old_color = GW::Chat::SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFFFFFFFF);
    SetItem_Func = reinterpret_cast<SetItem_pt>(GW::Scanner::Find("\x83\xC4\x04\x8B\x08\x8B\xC1\xC1", "xxxxxxxx", -0x24));
    if (!SetItem_Func) {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find the SetItem function.", L"Armory");
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

    GW::Chat::WriteChat(GW::Chat::CHANNEL_GWCA1, L"Initialized", L"Armory");
    GW::Chat::SetMessageColor(GW::Chat::CHANNEL_GWCA1, old_color);
}

void ArmoryWindow::Terminate()
{
    ToolboxWindow::Terminate();
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);

    UpdateArmorsFilter(prof, Database::Campaign_All);
}
