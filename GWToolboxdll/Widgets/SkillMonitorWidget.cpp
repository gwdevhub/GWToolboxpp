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

#include <Defines.h>
#include <Modules/Resources.h>
#include <Widgets/SkillMonitorWidget.h>

namespace {

    enum SkillActivationStatus {
        CASTING,
        COMPLETED,
        CANCELLED,
        INTERRUPTED,
    };

    struct SkillActivation {
        GW::Constants::SkillID id;
        SkillActivationStatus status;
        clock_t last_update{};
        clock_t cast_start = last_update;
        float cast_time = .0f;
    };

    const float PARTY_OFFSET_LEFT_BASE = 15.f;
    const float PARTY_OFFSET_TOP_BASE = 31.f;
    const float PARTY_OFFSET_RIGHT_BASE = 14.f;
    const float PARTY_MEMBER_PADDING_FIXED = 1.f;
    const float PARTY_HERO_INDENT_BASE = 22.f;


    GW::HookEntry GenericFloat_Entry;

    GW::UI::WindowPosition* party_window_position = nullptr;

    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericModifier_Entry;

    std::unordered_map<GW::AgentID, std::vector<SkillActivation>> history{};
    std::unordered_map<GW::AgentID, float> casttime_map{};

    std::unordered_map<GW::AgentID, size_t> party_map{};
    std::unordered_map<GW::AgentID, bool> party_map_indent{};
    size_t allies_start = 255;

    bool hide_in_outpost = false;
    bool show_non_party_members = false;
    Color background = Colors::ARGB(76, 0, 0, 0);

    bool snap_to_party_window = true;
    int user_offset = -1;
    int row_height = GW::Constants::HealthbarHeight::Normal;

    bool history_flip_direction = false;

    int cast_indicator_threshold = 1000;
    int cast_indicator_height = 3;
    Color cast_indicator_color = Colors::ARGB(255, 55, 153, 30);

    int status_border_thickness = 2;
    Color status_color_completed = Colors::ARGB(255, 219, 157, 14);
    Color status_color_casting = Colors::ARGB(255, 55, 153, 30);
    Color status_color_cancelled = Colors::ARGB(255, 71, 24, 102);
    Color status_color_interrupted = Colors::ARGB(255, 71, 24, 102);

    int history_length = 5;
    int history_timeout = 5000;

    Color GetColor(const SkillActivationStatus status)
    {
        switch (status) {
        case CASTING:
            return status_color_casting;
        case COMPLETED:
            return status_color_completed;
        case CANCELLED:
            return status_color_cancelled;
        case INTERRUPTED:
            return status_color_interrupted;
        }
        return Colors::Empty();
    }

