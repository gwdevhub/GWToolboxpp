#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Color.h>
#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Widgets/BondsWidget.h>
#include <Windows/FriendListWindow.h>

namespace {
    struct AvailableBond {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        GuiUtils::EncString skill_name;
        bool enabled = true;

        AvailableBond(const GW::Constants::SkillID _skill_id, const bool _enabled = true)
            : skill_id(_skill_id), enabled(_enabled) { };

        void Initialize()
        {
            // Because AvialableBond is used statically in toolbox, we need to explicitly call this function in the render loop - otherwise GetSkillConstantData won't be called at the right time.
            if (const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id)) {
                skill_name.reset(skill->name);
            }
        }
    };

    // Skill ID => enabled by default
    AvailableBond available_bonds[] = {
        {GW::Constants::SkillID::Balthazars_Spirit, true},
        {GW::Constants::SkillID::Essence_Bond, true},
        {GW::Constants::SkillID::Holy_Veil, true},
        {GW::Constants::SkillID::Life_Attunement, true},
        {GW::Constants::SkillID::Life_Barrier, true},
        {GW::Constants::SkillID::Life_Bond, true},
        {GW::Constants::SkillID::Live_Vicariously, true},
        {GW::Constants::SkillID::Mending, true},
        {GW::Constants::SkillID::Protective_Bond, true},
        {GW::Constants::SkillID::Purifying_Veil, true},
        {GW::Constants::SkillID::Retribution, true},
        {GW::Constants::SkillID::Strength_of_Honor, true},
        {GW::Constants::SkillID::Succor, true},
        {GW::Constants::SkillID::Vital_Blessing, true},
        {GW::Constants::SkillID::Watchful_Spirit, true},
        {GW::Constants::SkillID::Watchful_Intervention, false},
        {GW::Constants::SkillID::Heroic_Refrain, true},
        {GW::Constants::SkillID::Burning_Refrain, true},
        {GW::Constants::SkillID::Mending_Refrain, true},
        {GW::Constants::SkillID::Bladeturn_Refrain, true},
        {GW::Constants::SkillID::Hasty_Refrain, true},
        {GW::Constants::SkillID::Aggressive_Refrain, false}
    };

    AvailableBond* GetAvailableBond(const GW::Constants::SkillID skill_id)
    {
        for (auto& b : available_bonds) {
            if (b.skill_id == skill_id) {
                return &b;
            }
        }
        return nullptr;
    }


    // settings
    bool hide_in_outpost = false;
    bool click_to_cast = true;
    bool click_to_drop = true;
    bool show_allies = true;
    bool flip_bonds = false;

    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 64;

    bool overlay_party_window = false;

    Color background = 0;
    Color low_attribute_overlay = 0;

    std::vector<GW::Constants::SkillID> bond_list{};               // index to skill id
    std::unordered_map<GW::Constants::SkillID, size_t> bond_map{}; // skill id to index

    bool UseBuff(GW::AgentID agent_id, GW::Constants::SkillID skill_id)
    {
        if (!(GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && !GW::Map::GetIsObserving())) {
            return false;
        }
        const auto target = GW::Agents::GetAgentByID(agent_id);
        if (!target) {
            return false;
        }

        const auto islot = GW::SkillbarMgr::GetSkillSlot(skill_id);
        if (islot < 0) {
            return false;
        }
        const auto slot = static_cast<uint32_t>(islot);
        const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!skillbar || !skillbar->IsValid()) {
            return false;
        }
        if (skillbar->skills[slot].recharge != 0) {
            return false;
        }

        // capture by value!
        GW::GameThread::Enqueue([slot, agent_id] {
            GW::SkillbarMgr::UseSkill(slot, agent_id);
        });
        return true;
    }

    bool FetchBondSkills()
    {
        const GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!bar || !bar->IsValid()) {
            return false;
        }
        bond_list.clear();
        bond_map.clear();
        for (const auto& skill : bar->skills) {
            auto skill_id = static_cast<GW::Constants::SkillID>(skill.skill_id);
            if (const auto found = GetAvailableBond(skill_id); found && found->enabled) {
                bond_map[skill_id] = bond_list.size();
                bond_list.push_back(skill_id);
            }
        }
        return true;
    }

    bool DropBuffs(GW::AgentID targetId, GW::Constants::SkillID skill_id) {
        const auto buffs = GW::Effects::GetPlayerBuffs();
        if (!buffs)
            return false;
        size_t dropped = 0;
        for (const auto& buff : *buffs) {
            if (!(skill_id == (GW::Constants::SkillID)0 || buff.skill_id == skill_id))
                continue;
            if (!(targetId == (GW::AgentID)0 || buff.target_agent_id == targetId))
                continue;
            const auto buff_id = buff.buff_id;
            GW::GameThread::Enqueue([buff_id] {
                GW::Effects::DropBuff(buff_id);
                });
            dropped++;
        }
        return dropped > 0;
    }

    uint32_t GetPartyMemberAgentId(uint32_t index) {
        const auto info = GW::PartyMgr::GetPartyInfo();
        if (!info)
            return 0;
        uint32_t i = 0;
        for (auto& player : info->players) {
            if (i == index)
                return GW::PlayerMgr::GetPlayerAgentId(player.login_number);
            i++;
            for (auto& hero : info->heroes) {
                if (hero.owner_player_id != player.login_number)
                    continue;
                if (i == index)
                    return hero.agent_id;
                i++;
            }
        }
        for (auto& henchman : info->henchmen) {
            if (i == index)
                return henchman.agent_id;
            i++;
        }
        for (auto& agent_id : info->others) {
            if (i == index)
                return agent_id;
            i++;
        }
        return 0;
    }

    bool ToggleBuff(GW::AgentID agent_id, GW::Constants::SkillID skill_id) {
        return DropBuffs(agent_id, skill_id) || UseBuff(agent_id, skill_id);
    }

    const char* cmd_bonds_syntax = "'/bonds [remove|add] [party_member_index|all] [all|skill_id]' remove or add bonds from a single party member, or all party members";

    void CHAT_CMD_FUNC(CmdBondsAddRemove) {

        const auto syntax_err = [argc, argv] {
            Log::WarningW(L"Invalid syntax for /%s; correct syntax:\n%S", argc ? argv[0] : L"Unk", cmd_bonds_syntax);
            };

        if (argc < 4) {
            syntax_err();
            return;
        }
        bool add_bond = true;
        uint32_t agent_id = 0;
        uint32_t skill_id = 0;

        if (wcscmp(argv[1], L"add") == 0) {
            add_bond = true;
        }
        else if (wcscmp(argv[1], L"remove") == 0) {
            add_bond = false;
        }
        else {
            syntax_err();
            return;
        }
        // Party member (or all)
        if (wcscmp(argv[2], L"all") != 0) {
            uint32_t party_member_idx = 0;
            if (!TextUtils::ParseUInt(argv[2], &party_member_idx)) {
                syntax_err();
                return;
            }
            agent_id = GetPartyMemberAgentId(party_member_idx);
            if (!agent_id) {
                return; // Failed to find party member
            }
        }
        // Skill
        if (wcscmp(argv[3], L"all") != 0) {
            if (!TextUtils::ParseUInt(argv[3], &skill_id)) {
                syntax_err();
                return;
            }
        }
        if (add_bond && !skill_id) {
            Log::WarningW(L"/%s: skill_id required when adding bond", argv[0]);
            syntax_err();
            return;
        }
        if (skill_id >= std::to_underlying(GW::Constants::SkillID::Count)) {
            Log::WarningW(L"%d: is not a valid skill id", skill_id);
            syntax_err();
            return;
        }
        if (add_bond && !agent_id) {
            Log::WarningW(L"/%s: party_member_index required when adding bond", argv[0]);
            syntax_err();
            return;
        }

        if (add_bond) {
            UseBuff(agent_id, static_cast<GW::Constants::SkillID>(skill_id));
        }
        else {
            DropBuffs(agent_id, static_cast<GW::Constants::SkillID>(skill_id));
        }

    }

}

