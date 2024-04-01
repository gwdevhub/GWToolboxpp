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
    int row_height = 64;

    bool snap_to_party_window = true;
    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 64;

    GW::UI::WindowPosition* party_window_position = nullptr;

    void UseBuff(GW::AgentID targetId, DWORD buff_skillid)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return;
        }
        if (GW::Map::GetIsObserving()) {
            return;
        }
        if (targetId == 0) {
            return;
        }

        const GW::Agent* target = GW::Agents::GetAgentByID(targetId);
        if (target == nullptr) {
            return;
        }

        const auto islot = GW::SkillbarMgr::GetSkillSlot(static_cast<GW::Constants::SkillID>(buff_skillid));
        if (islot < 0) {
            return;
        }
        auto slot = static_cast<uint32_t>(islot);
        const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!skillbar || !skillbar->IsValid()) {
            return;
        }
        if (skillbar->skills[slot].recharge != 0) {
            return;
        }

        // capture by value!
        GW::GameThread::Enqueue([slot, targetId]() -> void {
            GW::SkillbarMgr::UseSkill(slot, targetId);
        });
    }

    Color background = 0;
    Color low_attribute_overlay = 0;

    std::vector<GW::Constants::SkillID> bond_list{};               // index to skill id
    std::unordered_map<GW::Constants::SkillID, size_t> bond_map{}; // skill id to index
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

    std::vector<GW::AgentID> party_list{};               // index to agent id
    std::unordered_map<GW::AgentID, size_t> party_map{}; // agent id to index
    size_t allies_start = 255;

    bool FetchPartyInfo()
    {
        const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
        if (!info) {
            return false;
        }
        party_map.clear();
        party_list.clear();
        allies_start = 255;
        for (const GW::PlayerPartyMember& player : info->players) {
            const auto id = GW::PlayerMgr::GetPlayerAgentId(player.login_number);
            if (!id) {
                continue;
            }
            party_map[id] = party_map.size();
            party_list.push_back(id);

            if (info->heroes.valid()) {
                for (const GW::HeroPartyMember& hero : info->heroes) {
                    if (hero.owner_player_id == player.login_number) {
                        party_map[hero.agent_id] = party_map.size();
                        party_list.push_back(hero.agent_id);
                    }
                }
            }
        }
        if (info->henchmen.valid()) {
            for (const GW::HenchmanPartyMember& hench : info->henchmen) {
                party_map[hench.agent_id] = party_map.size();
                party_list.push_back(hench.agent_id);
            }
        }
        if (show_allies && info->others.valid()) {
            for (const DWORD ally_id : info->others) {
                GW::Agent* agent = GW::Agents::GetAgentByID(ally_id);
                const GW::AgentLiving* ally = agent ? agent->GetAsAgentLiving() : nullptr;
                if (ally && ally->allegiance != GW::Constants::Allegiance::Minion && ally->GetCanBeViewedInPartyWindow() && !ally->GetIsSpawned()) {
                    if (allies_start == 255) {
                        allies_start = party_map.size();
                    }
                    party_map[ally_id] = party_map.size();
                    party_list.push_back(ally_id);
                }
            }
        }
        return true;
    }
}

bool BondsWidget::UseBuff(GW::AgentID targetId, GW::Constants::SkillID skill_id)
{
    if (GW::Map::GetIsObserving() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return false;
    }
    if (!GW::Agents::GetAgentByID(targetId)) {
        return false;
    }
    if (!IsBondLikeSkill(skill_id)) {
        return false;
    }

    const auto islot = GW::SkillbarMgr::GetSkillSlot(skill_id);
    if (islot < 0) {
        return false;
    }
    auto slot = static_cast<uint32_t>(islot);
    const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        return false;
    }
    if (skillbar->skills[slot].recharge != 0) {
        return false;
    }

    // capture by value!
    GW::GameThread::Enqueue([slot, targetId]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
        });
    return true;
}

bool BondsWidget::DropBuffs(GW::AgentID targetId, GW::Constants::SkillID skill_id) {
    const auto buffs = GW::Effects::GetPlayerBuffs();
    if (!buffs)
        return false;
    for (const auto& buff : *buffs) {
        if (!(skill_id == (GW::Constants::SkillID)0 || buff.skill_id == skill_id))
            continue;
        if (!(targetId == (GW::AgentID)0 || buff.target_agent_id == targetId))
            continue;
        const auto buff_id = buff.buff_id;
        GW::GameThread::Enqueue([buff_id]() -> void {
            GW::Effects::DropBuff(buff_id);
            });
    }
    return true;
}

bool BondsWidget::IsBondLikeSkill(GW::Constants::SkillID skill_id) {
    return GetAvailableBond(skill_id) != nullptr;
}

