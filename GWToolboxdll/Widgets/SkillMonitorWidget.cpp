#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hook.h>

#include <Color.h>
#include <Defines.h>
#include <Timer.h>
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

    SkillMonitorWidget::Settings settings;

    Color GetColor(const SkillActivationStatus status)
    {
        switch (status) {
        case CASTING:
            return settings.status_color_casting;
        case COMPLETED:
            return settings.status_color_completed;
        case CANCELLED:
            return settings.status_color_cancelled;
        case INTERRUPTED:
            return settings.status_color_interrupted;
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

    GW::HookEntry PostUIMessage_Entry;
    void OnSkillStartedCast(uint32_t agent_id, GW::Constants::SkillID skill_id, float duration)
    {
        const auto skill_history = &history[agent_id];
        if (!skill_history) {
            return;
        }
        casttime_map[agent_id] = duration;
        skill_history->push_back({
            skill_id,
            CASTING,
            TIMER_INIT(),
            TIMER_INIT(),
            duration,
        });
    }
    void OnSkillCompleted(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        const auto skill_history = &history[agent_id];
        if (!skill_history) {
            return;
        }
        const auto casting = find_if(skill_history->begin(), skill_history->end(), [&](const SkillActivation& skill_activation) {
            return skill_activation.status == CASTING && skill_activation.id == skill_id;
        });
        float casttime = 0.f;
        if (casttime_map.contains(agent_id)) {
            casttime = casttime_map[agent_id];
        }
        else {
            const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (skill) casttime = skill->activation;
        }
        if (casting != skill_history->end()) {
            casting->status = COMPLETED;
            casttime_map.erase(agent_id);
        }
        else {
            skill_history->push_back({
                skill_id,
                CANCELLED,
                TIMER_INIT(),
                TIMER_INIT(),
                casttime,
            });
        }
    }
    void OnSkillCancelledOrInterrupted(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        const auto skill_history = &history[agent_id];
        if (!skill_history) {
            return;
        }
        const auto casting = find_if(skill_history->begin(), skill_history->end(), [&](const SkillActivation& skill_activation) {
            return skill_activation.status == CASTING && skill_activation.id == skill_id;
        });
        float casttime = 0.f;
        if (casttime_map.contains(agent_id)) {
            casttime = casttime_map[agent_id];
        }
        else {
            const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (skill) casttime = skill->activation;
        }
        if (casting != skill_history->end()) {
            casting->status = CANCELLED;
            casttime_map.erase(agent_id);
        }
        else {
            skill_history->push_back({
                skill_id,
                CANCELLED,
                TIMER_INIT(),
                TIMER_INIT(),
                casttime,
            });
        }
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
            case GW::UI::UIMessage::kAgentSkillStartedCast: {
                const auto packet = (GW::UI::UIPacket::kAgentSkillStartedCast*)wparam;
                OnSkillStartedCast(packet->agent_id, packet->skill_id, packet->duration);
            } break;
            case GW::UI::UIMessage::kAgentSkillActivatedInstantly:
            case GW::UI::UIMessage::kAgentSkillActivated: {
                const auto packet = (GW::UI::UIPacket::kAgentSkillPacket*)wparam;
                OnSkillCompleted(packet->agent_id, packet->skill_id);
            } break;
            case GW::UI::UIMessage::kAgentSkillCancelled: {
                const auto packet = (GW::UI::UIPacket::kAgentSkillPacket*)wparam;
                OnSkillCancelledOrInterrupted(packet->agent_id, packet->skill_id);
            } break;
        }
    }

}

void SkillMonitorWidget::Initialize()
{
    SnapsToPartyWindow::Initialize();
    SettingsRegistry::Register(this, settings);
    GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kAgentSkillActivated, GW::UI::UIMessage::kAgentSkillActivatedInstantly, GW::UI::UIMessage::kAgentSkillCancelled, GW::UI::UIMessage::kAgentSkillStartedCast};
    for (auto message_id : ui_messages) {
        RegisterUIMessageCallback(&PostUIMessage_Entry, message_id, OnPostUIMessage, 0x4000);
    }
}

void SkillMonitorWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    SnapsToPartyWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void SkillMonitorWidget::SaveSettings(SettingsDoc& doc)
{
    SnapsToPartyWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void SkillMonitorWidget::Terminate()
{
    SnapsToPartyWindow::Terminate();
    GW::UI::RemoveUIMessageCallback(&PostUIMessage_Entry);
}

void SkillMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
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
    const auto width = img_size * settings.history_length;

    const auto user_offset_x = abs(static_cast<float>(settings.user_offset));
    float window_x = .0f;
    if (settings.overlay_party_window) {
        window_x = party_health_bars_position.top_left.x + user_offset_x;
        if (settings.user_offset < 0) {
            window_x = party_health_bars_position.bottom_right.x - user_offset_x - width;
        }

    }
    else {
        window_x = party_health_bars_position.top_left.x - user_offset_x - width;
        if (window_x < 0 || settings.user_offset < 0) {
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
            if (!settings.show_non_party_members && party_slot >= allies_start_idx)
                continue; // Don't draw allies

            const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
            if (!health_bar_pos)
                continue;
            draw_list->AddRectFilled({ window_x , health_bar_pos->top_left.y }, { window_x + width, health_bar_pos->bottom_right.y }, settings.background);

            auto& skill_history = history[agent_id];
            for (size_t i = 0; i < skill_history.size(); i++) {
                const auto& skill_activation = skill_history.at(i);
                const ImVec2 top_left = {settings.history_flip_direction ? window_x + (i * img_size) : window_x + width - (i * img_size) - img_size, health_bar_pos->top_left.y};
                const ImVec2 bottom_right = {top_left.x + img_size, top_left.y + img_size};

                const auto texture = *Resources::GetSkillImage(skill_activation.id);
                if (texture) {
                    draw_list->AddImage(texture, top_left, bottom_right);
                }

                if (settings.status_border_thickness != 0) {
                    draw_list->AddRect(top_left, bottom_right, GetColor(skill_activation.status), 0.f,
                        ImDrawFlags_RoundCornersNone, static_cast<float>(settings.status_border_thickness));

                }

                if (skill_activation.status == CASTING
                    && skill_activation.cast_time * 1000 >= settings.cast_indicator_threshold
                    && ImGui::ColorConvertU32ToFloat4(settings.cast_indicator_color).w != 0) {
                    const auto remaining_cast = TIMER_DIFF(skill_activation.cast_start);
                    const auto percentage_cast = std::min(remaining_cast / (skill_activation.cast_time * 1000), 1.0f);

                    const auto health_bar_width = health_bar_pos->bottom_right.x - health_bar_pos->top_left.x;
                    ImGui::GetBackgroundDrawList()->AddRectFilled(
                        ImVec2(health_bar_pos->top_left.x, health_bar_pos->bottom_right.y - settings.cast_indicator_height),
                        ImVec2(health_bar_pos->top_left.x + (health_bar_width * percentage_cast), health_bar_pos->bottom_right.y),
                        settings.cast_indicator_color);
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
        if (skill_history.size() > static_cast<size_t>(settings.history_length)) {
            skill_history.erase(skill_history.begin(), skill_history.begin() + (skill_history.size() - settings.history_length));
        }

        if (settings.history_timeout != 0) {
            std::erase_if(
                skill_history,
                [&](const SkillActivation& skill_activation) -> bool {
                    return TIMER_DIFF(skill_activation.last_update) > settings.history_timeout;
                });
        }
    }
}

void SkillMonitorWidget::DrawSettingsInternal()
{
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Hide in outpost", &settings.hide_in_outpost);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show non party-members (allies)", &settings.show_non_party_members);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Flip history direction (left/right)", &settings.history_flip_direction);
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("Show on top of health bars", &settings.overlay_party_window, "Untick to show this widget to the left (or right) of the party window.\nTick to show this widget over the top of the party health bars inside the party window");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("Party window offset", &settings.user_offset);
    ImGui::PopItemWidth();
    ImGui::ShowHelp("Distance away from the party window");

    ImGui::Text("Cast Indicator");
    ImGui::DragInt("Threshold", &settings.cast_indicator_threshold, 1.0f, 0, 0, "%d milliseconds");
    ImGui::ShowHelp(
        "Minimum cast time a skill has to have to display the indicator. Note that instantly casted skill will never be displayed.");
    ImGui::InputInt("Height", &settings.cast_indicator_height);
    Colors::DrawSettingHueWheel("Color", &settings.cast_indicator_color.value);

    Colors::DrawSettingHueWheel("Background", &settings.background.value, 0);

    ImGui::Text("Status Border");
    ImGui::InputInt("Border Thickness", &settings.status_border_thickness);
    ImGui::ShowHelp("Set to 0 to disable.");
    if (settings.status_border_thickness < 0) {
        settings.status_border_thickness = 0;
    }
    if (settings.status_border_thickness != 0) {
        if (ImGui::TreeNodeEx("Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            Colors::DrawSettingHueWheel("Completed", &settings.status_color_completed.value);
            Colors::DrawSettingHueWheel("Casting", &settings.status_color_casting.value);
            Colors::DrawSettingHueWheel("Cancelled", &settings.status_color_cancelled.value);
            Colors::DrawSettingHueWheel("Interrupted", &settings.status_color_interrupted.value);
            ImGui::TreePop();
        }
    }

    ImGui::Text("History");
    ImGui::InputInt("Length", &settings.history_length, 0, 25);
    if (settings.history_length < 0) {
        settings.history_length = 0;
    }
    ImGui::DragInt("Timeout", &settings.history_timeout, 1.0f, 0, 0, "%d milliseconds");
    ImGui::ShowHelp("Amount of time after which a skill gets removed from the skill history. Set to 0 to disable.");
    if (settings.history_timeout < 0) {
        settings.history_timeout = 0;
    }
}