bool BondsWidget::IsBondLikeSkill(GW::Constants::SkillID skill_id) {
    return GetAvailableBond(skill_id) != nullptr;
}

void BondsWidget::Initialize()
{
    SnapsToPartyWindow::Initialize();
    GW::Chat::CreateCommand(L"bonds", CmdBondsAddRemove);
    for (auto& b : available_bonds) {
        b.Initialize();
    }
}
void BondsWidget::Terminate()
{
    SnapsToPartyWindow::Terminate();
    GW::Chat::DeleteCommand(L"bonds");
}

bool BondsWidget::DrawBondImage(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out) {
    if (!GetBondPosition(agent_id, skill_id, top_left_out, bottom_right_out))
        return false;
    const auto texture = *Resources::GetSkillImage(skill_id);
    if (texture) {
        ImGui::AddImageCropped(texture, *top_left_out, *bottom_right_out);
        return true;
    }
    return false;
}

bool BondsWidget::GetBondPosition(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out) {

    const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
    if (!health_bar_pos)
        return false;

    if (!party_indeces_by_agent_id.contains(agent_id))
        return false;
    const auto party_slot = party_indeces_by_agent_id[agent_id];
    if (party_slot >= allies_start_idx && !show_allies)
        return false;

    if (!bond_map.contains(skill_id)) {
        return false; // bond with a skill not in skillbar
    }

    const auto img_width = health_bar_pos->bottom_right.y - health_bar_pos->top_left.y;
    const auto y = health_bar_pos->top_left.y;
    const auto x = ImGui::GetCurrentWindow()->Pos.x + (img_width * bond_map[skill_id]);

    *top_left_out = { x, y };
    *bottom_right_out = { x + img_width, y + img_width };
    return true;
}

void BondsWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    const GW::PlayerArray* players = info ? GW::Agents::GetPlayerArray() : nullptr;
    if (!players) {
        return;
    }
    // note: info->heroes, ->henchmen, and ->others CAN be invalid during normal use.

    // @Cleanup: This doesn't need to be done every frame - only when player skills have changed
    if (!FetchBondSkills()) {
        return;
    }
    if (bond_list.empty()) {
        return; // Don't display bonds widget if we've not got any bonds on our skillbar
    }
    // @Cleanup: Only call when the party window has been moved or updated
    if (!(FetchPartyInfo() && RecalculatePartyPositions())) {
        return;
    }

    // ==== Draw ====

    const auto& first_health_bar_position = agent_health_bar_positions.begin()->second;

    const auto img_width = (first_health_bar_position.bottom_right.y - first_health_bar_position.top_left.y);

    const float width = bond_list.size() * img_width;

    const auto user_offset_x = abs(static_cast<float>(user_offset));
    float window_x = .0f;
    if (overlay_party_window) {
        window_x = party_health_bars_position.top_left.x + user_offset_x;
        if (user_offset < 0) {
            window_x = party_health_bars_position.bottom_right.x - user_offset_x - width;
        }

    }
    else {
        window_x = party_health_bars_position.top_left.x - user_offset_x - width;
        if (window_x < 0 || user_offset < 0) {
            // Right placement
            window_x = party_health_bars_position.bottom_right.x + user_offset_x;
        }
    }
    // Add a window to capture mouse clicks.
    ImGui::SetNextWindowPos({ window_x,party_health_bars_position.top_left.y });
    ImGui::SetNextWindowSize({ width, party_health_bars_position.bottom_right.y - party_health_bars_position.top_left.y });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(click_to_cast || click_to_drop)))) {
        const auto draw_list = ImGui::GetWindowDrawList();
        bool handled_click = false;
        ImVec2 bond_top_left;
        ImVec2 bond_bottom_right;

        for (auto& [agent_id, party_slot] : party_indeces_by_agent_id) {
            if (!GetBondPosition(agent_id, bond_list[0], &bond_top_left, &bond_bottom_right))
                continue;
            draw_list->AddRectFilled({ window_x , bond_top_left.y}, { window_x + width, bond_bottom_right.y }, background);
        }

        if (GW::BuffArray* buffs = GW::Effects::GetPlayerBuffs()) {
            for (const auto& buff : *buffs) {
                DrawBondImage(buff.target_agent_id, buff.skill_id, &bond_top_left, &bond_bottom_right);
                if (!handled_click && ImGui::IsMouseHoveringRect(bond_top_left, bond_bottom_right, false) && ImGui::IsMouseReleased(0)) {
                    if(click_to_drop)
                        DropBuffs(buff.target_agent_id, buff.skill_id);
                    handled_click = true;
                }
            }
        }

        // Player and hero effects that aren't bonds
        if (const GW::AgentEffectsArray* agent_effects_array = GW::Effects::GetPartyEffectsArray(); agent_effects_array != nullptr) {
            for (auto& agent_effects_it : *agent_effects_array) {
                auto& agent_effects = agent_effects_it.effects;
                if (!agent_effects.valid()) {
                    continue;
                }
                const auto agent_id = agent_effects_it.agent_id;
                for (const GW::Effect& effect : agent_effects) {
                    const auto skill_id = static_cast<GW::Constants::SkillID>(effect.skill_id);

                    const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                    if (!skill_data || skill_data->duration0 == 0x20000) {
                        continue; // Maintained skill/enchantment
                    }

                    if (!DrawBondImage(agent_id, skill_id, &bond_top_left, &bond_bottom_right))
                        continue;

                    const GW::Attribute* agentAttributes = GW::PartyMgr::GetAgentAttributes(agent_id);
                    ASSERT(agentAttributes);
                    agentAttributes = &agentAttributes[static_cast<size_t>(skill_data->attribute)];
                    const bool overlay = effect.attribute_level < agentAttributes->level;

                    if (overlay) {
                        draw_list->AddRectFilled(bond_top_left, bond_bottom_right, low_attribute_overlay);
                    }
                }
            }
        }

        if (!handled_click) {
            for (auto agent_id : party_agent_ids_by_index) {
                for (auto skill_id : bond_list) {
                    if (!GetBondPosition(agent_id, skill_id, &bond_top_left, &bond_bottom_right))
                        continue;
                    if (!ImGui::IsMouseHoveringRect(bond_top_left, bond_bottom_right, false))
                        continue;
                    draw_list->AddRect(bond_top_left, bond_bottom_right, IM_COL32(255, 255, 255, 255));
                    if (ImGui::IsMouseReleased(0)) {
                        if(click_to_cast)
                            UseBuff(agent_id, skill_id);
                        handled_click = true;
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);
}

void BondsWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    background = Colors::Load(ini, Name(), VAR_NAME(background), Colors::ARGB(76, 0, 0, 0));
    low_attribute_overlay = Colors::Load(ini, Name(), VAR_NAME(low_attribute_overlay), Colors::ARGB(76, 0, 0, 0));

    LOAD_BOOL(click_to_cast);
    LOAD_BOOL(click_to_drop);
    LOAD_BOOL(show_allies);
    LOAD_BOOL(flip_bonds);
    LOAD_BOOL(hide_in_outpost);
    LOAD_UINT(user_offset);
    LOAD_BOOL(overlay_party_window);

    for (auto& b : available_bonds) {
        char buf[128];
        const int written = snprintf(buf, sizeof(buf), "bond_enabled_%d", b.skill_id);
        ASSERT(written != -1);
        b.enabled = ini->GetBoolValue(Name(), buf, b.enabled);
    }
}

void BondsWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_COLOR(background);
    SAVE_COLOR(low_attribute_overlay);
    SAVE_BOOL(click_to_cast);
    SAVE_BOOL(click_to_drop);
    SAVE_BOOL(show_allies);
    SAVE_BOOL(flip_bonds);
    SAVE_BOOL(hide_in_outpost);
    SAVE_UINT(user_offset);
    SAVE_BOOL(overlay_party_window);

    for (const auto& b : available_bonds) {
        char buf[128];
        const int written = snprintf(buf, sizeof(buf), "bond_enabled_%d", b.skill_id);
        ASSERT(written != -1);
        ini->SetBoolValue(Name(), buf, b.enabled);
    }
}

void BondsWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    if (bond_list.empty()) {
        ImGui::TextColored(ImVec4(0xFF, 0, 0, 0xFF), "Equip a maintainable enchantment or refrain to show bonds widget on-screen");
    }
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show on top of health bars", &overlay_party_window);
    ImGui::ShowHelp("Untick to show this widget to the left (or right) of the party window.\nTick to show this widget over the top of the party health bars inside the party window");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("Party window offset", &user_offset);
    ImGui::TextUnformatted("Skills enabled for bond monitor:");
    ImGui::Indent();
    ImGui::StartSpacedElements(180.f);
    for (auto& bond : available_bonds) {
        char label_buf[128];
        ImGui::NextSpacedElement();
        const auto written = snprintf(label_buf, sizeof(label_buf), "%s##available_bond_%p", bond.skill_name.string().c_str(), &bond);
        ASSERT(written != -1);
        if (ImGui::Checkbox(label_buf, &bond.enabled)) {
            FetchBondSkills();
        }
    }
    ImGui::Unindent();

    Colors::DrawSettingHueWheel("Background", &background, 0);
    ImGui::Checkbox("Click to cast bond", &click_to_cast);
    ImGui::Checkbox("Click to cancel bond", &click_to_drop);
    ImGui::Checkbox("Show bonds for Allies", &show_allies);
    ImGui::ShowHelp("'Allies' meaning the ones that show in party window, such as summoning stones");
    ImGui::Checkbox("Flip bond order (left/right)", &flip_bonds);
    ImGui::ShowHelp("Bond order is based on your build. Check this to flip them left <-> right");
    Colors::DrawSetting("Low Attribute Overlay", &low_attribute_overlay);
    ImGui::ShowHelp(
        "Overlays effects casted with less than current attribute level.\n"
        "Only works for yourself and your heroes and doesn't include bonds."
    );
}
