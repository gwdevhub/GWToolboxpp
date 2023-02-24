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
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Widgets/BondsWidget.h>

void BondsWidget::Initialize() {
    ToolboxWidget::Initialize();
    party_window_position = GW::UI::GetWindowPosition(GW::UI::WindowID_PartyWindow);
}

void BondsWidget::Terminate() {
    ToolboxWidget::Terminate();
}

void BondsWidget::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (!visible)
        return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    const GW::PlayerArray* players = info ? GW::Agents::GetPlayerArray() : nullptr;
    if (!players) return;
    // note: info->heroes, ->henchmen, and ->others CAN be invalid during normal use.

    // ==== Get bonds ====
    // @Cleanup: This doesn't need to be done every frame - only when player skills have changed
    if (!FetchBondSkills())
        return;
    if (bond_list.empty())
        return; // Don't display bonds widget if we've not got any bonds on our skillbar
    // ==== Get party ====
    // @Cleanup: This doesn't need to be done every frame - only when the party has been changed
    if (!FetchPartyInfo())
        return;

    // ==== Draw ====
    const float img_size = row_height > 0 && !snap_to_party_window ? row_height : GuiUtils::GetPartyHealthbarHeight();
    const float height = (party_list.size() + (allies_start < 255 ? 1 : 0)) * img_size;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);

    float width = bond_list.size() * img_size;
    if (snap_to_party_window && party_window_position) {
        float uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
        // NB: Use case to define GW::Vec4f ?
        GW::Vec2f x = party_window_position->xAxis();
        GW::Vec2f y = party_window_position->yAxis();
        // Do the uiscale multiplier
        x *= uiscale_multiply;
        y *= uiscale_multiply;
        // Clamp
        ImVec4 rect(x.x, y.x, x.y, y.y);
        ImVec4 viewport(0, 0, (float)GW::Render::GetViewportWidth(), (float)GW::Render::GetViewportHeight());
        // GW Clamps windows to viewport; we need to do the same.
        GuiUtils::ClampRect(rect, viewport);
        // Left placement
        GW::Vec2f internal_offset(
            7.f,
            GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? 31.f : 34.f
        );
        internal_offset *= uiscale_multiply;
        int user_offset_x = abs(user_offset);
        float offset_width = width;
        ImVec2 calculated_pos = ImVec2(rect.x + internal_offset.x - user_offset_x - offset_width, rect.y + internal_offset.y);
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
        float win_x = ImGui::GetWindowPos().x;
        float win_y = ImGui::GetWindowPos().y;

        auto GetGridPos = [&](const size_t _x, const size_t _y, bool topleft) -> ImVec2 {
            size_t x = _x;
            size_t y = _y;
            if (y >= allies_start) ++y;
            if (!topleft) {
                ++x; ++y;
            }
            return ImVec2(win_x + x * img_size,
                win_y + y * img_size);
        };

        bool handled_click = false;

        if (GW::BuffArray* buffs = GW::Effects::GetPlayerBuffs()) {
            for (const auto& buff : *buffs) {
                const auto agent = buff.target_agent_id;
                const auto skill = static_cast<GW::Constants::SkillID>(buff.skill_id);
                if (!party_map.contains(agent))
                    continue; // bond target not in party
                if (!bond_map.contains(skill))
                    continue; // bond with a skill not in skillbar
                size_t y = party_map[agent];
                size_t x = bond_map[skill];
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
        if (const GW::AgentEffectsArray* agent_effects_array = GW::Effects::GetPartyEffectsArray();
            agent_effects_array != nullptr) {
            for (auto& agent_effects_it : *agent_effects_array) {
                auto& agent_effects = agent_effects_it.effects;
                if (!agent_effects.valid()) continue;
                const auto agent_id = agent_effects_it.agent_id;
                for (const GW::Effect& effect : agent_effects) {
                    const auto skill_id = static_cast<GW::Constants::SkillID>(effect.skill_id);
                    if (!bond_map.contains(skill_id)) continue;
                    
                    const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                    if (!skill_data || skill_data->duration0 == 0x20000) continue; // Maintained skill/enchantment
                    const GW::Attribute* agentAttributes = GW::PartyMgr::GetAgentAttributes(agent_id);
                    assert(agentAttributes);
                    agentAttributes = &agentAttributes[skill_data->attribute];
                    const bool overlay = effect.attribute_level < agentAttributes->level;

                    size_t y = party_map[agent_id];
                    size_t x = bond_map[skill_id];

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
                    if (!ImGui::IsMouseHoveringRect(tl, br))
                        continue;
                    ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
                    if (ImGui::IsMouseReleased(0))
                        UseBuff(party_list[y], (DWORD) bond_list[x]);
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(); // window bg
    ImGui::PopStyleVar(3);
}

void BondsWidget::UseBuff(GW::AgentID targetId, DWORD buff_skillid) {
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
    if (GW::Map::GetIsObserving())
        return;
    if (targetId == 0) return;

    GW::Agent* target = GW::Agents::GetAgentByID(targetId);
    if (target == nullptr) return;

    int islot = GW::SkillbarMgr::GetSkillSlot((GW::Constants::SkillID)buff_skillid);
    if (islot < 0) return;
    uint32_t slot = static_cast<uint32_t>(islot);
    GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) return;
    if (skillbar->skills[slot].recharge != 0) return;

    // capture by value!
    GW::GameThread::Enqueue([slot, targetId]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}

void BondsWidget::LoadSettings(ToolboxIni* ini) {
    ToolboxWidget::LoadSettings(ini);
    lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), true);

    background = Colors::Load(ini, Name(), VAR_NAME(background), Colors::ARGB(76, 0, 0, 0));
    click_to_cast = ini->GetBoolValue(Name(), VAR_NAME(click_to_cast), click_to_cast);
    click_to_drop = ini->GetBoolValue(Name(), VAR_NAME(click_to_drop), click_to_drop);
    show_allies = ini->GetBoolValue(Name(), VAR_NAME(show_allies), show_allies);
    flip_bonds = ini->GetBoolValue(Name(), VAR_NAME(flip_bonds), flip_bonds);
    row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), row_height);
    low_attribute_overlay = Colors::Load(ini, Name(), VAR_NAME(low_attribute_overlay), Colors::ARGB(76, 0, 0, 0));
    hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    snap_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(snap_to_party_window), snap_to_party_window);
    user_offset = ini->GetLongValue(Name(), VAR_NAME(user_offset), user_offset);
}