void BondsWidget::Initialize()
{
    ToolboxWidget::Initialize();
    party_window_position = GetWindowPosition(GW::UI::WindowID_PartyWindow);
    for (auto& b : available_bonds) {
        b.Initialize();
    }
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

    // ==== Get bonds ====
    // @Cleanup: This doesn't need to be done every frame - only when player skills have changed
    if (!FetchBondSkills()) {
        return;
    }
    if (bond_list.empty()) {
        return; // Don't display bonds widget if we've not got any bonds on our skillbar
    }
    // ==== Get party ====
    // @Cleanup: This doesn't need to be done every frame - only when the party has been changed
    if (!FetchPartyInfo()) {
        return;
    }

    // ==== Draw ====
    const auto img_size = row_height > 0 && !snap_to_party_window ? row_height : GuiUtils::GetPartyHealthbarHeight();
    const auto height = (party_list.size() + (allies_start < 255 ? 1 : 0)) * img_size;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);

    const float width = bond_list.size() * img_size;
    if (snap_to_party_window && party_window_position) {
        const float uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
        // NB: Use case to define GW::Vec4f ?
        GW::Vec2f x = party_window_position->xAxis();
        GW::Vec2f y = party_window_position->yAxis();

        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && GW::PartyMgr::GetIsHardModeUnlocked()) {
            constexpr int HARD_MODE_BUTTONS_HEIGHT = 30;
            y.x += HARD_MODE_BUTTONS_HEIGHT;
        }

        // Do the uiscale multiplier
        x *= uiscale_multiply;
        y *= uiscale_multiply;
        // Clamp
        ImVec4 rect(x.x, y.x, x.y, y.y);
        const ImVec4 viewport(0, 0, static_cast<float>(GW::Render::GetViewportWidth()), static_cast<float>(GW::Render::GetViewportHeight()));
        // GW Clamps windows to viewport; we need to do the same.
        GuiUtils::ClampRect(rect, viewport);
        // Left placement
        GW::Vec2f internal_offset(
            7.f, GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? 31.f : 34.f);
        internal_offset *= uiscale_multiply;
        const auto user_offset_x = abs(user_offset);
        float offset_width = width;
        auto calculated_pos =
            ImVec2(rect.x + internal_offset.x - user_offset_x - offset_width, rect.y + internal_offset.y);
        if (calculated_pos.x < 0 || user_offset < 0) {
            // Right placement
            internal_offset.x = 4.f * uiscale_multiply;
            offset_width = rect.z - rect.x;
            calculated_pos.x = rect.x - internal_offset.x + user_offset_x + offset_width;
        }
        ImGui::SetNextWindowPos(calculated_pos);
    }

    ImGui::SetNextWindowSize(ImVec2(width, height));
    if (ImGui::Begin(Name(), &visible, GetWinFlags(0, !(click_to_cast || click_to_drop)))) {
        const float win_x = ImGui::GetWindowPos().x;
        const float win_y = ImGui::GetWindowPos().y;
        auto GetGridPos = [&](const size_t _x, const size_t _y, const bool topleft) -> ImVec2 {
            size_t x = _x;
            size_t y = _y;
            if (y >= allies_start) {
                ++y;
            }
            if (!topleft) {
                ++x;
                ++y;
            }
            return { win_x + x * img_size, win_y + y * img_size };
        };

        bool handled_click = false;

        if (GW::BuffArray* buffs = GW::Effects::GetPlayerBuffs()) {
            for (const auto& buff : *buffs) {
                const auto agent = buff.target_agent_id;
                const auto skill = static_cast<GW::Constants::SkillID>(buff.skill_id);
                if (!party_map.contains(agent)) {
                    continue; // bond target not in party
                }
                if (!bond_map.contains(skill)) {
                    continue; // bond with a skill not in skillbar
                }
                const size_t y = party_map[agent];
                const size_t x = bond_map[skill];
                const auto texture = *Resources::GetSkillImage(buff.skill_id);
                ImVec2 tl = GetGridPos(x, y, true);
                ImVec2 br = GetGridPos(x, y, false);
                if (texture) {
                    ImGui::AddImageCropped(texture, tl, br);
                }
                if (click_to_drop && ImGui::IsMouseHoveringRect(tl, br) && ImGui::IsMouseReleased(0)) {
                    GW::Effects::DropBuff(buff.buff_id);
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
                    if (!bond_map.contains(skill_id)) {
                        continue;
                    }

                    const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                    if (!skill_data || skill_data->duration0 == 0x20000) {
                        continue; // Maintained skill/enchantment
                    }
                    const GW::Attribute* agentAttributes = GW::PartyMgr::GetAgentAttributes(agent_id);
                    assert(agentAttributes);
                    agentAttributes = &agentAttributes[static_cast<size_t>(skill_data->attribute)];
                    const bool overlay = effect.attribute_level < agentAttributes->level;

                    const size_t y = party_map[agent_id];
                    const size_t x = bond_map[skill_id];

                    const auto texture = *Resources::GetSkillImage(skill_id);
                    ImVec2 tl = GetGridPos(x, y, true);
                    ImVec2 br = GetGridPos(x, y, false);
                    if (texture) {
                        ImGui::AddImageCropped(texture, tl, br);
                    }
                    if (overlay) {
                        ImGui::GetWindowDrawList()->AddRectFilled(tl, br, low_attribute_overlay);
                    }
                }
            }
        }

        if (click_to_cast && !handled_click) {
            for (unsigned int y = 0; y < party_list.size(); ++y) {
                for (unsigned int x = 0; x < bond_list.size(); ++x) {
                    ImVec2 tl = GetGridPos(x, y, true);
                    ImVec2 br = GetGridPos(x, y, false);
                    if (!ImGui::IsMouseHoveringRect(tl, br)) {
                        continue;
                    }
                    ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
                    if (ImGui::IsMouseReleased(0)) {
                        UseBuff(party_list[y], bond_list[x]);
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(); // window bg
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
    LOAD_BOOL(snap_to_party_window);
    LOAD_UINT(row_height);
    LOAD_UINT(user_offset);

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
    SAVE_BOOL(snap_to_party_window);
    SAVE_UINT(row_height);
    SAVE_UINT(user_offset);

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
    ImGui::Checkbox("Attach to party window", &snap_to_party_window);
    if (snap_to_party_window) {
        ImGui::InputInt("Party window offset", &user_offset);
        ImGui::ShowHelp("Distance away from the party window");
    }
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
    if (!snap_to_party_window) {
        ImGui::InputInt("Row Height", &row_height);
        ImGui::ShowHelp("Height of each row, leave 0 for default");
    }
    if (row_height < 0) {
        row_height = 0;
    }
}