    bool FetchPartyInfo()
    {
        const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
        if (!info) {
            return false;
        }
        party_map.clear();
        party_map_indent.clear();
        allies_start = 255;
        for (const GW::PlayerPartyMember& player : info->players) {
            const auto id = GW::PlayerMgr::GetPlayerAgentId(player.login_number);
            if (!id) {
                continue;
            }
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
                const GW::AgentLiving* ally = agent ? agent->GetAsAgentLiving() : nullptr;
                if (ally && ally->allegiance != GW::Constants::Allegiance::Minion && ally->GetCanBeViewedInPartyWindow() && !ally->GetIsSpawned()) {
                    if (allies_start == 255) {
                        allies_start = party_map.size();
                    }
                    party_map[ally_id] = party_map.size();
                }
            }
        }
        return true;
    }

    void CasttimeCallback(const uint32_t value_id, const uint32_t caster_id, const float value)
    {
        if (value_id != GW::Packet::StoC::GenericValueID::casttime) {
            return;
        }

        casttime_map[caster_id] = value;
    }

    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t value)
    {
        if (!party_map.contains(caster_id)) {
            return;
        }
        using namespace GW::Packet::StoC;

        const auto skill_history = &history[caster_id];
        if (!skill_history) {
            return;
        }

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
        case GenericValueID::attack_skill_finished: {
            const auto casting = find_if(skill_history->begin(), skill_history->end(),
                [&](const SkillActivation& skill_activation) { return skill_activation.status == CASTING; });
            if (casting == skill_history->end()) {
                break;
            }
            casting->status = value_id == GenericValueID::skill_stopped
                ? CANCELLED
                : COMPLETED;
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

    std::unordered_map<uint32_t, GW::HookEntry*> packet_hooks;

    void OnStoCPacket(GW::HookStatus* status, GW::Packet::StoC::PacketBase* base) {
        if (status->blocked)
            return;
        switch (base->header) {
        case GW::Packet::StoC::InstanceLoadInfo::STATIC_HEADER: {
            history.clear();
            casttime_map.clear();
        } break;

        case GW::Packet::StoC::GenericModifier::STATIC_HEADER: {
            const auto packet = (GW::Packet::StoC::GenericModifier*)base;
            CasttimeCallback(packet->type, packet->target_id, packet->value);
        } break;

        case GW::Packet::StoC::GenericFloat::STATIC_HEADER: {
            const auto packet = (GW::Packet::StoC::GenericFloat*)base;
            CasttimeCallback(packet->type, packet->agent_id, packet->value);
        } break;

        case GW::Packet::StoC::GenericValue::STATIC_HEADER: {
            const auto packet = (GW::Packet::StoC::GenericValue*)base;
            SkillCallback(packet->value_id, packet->agent_id, packet->value);
        } break;

        case GW::Packet::StoC::GenericValueTarget::STATIC_HEADER: {
            using namespace GW::Packet::StoC::GenericValueID;

            const auto packet = (GW::Packet::StoC::GenericValueTarget*)base;
            const auto value_id = packet->Value_id;
            const auto caster_id = packet->caster;
            const auto target_id = packet->target;

            const bool isSwapped = value_id == skill_activated || value_id == attack_skill_activated;
            SkillCallback(value_id, isSwapped ? target_id : caster_id, packet->value);
        } break;

        }
    }

}

void SkillMonitorWidget::Initialize()
{
    ToolboxWidget::Initialize();
    party_window_position = GetWindowPosition(GW::UI::WindowID_PartyWindow);

    uint32_t packet_headers_to_hook[] = {
        GW::Packet::StoC::GenericModifier::STATIC_HEADER,
        GW::Packet::StoC::GenericFloat::STATIC_HEADER,
        GW::Packet::StoC::GenericValue::STATIC_HEADER,
        GW::Packet::StoC::GenericValueTarget::STATIC_HEADER,
        GW::Packet::StoC::InstanceLoadInfo::STATIC_HEADER
    };

    for (const auto header : packet_headers_to_hook) {
        const auto entry = new GW::HookEntry;
        packet_hooks[header] = entry;
        GW::StoC::RegisterPostPacketCallback(entry, header, OnStoCPacket);
    }
}

void SkillMonitorWidget::Terminate()
{
    ToolboxWidget::Terminate();
    for (const auto& it : packet_hooks) {
        GW::StoC::RemoveCallback(it.first, it.second);
        delete it.second;
    }
    packet_hooks.clear();
}

void SkillMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    // @Cleanup: This doesn't need to be done every frame - only when the party has been changed
    if (!FetchPartyInfo()) {
        return;
    }

    const float img_size = row_height > 0 && !snap_to_party_window ? row_height : GuiUtils::GetPartyHealthbarHeight();
    const auto num_rows = show_non_party_members
                              ? party_map.size() + (allies_start < 255 ? 1 : 0)
                              : GW::PartyMgr::GetPartySize();
    const float height = num_rows * img_size;
    const float width = static_cast<float>(history_length) * img_size;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);

    const float uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
    const auto calculate_window_position = [uiscale_multiply]() -> auto {
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
        return rect;
    };

    if (snap_to_party_window && party_window_position) {
        GW::Vec2f internal_offset(7.f, GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? 31.f : 34.f);
        internal_offset *= uiscale_multiply;
        const auto user_offset_x = abs(user_offset);
        float offset_width = width;
        const auto rect = calculate_window_position();
        auto calculated_pos = ImVec2(rect.x + internal_offset.x - user_offset_x - offset_width, rect.y + internal_offset.y);
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
            return {win_x + x * img_size, win_y + y * img_size};
        };

        auto party_index = 0u;
        for (auto& [agent_id, party_slot] : party_map) {
            if (++party_index > num_rows) {
                continue;
            }
            auto& skill_history = history[agent_id];
            const size_t y = party_slot;

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
                        const auto calculated_pos = calculate_window_position();
                        GW::Vec2f xPartyWindow = { calculated_pos.x, calculated_pos.z };
                        xPartyWindow.x += PARTY_OFFSET_LEFT_BASE * uiscale_multiply;
                        xPartyWindow.y -= PARTY_OFFSET_RIGHT_BASE * uiscale_multiply;
                        GW::Vec2f yPartyWindow = {calculated_pos.y, calculated_pos.w};
                        yPartyWindow.x += PARTY_OFFSET_TOP_BASE * uiscale_multiply;

                        ImVec2 member_topleft(xPartyWindow.x, yPartyWindow.x + y * GuiUtils::GetPartyHealthbarHeight());

                        if (party_map_indent[agent_id]) {
                            member_topleft.x += PARTY_HERO_INDENT_BASE * uiscale_multiply;
                        }

                        ImVec2 member_bottomright(xPartyWindow.y,
                                         member_topleft.y + GuiUtils::GetPartyHealthbarHeight() - PARTY_MEMBER_PADDING_FIXED);

                        ImGui::PushClipRect(member_topleft, member_bottomright, false);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(member_topleft.x, member_bottomright.y - cast_indicator_height),
                            ImVec2(member_topleft.x + (member_bottomright.x - member_topleft.x) * percentageCast, member_bottomright.y),
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

void SkillMonitorWidget::Update(const float)
{
    for (auto& skill_history : history | std::views::values) {
        if (skill_history.size() > static_cast<size_t>(history_length)) {
            skill_history.erase(skill_history.begin(), skill_history.begin() + (skill_history.size() - history_length));
        }

        if (history_timeout != 0) {
            std::erase_if(
                skill_history,
                [&](const SkillActivation& skill_activation) -> bool {
                    return TIMER_DIFF(skill_activation.last_update) > history_timeout;
                });
        }
    }
}

void SkillMonitorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(hide_in_outpost);
    LOAD_BOOL(show_non_party_members);

    LOAD_BOOL(snap_to_party_window);
    LOAD_UINT(user_offset);
    LOAD_UINT(row_height);
    LOAD_BOOL(history_flip_direction);

    LOAD_UINT(cast_indicator_threshold);
    LOAD_UINT(cast_indicator_height);
    LOAD_COLOR(cast_indicator_color);

    LOAD_UINT(status_border_thickness);
    LOAD_COLOR(status_color_completed);
    LOAD_COLOR(status_color_casting);
    LOAD_COLOR(status_color_cancelled);
    LOAD_COLOR(status_color_interrupted);

    LOAD_UINT(history_length);
    LOAD_UINT(history_timeout);
}

void SkillMonitorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(show_non_party_members);

    SAVE_BOOL(snap_to_party_window);
    SAVE_UINT(user_offset);
    SAVE_UINT(row_height);
    SAVE_BOOL(history_flip_direction);

    SAVE_UINT(cast_indicator_threshold);
    SAVE_UINT(cast_indicator_height);
    SAVE_COLOR(cast_indicator_color);

    SAVE_UINT(status_border_thickness);
    SAVE_COLOR(status_color_completed);
    SAVE_COLOR(status_color_casting);
    SAVE_COLOR(status_color_cancelled);
    SAVE_COLOR(status_color_interrupted);

    SAVE_UINT(history_length);
    SAVE_UINT(history_timeout);
}

void SkillMonitorWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::Checkbox("Show non party-members (allies)", &show_non_party_members);
    ImGui::Checkbox("Attach to party window", &snap_to_party_window);
    if (snap_to_party_window) {
        ImGui::InputInt("Party window offset", &user_offset);
        ImGui::ShowHelp("Distance away from the party window");
    }
    else {
        ImGui::InputInt("Row Height", &row_height);
        ImGui::ShowHelp("Height of each row, leave 0 for default");
    }
    if (row_height < 0) {
        row_height = 0;
    }

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