void BondsWidget::SaveSettings(ToolboxIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
    Colors::Save(ini, Name(), VAR_NAME(background), background);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_cast), click_to_cast);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_drop), click_to_drop);
    ini->SetBoolValue(Name(), VAR_NAME(show_allies), show_allies);
    ini->SetBoolValue(Name(), VAR_NAME(flip_bonds), flip_bonds);
    ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
    Colors::Save(ini, Name(), VAR_NAME(low_attribute_overlay), low_attribute_overlay);
    ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(snap_to_party_window), snap_to_party_window);
    ini->SetLongValue(Name(), VAR_NAME(user_offset), user_offset);
}

void BondsWidget::DrawSettingInternal() {
    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    if (bond_list.empty())
        ImGui::TextColored(ImVec4(0xFF, 0, 0, 0xFF), "Equip a maintainable enchantment or refrain to show bonds widget on-screen");
    ImGui::Checkbox("Attach to party window", &snap_to_party_window);
    if (snap_to_party_window) {
        ImGui::InputInt("Party window offset", &user_offset);
        ImGui::ShowHelp("Distance away from the party window");
    }
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
    if (row_height < 0) row_height = 0;
}

bool BondsWidget::FetchBondSkills()
{
    const GW::Skillbar *bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid())
        return false;
    bond_list.clear();
    bond_map.clear();
    for (const auto& skill : bar->skills) {
        auto skill_id = static_cast<GW::Constants::SkillID>(skill.skill_id);
        if (std::ranges::find(skills, skill_id) != skills.end()) {
            bond_map[skill_id] = bond_list.size();
            bond_list.push_back(skill_id);
        }
    }
    return true;
}
bool BondsWidget::FetchPartyInfo()
{
    const GW::PartyInfo *info = GW::PartyMgr::GetPartyInfo();
    if (!info)
        return false;
    party_list.clear();
    party_map.clear();
    allies_start = 255;
    for (const GW::PlayerPartyMember &player : info->players) {
        DWORD id = GW::PlayerMgr::GetPlayerAgentId(player.login_number);
        if (!id) continue;
        party_map[id] = party_list.size();
        party_list.push_back(id);

        if (info->heroes.valid()) {
            for (const GW::HeroPartyMember &hero : info->heroes) {
                if (hero.owner_player_id == player.login_number) {
                    party_map[hero.agent_id] = party_list.size();
                    party_list.push_back(hero.agent_id);
                }
            }
        }
    }
    if (info->henchmen.valid()) {
        for (const GW::HenchmanPartyMember &hench : info->henchmen) {
            party_list.push_back(hench.agent_id);
        }
    }
    if (show_allies && info->others.valid()) {
        for (const DWORD ally_id : info->others) {
            GW::Agent* agent = GW::Agents::GetAgentByID(ally_id);
            const GW::AgentLiving* ally = agent ? agent->GetAsAgentLiving() : nullptr;
            if (ally && ally->allegiance != GW::Constants::Allegiance::Minion && ally->GetCanBeViewedInPartyWindow() && !ally->GetIsSpawned()) {
                if (allies_start == 255)
                    allies_start = party_map.size();
                party_map[ally_id] = party_map.size();
            }
        }
    }
    return true;
}
