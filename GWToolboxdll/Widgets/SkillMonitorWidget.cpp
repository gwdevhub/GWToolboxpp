#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Modules/Resources.h>
#include <Widgets/SkillMonitorWidget.h>

void SkillMonitorWidget::Initialize() {
    ToolboxWidget::Initialize();
    party_window_position = GW::UI::GetWindowPosition(GW::UI::WindowID_PartyWindow);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Entry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadInfo*) {
            history.clear();
            casttime_map.clear();
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &GenericModifier_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericModifier* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            CasttimeCallback(packet->type, packet->target_id, packet->value);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const uint32_t value_id = packet->value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t value = packet->value;

            SkillCallback(value_id, caster_id, value);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            using namespace GW::Packet::StoC::GenericValueID;

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;

            const bool isSwapped = value_id == skill_activated || value_id == attack_skill_activated;
            SkillCallback(value_id, isSwapped ? target_id : caster_id, value);
        });
}

void SkillMonitorWidget::Terminate() {
	ToolboxWidget::Terminate();
}

void SkillMonitorWidget::Draw(IDirect3DDevice9* device) {
	UNREFERENCED_PARAMETER(device);
    if (!visible)
        return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
    // @Cleanup: This doesn't need to be done every frame - only when the party has been changed
    if (!FetchPartyInfo()) return;

    const float img_size = row_height > 0 && !snap_to_party_window ? row_height : GuiUtils::GetPartyHealthbarHeight();
    const auto num_rows = show_non_party_members ?
                                                   party_map.size() + (allies_start < 255 ? 1 : 0)
                                                 : GW::PartyMgr::GetPartySize();
    const float height = num_rows * img_size;
    const float width = static_cast<float>(history_length) * img_size;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);

    if (snap_to_party_window && party_window_position) {
        const float uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
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
            7.f, GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? 31.f : 34.f);
        internal_offset *= uiscale_multiply;
        int user_offset_x = abs(user_offset);
        float offset_width = width;
        ImVec2 calculated_pos =
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
    if (ImGui::Begin(Name(), &visible, GetWinFlags(0))) {
        const float win_x = ImGui::GetWindowPos().x;
        const float win_y = ImGui::GetWindowPos().y;
        auto GetGridPos = [&](const size_t _x, const size_t _y, bool topleft) -> ImVec2 {
            size_t x = _x;
            size_t y = _y;
            if (y >= allies_start) ++y;
            if (!topleft) {
                ++x;
                ++y;
            }
            return {win_x + x * img_size, win_y + y * img_size};
        };
        
        auto party_index = 0u;
        for (auto& [agent_id, party_slot] : party_map) {
            if (++party_index > num_rows) continue;
            auto& skill_history = history[agent_id];
            size_t y = party_slot;

            for (size_t i = 0; i < skill_history.size(); i++) {
                const auto& skill_activation = skill_history.at(i);
                const auto xIndex = history_flip_direction ? history_length - skill_history.size() + i : skill_history.size() - 1 - i;

                const auto texture = *Resources::GetSkillImage(skill_activation.id);
                ImVec2 tl = GetGridPos(xIndex, y, true);
                ImVec2 br = GetGridPos(xIndex, y, false);

                if (texture) {
                    ImGui::GetWindowDrawList()->AddImage(texture, tl, br);
                }

                if (status_border_thickness != 0) {
                    ImGui::PushClipRect(tl, br, true);
                    ImGui::GetWindowDrawList()->AddRect(tl, br, GetColor(skill_activation.status), 0.f,
                        ImDrawFlags_RoundCornersNone, static_cast<float>(status_border_thickness));
                    ImGui::PopClipRect();
                }
                
                if (ImGui::ColorConvertU32ToFloat4(cast_indicator_color).w != 0) {
                    if (skill_activation.status == CASTING && skill_activation.cast_time * 1000 >= cast_indicator_threshold) {
                        const auto remainingCast = TIMER_DIFF(skill_activation.cast_start);
                        const auto percentageCast = std::min(remainingCast / (skill_activation.cast_time * 1000), 1.0f);
                        const auto uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
                        GW::Vec2f xPartyWindow = party_window_position->xAxis() * uiscale_multiply;
                        xPartyWindow.x += PARTY_OFFSET_LEFT_BASE * uiscale_multiply;
                        xPartyWindow.y -= PARTY_OFFSET_RIGHT_BASE * uiscale_multiply;
                        GW::Vec2f yPartyWindow = party_window_position->yAxis() * uiscale_multiply;
                        yPartyWindow.x += PARTY_OFFSET_TOP_BASE * uiscale_multiply;

                        ImVec2 member_tl(xPartyWindow.x, yPartyWindow.x + y * (GuiUtils::GetPartyHealthbarHeight()));

                        if (party_map_indent[agent_id]) {
                            member_tl.x += PARTY_HERO_INDENT_BASE * uiscale_multiply;
                        }

                        ImVec2 member_br(xPartyWindow.y,
                            member_tl.y + GuiUtils::GetPartyHealthbarHeight() - PARTY_MEMBER_PADDING_FIXED);

                        ImGui::PushClipRect(member_tl, member_br, false);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(member_tl.x, member_br.y - cast_indicator_height),
                            ImVec2(member_tl.x + (member_br.x - member_tl.x) * percentageCast, member_br.y),
                            cast_indicator_color);
                        ImGui::PopClipRect();
                    }
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void SkillMonitorWidget::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    for (auto& [agent_id, skill_history] : history) {
        if (skill_history.size() > static_cast<size_t>(history_length)) {
            skill_history.erase(skill_history.begin(), skill_history.begin() + (skill_history.size() - history_length));
        }

        if (history_timeout != 0) {
            skill_history.erase(
                std::remove_if(
                    skill_history.begin(),
                    skill_history.end(),
                    [&](const SkillActivation& skill_activation) -> bool {
                        return TIMER_DIFF(skill_activation.last_update) > history_timeout;
                    }),
                skill_history.end()
            );
        }
    }
}

void SkillMonitorWidget::LoadSettings(CSimpleIni* ini) {
    ToolboxWidget::LoadSettings(ini);
    hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    show_non_party_members = ini->GetBoolValue(Name(), VAR_NAME(show_non_party_members), show_non_party_members);

    snap_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(snap_to_party_window), snap_to_party_window);
    user_offset = ini->GetLongValue(Name(), VAR_NAME(user_offset), user_offset);
    row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), row_height);
    history_flip_direction = ini->GetBoolValue(Name(), VAR_NAME(history_flip_direction), history_flip_direction);

    cast_indicator_threshold = ini->GetLongValue(Name(), VAR_NAME(cast_indicator_threshold), cast_indicator_threshold);
    cast_indicator_height = ini->GetLongValue(Name(), VAR_NAME(cast_indicator_height), cast_indicator_height);
    cast_indicator_color = Colors::Load(ini, Name(), VAR_NAME(cast_indicator_color), cast_indicator_color);

    status_border_thickness = ini->GetLongValue(Name(), VAR_NAME(status_border_thickness), status_border_thickness);
    status_color_completed = Colors::Load(ini, Name(), VAR_NAME(status_color_completed), status_color_completed);
    status_color_casting = Colors::Load(ini, Name(), VAR_NAME(status_color_casting), status_color_casting);
    status_color_cancelled = Colors::Load(ini, Name(), VAR_NAME(status_color_cancelled), status_color_cancelled);
    status_color_interrupted = Colors::Load(ini, Name(), VAR_NAME(status_color_interrupted), status_color_interrupted);

    history_length = ini->GetLongValue(Name(), VAR_NAME(history_length), history_length);
    history_timeout = ini->GetLongValue(Name(), VAR_NAME(history_timeout), history_timeout);
}

