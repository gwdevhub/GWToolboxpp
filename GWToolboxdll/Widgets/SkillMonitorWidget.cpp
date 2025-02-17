#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
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

    std::unordered_map<GW::AgentID, std::vector<SkillActivation>> history{};
    std::unordered_map<GW::AgentID, float> casttime_map{};

    bool hide_in_outpost = false;
    bool show_non_party_members = false;
    Color background = Colors::ARGB(76, 0, 0, 0);

    int user_offset = -1;

    bool history_flip_direction = false;

    int cast_indicator_threshold = 1000;
    int cast_indicator_height = 3;
    Color cast_indicator_color = Colors::ARGB(255, 55, 153, 30);

    int status_border_thickness = 2;
    Color status_color_completed = Colors::ARGB(255, 219, 157, 14);
    Color status_color_casting = Colors::ARGB(255, 55, 153, 30);
    Color status_color_cancelled = Colors::ARGB(255, 71, 24, 102);
    Color status_color_interrupted = Colors::ARGB(255, 71, 24, 102);

    bool overlay_party_window = false;

    int history_length = 5;
    int history_timeout = 5000;
    std::unordered_map<uint32_t, GW::HookEntry*> packet_hooks;

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

    void CasttimeCallback(const uint32_t value_id, const uint32_t caster_id, const float value)
    {
        if (value_id != GW::Packet::StoC::GenericValueID::casttime) {
            return;
        }

        casttime_map[caster_id] = value;
    }

}

void SkillMonitorWidget::OnStoCPacket(GW::HookStatus* status, GW::Packet::StoC::PacketBase* base) {
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
void SkillMonitorWidget::SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t value)
{
    if (!party_indeces_by_agent_id.contains(caster_id))
        return;
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

void SkillMonitorWidget::Initialize()
{
    SnapsToPartyWindow::Initialize();

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
    SnapsToPartyWindow::Terminate();
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
    // @Cleanup: Only call when the party window has been moved or updated
    if (!(FetchPartyInfo() && RecalculatePartyPositions())) {
        return;
    }

    if (agent_health_bar_positions.empty()) {
        return;
    }

    const auto& first_agent_health_bar = agent_health_bar_positions.begin()->second;
    const auto img_size = first_agent_health_bar.bottom_right.y - first_agent_health_bar.top_left.y;
    const auto width = img_size * history_length;

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
    ImGui::SetNextWindowPos({ window_x, party_health_bars_position.top_left.y });
    ImGui::SetNextWindowSize({ width, party_health_bars_position.bottom_right.y - party_health_bars_position.top_left.y });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);

    if (ImGui::Begin(Name(), &visible, GetWinFlags())) {
        const auto draw_list = ImGui::GetWindowDrawList();

        for (auto& [agent_id, party_slot] : party_indeces_by_agent_id) {

            if (party_slot >= pets_start_idx && party_slot < allies_start_idx)
                continue; // Don't draw pets
            if (!show_non_party_members && party_slot >= allies_start_idx)
                continue; // Don't draw allies

            const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
            if (!health_bar_pos)
                continue;
            draw_list->AddRectFilled({ window_x , health_bar_pos->top_left.y }, { window_x + width, health_bar_pos->bottom_right.y }, background);

            auto& skill_history = history[agent_id];
            for (size_t i = 0; i < skill_history.size(); i++) {
                const auto& skill_activation = skill_history.at(i);
                const ImVec2 top_left = {history_flip_direction ? window_x + (i * img_size) : window_x + width - (i * img_size) - img_size, health_bar_pos->top_left.y};
                const ImVec2 bottom_right = {top_left.x + img_size, top_left.y + img_size};

                const auto texture = *Resources::GetSkillImage(skill_activation.id);
                if (texture) {
                    draw_list->AddImage(texture, top_left, bottom_right);
                }

                if (status_border_thickness != 0) {
                    draw_list->AddRect(top_left, bottom_right, GetColor(skill_activation.status), 0.f,
                        ImDrawFlags_RoundCornersNone, static_cast<float>(status_border_thickness));

                }

                if (skill_activation.status == CASTING 
                    && skill_activation.cast_time * 1000 >= cast_indicator_threshold
                    && ImGui::ColorConvertU32ToFloat4(cast_indicator_color).w != 0) {
                    const auto remaining_cast = TIMER_DIFF(skill_activation.cast_start);
                    const auto percentage_cast = std::min(remaining_cast / (skill_activation.cast_time * 1000), 1.0f);

                    const auto health_bar_width = health_bar_pos->bottom_right.x - health_bar_pos->top_left.x;
                    ImGui::GetBackgroundDrawList()->AddRectFilled(
                        ImVec2(health_bar_pos->top_left.x, health_bar_pos->bottom_right.y - cast_indicator_height),
                        ImVec2(health_bar_pos->top_left.x + (health_bar_width * percentage_cast), health_bar_pos->bottom_right.y),
                        cast_indicator_color);
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

    LOAD_UINT(user_offset);
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

    LOAD_BOOL(overlay_party_window);

    LOAD_COLOR(background);
}

void SkillMonitorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(show_non_party_members);

    SAVE_UINT(user_offset);
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

    SAVE_BOOL(overlay_party_window);

    SAVE_COLOR(background);
}

void SkillMonitorWidget::DrawSettingsInternal()
{
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show non party-members (allies)", &show_non_party_members);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Flip history direction (left/right)", &history_flip_direction);
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show on top of health bars", &overlay_party_window);
    ImGui::ShowHelp("Untick to show this widget to the left (or right) of the party window.\nTick to show this widget over the top of the party health bars inside the party window");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("Party window offset", &user_offset);
    ImGui::PopItemWidth();
    ImGui::ShowHelp("Distance away from the party window");

    ImGui::Text("Cast Indicator");
    ImGui::DragInt("Threshold", &cast_indicator_threshold, 1.0f, 0, 0, "%d milliseconds");
    ImGui::ShowHelp(
        "Minimum cast time a skill has to have to display the indicator. Note that instantly casted skill will never be displayed.");
    ImGui::InputInt("Height", &cast_indicator_height);
    Colors::DrawSettingHueWheel("Color", &cast_indicator_color);

    Colors::DrawSettingHueWheel("Background", &background, 0);

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