void SkillMonitorWidget::SaveSettings(CSimpleIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(show_non_party_members), show_non_party_members);

    ini->SetBoolValue(Name(), VAR_NAME(snap_to_party_window), snap_to_party_window);
    ini->SetLongValue(Name(), VAR_NAME(user_offset), user_offset);
    ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
    ini->SetBoolValue(Name(), VAR_NAME(history_flip_direction), history_flip_direction);

    ini->SetLongValue(Name(), VAR_NAME(cast_indicator_threshold), cast_indicator_threshold);
    ini->SetLongValue(Name(), VAR_NAME(cast_indicator_height), cast_indicator_height);
    Colors::Save(ini, Name(), VAR_NAME(cast_indicator_color), cast_indicator_color);

    ini->SetLongValue(Name(), VAR_NAME(status_border_thickness), status_border_thickness);
    Colors::Save(ini, Name(), VAR_NAME(status_color_completed), status_color_completed);
    Colors::Save(ini, Name(), VAR_NAME(status_color_casting), status_color_casting);
    Colors::Save(ini, Name(), VAR_NAME(status_color_cancelled), status_color_cancelled);
    Colors::Save(ini, Name(), VAR_NAME(status_color_interrupted), status_color_interrupted);

    ini->SetLongValue(Name(), VAR_NAME(history_length), history_length);
    ini->SetDoubleValue(Name(), VAR_NAME(history_timeout), history_timeout);
}

void SkillMonitorWidget::DrawSettingInternal() {
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::Checkbox("Show non party-members (allies)", &show_non_party_members);
    ImGui::Checkbox("Attach to party window", &snap_to_party_window);
    if (snap_to_party_window) {
        ImGui::InputInt("Party window offset", &user_offset);
        ImGui::ShowHelp("Distance away from the party window");
    } else {
        ImGui::InputInt("Row Height", &row_height);
        ImGui::ShowHelp("Height of each row, leave 0 for default");
    }
    if (row_height < 0) row_height = 0;

    ImGui::Checkbox("Flip history direction (left/right)", &history_flip_direction);

    ImGui::Text("Cast Indicator");
    ImGui::DragInt("Threshold", &cast_indicator_threshold, 1.0f, 0, 0, "%d milliseconds");
    ImGui::ShowHelp(
        "Minimum cast time a skill has to have to display the indicator. Note that instantly casted skill will never be displayed.");
    ImGui::InputInt("Height", &cast_indicator_height);
    Colors::DrawSettingHueWheel("Color", &cast_indicator_color);

    ImGui::Text("Status Border");
    ImGui::InputInt("Border Thickness", &status_border_thickness);
    ImGui::ShowHelp("Set to 0 to disable.");
    if (status_border_thickness < 0) {
        status_border_thickness = 0;
    }
    if (status_border_thickness != 0) {
        if (ImGui::TreeNodeEx("Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            Colors::DrawSettingHueWheel("Completed", &status_color_completed);
            Colors::DrawSettingHueWheel("Casting", &status_color_casting);
            Colors::DrawSettingHueWheel("Cancelled", &status_color_cancelled);
            Colors::DrawSettingHueWheel("Interrupted", &status_color_interrupted);
            ImGui::TreePop();
        }
    }

    
    ImGui::Text("History");
    ImGui::InputInt("Length", &history_length, 0, 25);
    if (history_length < 0) {
        history_length = 0;
    }
    ImGui::DragInt("Timeout", &history_timeout, 1.0f, 0, 0, "%d milliseconds");
    ImGui::ShowHelp("Amount of time after which a skill gets removed from the skill history. Set to 0 to disable.");
    if (history_timeout < 0) {
        history_timeout = 0;
    }
}

void SkillMonitorWidget::SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t value) {
    if (!party_map.count(caster_id)) {
        return;
    }
    using namespace GW::Packet::StoC;

    const auto skill_history = &history[caster_id];
    if (!skill_history) return;

    switch (value_id) {
        case GenericValueID::instant_skill_activated:
        case GenericValueID::attack_skill_activated:
        case GenericValueID::skill_activated: {
            float casttime = casttime_map[caster_id];
            const bool is_instant = value_id == GenericValueID::instant_skill_activated;
            if (!is_instant && !casttime) {
                if (const auto skill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(value))) {
                    casttime = skill->activation;
                }
            }

            skill_history->push_back({
                static_cast<GW::Constants::SkillID>(value),
                is_instant ? COMPLETED : CASTING,
                TIMER_INIT(),
                TIMER_INIT(),
                casttime,
            });

            casttime_map.erase(caster_id);
            break;
        }
        case GenericValueID::skill_stopped:
        case GenericValueID::skill_finished:
        case GenericValueID::attack_skill_finished:
        {
            const auto casting = find_if(skill_history->begin(), skill_history->end(),
                                         [&](const SkillActivation& skill_activation) { return skill_activation.status == CASTING; });
            if (casting == skill_history->end()) {
                break;
            }
            casting->status = value_id == GenericValueID::skill_stopped
                                  ? CANCELLED
                                  : COMPLETED;
            ;
            casting->last_update = TIMER_INIT();
            break;
        }
        case GenericValueID::interrupted: {
            const auto cancelled = find_if(skill_history->begin(), skill_history->end(),
                                           [&](const SkillActivation& skill_activation) { return skill_activation.status == CANCELLED; });
            if (cancelled == skill_history->end()) {
                break;
            }

            cancelled->status = INTERRUPTED;
            cancelled->last_update = TIMER_INIT();
            break;
        }
        default:
            return;
    }
}

void SkillMonitorWidget::CasttimeCallback(const uint32_t value_id, const uint32_t caster_id, const float value) {
    if (value_id != GW::Packet::StoC::GenericValueID::casttime) return;

    casttime_map[caster_id] = value;
}

bool SkillMonitorWidget::FetchPartyInfo() {
    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (!info) return false;
    party_map.clear();
    party_map_indent.clear();
    allies_start = 255;
    for (const GW::PlayerPartyMember& player : info->players) {
        const auto id = GW::PlayerMgr::GetPlayerAgentId(player.login_number);
        if (!id) continue;
        party_map[id] = party_map.size();

        if (info->heroes.valid()) {
            for (const GW::HeroPartyMember& hero : info->heroes) {
                if (hero.owner_player_id == player.login_number) {
                    party_map[hero.agent_id] = party_map.size();
                    party_map_indent[hero.agent_id] = true;
                }
            }
        }
    }
    if (info->henchmen.valid()) {
        for (const GW::HenchmanPartyMember& hench : info->henchmen) {
            party_map[hench.agent_id] = party_map.size();
        }
    }
    if (info->others.valid()) {
        for (const DWORD ally_id : info->others) {
            GW::Agent* agent = GW::Agents::GetAgentByID(ally_id);
            GW::AgentLiving* ally = agent ? agent->GetAsAgentLiving() : nullptr;
            if (ally && ally->allegiance != GW::Constants::Allegiance::Minion && ally->GetCanBeViewedInPartyWindow() && !ally->GetIsSpawned()) {
                if(allies_start == 255)
                    allies_start = party_map.size();
                party_map[ally_id] = party_map.size();
            }
        }
    }
    return true;
}
