#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/GameEntities/Frame.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Widgets/TitleTrackerWidget.h>

#include "Utils/FontLoader.h"
#include "Utils/GuiUtils.h"
#include <Color.h>
#include <Defines.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>
#include <Constants/EncStrings.h>

namespace {

    bool override_title_sort_order = true;
    bool pending_title_sort_order = false;
    bool show_overall_title_progress = false;
    bool hide_completed_titles = true;
    bool automatically_show_title_progress_for_current_map = true;
    float progress_bar_height = 24.f;
    Color widget_background_color = IM_COL32(0,0,0,0);
    Color progress_bar_background_color = IM_COL32(30, 30, 30, 255);
    Color progress_bar_foreground_color = IM_COL32(110, 83, 123, 255);
    Color progress_bar_border_color = IM_COL32(80, 80, 80, 255);
    Color progress_overlay_label_color = IM_COL32(255, 255, 255, 255);
    Color title_label_color = IM_COL32_WHITE;
    Color tier_label_color = IM_COL32_WHITE;

    std::vector<GW::Constants::TitleID> title_for_bounty;

    struct TitleProgress {
        GW::Constants::TitleID title_id;
        GuiUtils::EncString title_label;
        GuiUtils::EncString tier_label;
        GuiUtils::EncString* overlay_label = 0; // Because this can change quickly, we may need to recycle it faster than the frame rate can handle
        float percent = 0.f;
        float secondary_percent = 0.f;
        GuiUtils::EncString* secondary_label = 0;
        bool show = false;
        TitleProgress(GW::Constants::TitleID _title_id) : title_id(_title_id) { 
            overlay_label = new GuiUtils::EncString();
            secondary_label = new GuiUtils::EncString();
            RefreshLabels();
        };
        ~TitleProgress() { 
            if (overlay_label) overlay_label->Release();
            overlay_label = 0;
            if (secondary_label) secondary_label->Release();
            secondary_label = 0;
        }
        void RefreshProgress();
        void RefreshLabels();
        bool IsValid() { return overlay_label && !overlay_label->encoded().empty();}
    };

    std::vector<TitleProgress*> title_progress_by_id;
    std::vector<TitleProgress*> title_progress_by_title;

    const uint32_t GetTitleMax(GW::Constants::TitleID title_id)
    {
        using namespace GW::Constants;
        switch (title_id) {
            case TitleID::Drunkard:
            case TitleID::Party:
            case TitleID::Sweets:
            case TitleID::Champion:
            case TitleID::TreasureHunter:
            case TitleID::Wisdom:
                return 10000;
            case TitleID::Survivor:
                return 1337500;
            case TitleID::KoaBD:
                return 30;
            case TitleID::Lightbringer:
            case TitleID::Sunspear:
                return 50000;
            case TitleID::Asuran:
            case TitleID::Deldrimor:
            case TitleID::Vanguard:
            case TitleID::Norn:
                return 160000;
            case TitleID::MasterOfTheNorth:
                return 1000;
            case TitleID::Codex:
            case TitleID::Gladiator:
                return 200000;
            case TitleID::Gamer:
                return 135000;
            case TitleID::Zaishen:
            case TitleID::Hero:
                return 100000;
            case TitleID::Lucky:
                return 2500000;
            case TitleID::Unlucky:
                return 500000;
            case TitleID::Kurzick:
            case TitleID::Luxon:
                return 10000000;
        }
        return 0; // Fall back to points_needed_next_rank if 0
    }
    float GetTitleProgressRatio(GW::Constants::TitleID title_id, GW::Title* title, bool current_tier = false)
    {
        if (!title) return 0.f;
        // Calculate progress ratio like original
        if (title->is_percentage_based()) {
            // Percentage-based: min(current_points, 1000) / 1000
            uint32_t capped_points = std::min(title->current_points, 1000u);
            return (float)capped_points / 1000.0f;
        }
        if (title->points_needed_next_rank == 0xFFFFFFFF) {
            return 1.f;
        }
        if (title->current_points == 0) {
            return 0.f;
        }
        uint32_t title_max_points_needed = GetTitleMax(title_id);
        if (!title_max_points_needed || current_tier) {
            title_max_points_needed = title->points_needed_next_rank;
        }
        return (float)title->current_points / (float)title_max_points_needed;
    }
    
    bool CompareTitleProgress(const TitleProgress* t1, const TitleProgress* t2)
    {
        const auto title_1 = GW::PlayerMgr::GetTitleTrack(t1->title_id);
        const auto title_2 = GW::PlayerMgr::GetTitleTrack(t2->title_id);

        if (!title_1 || !title_2) return false;

        // Check for unavailable titles (points_needed_next_rank == -1)
        bool unavailable_1 = (title_1->points_needed_next_rank == 0xFFFFFFFF);
        bool unavailable_2 = (title_2->points_needed_next_rank == 0xFFFFFFFF);

        if (unavailable_1 != unavailable_2) {
            return !unavailable_1; // available titles come first
        }

        float ratio_1 = GetTitleProgressRatio(t1->title_id, title_1, show_overall_title_progress);
        float ratio_2 = GetTitleProgressRatio(t2->title_id, title_2, show_overall_title_progress);

        return ratio_1 > ratio_2; // higher progress comes first
    }
    
    int TitleSortHandler(uint32_t frame_id_1, uint32_t frame_id_2)
    {
        auto GetTitleInfo = [](uint32_t frame_id, GW::Constants::TitleID* title_id_out) -> GW::Title* {
            const auto frame = GW::UI::GetFrameById(frame_id);
            if (!frame) return nullptr;
            const auto title_id = frame->child_offset_id;
            const auto w = GW::GetWorldContext();
            if (!w) return nullptr;
            if (w->titles.size() <= title_id) return nullptr;
            if (title_id_out) *title_id_out = (GW::Constants::TitleID)title_id;
            return &w->titles[title_id];
        };
        auto title_id_1 = (GW::Constants::TitleID)0;
        auto title_id_2 = (GW::Constants::TitleID)0;
        const auto title_1 = GetTitleInfo(frame_id_1, &title_id_1);
        const auto title_2 = GetTitleInfo(frame_id_2, &title_id_2);

        if (!title_1 || !title_2) return 0;

        // Check for unavailable titles (points_needed_next_rank == -1)
        bool unavailable_1 = (title_1->points_needed_next_rank == 0xFFFFFFFF);
        bool unavailable_2 = (title_2->points_needed_next_rank == 0xFFFFFFFF);

        if (unavailable_1 != unavailable_2) {
            return unavailable_1 ? 1 : 0; // unavailable title goes after available one
        }

        float ratio_1 = GetTitleProgressRatio(title_id_1, title_1, show_overall_title_progress);
        float ratio_2 = GetTitleProgressRatio(title_id_2, title_2, show_overall_title_progress);

        const auto progress_1 = (GW::ProgressBar*)GW::UI::GetChildFrame(GW::UI::GetFrameById(frame_id_1), 2);
        if (progress_1) {
            progress_1->SetMax(1000);
            progress_1->SetValue(static_cast<uint32_t>(std::floor(ratio_1 * 1000.0f)));
            // progress_1->SetStyle((GW::ProgressBar::ProgressBarStyle)8);
        }
        const auto progress_2 = (GW::ProgressBar*)GW::UI::GetChildFrame(GW::UI::GetFrameById(frame_id_2), 2);
        if (progress_2) {
            progress_2->SetMax(1000);
            progress_2->SetValue(static_cast<uint32_t>(std::floor(ratio_2 * 1000.0f)));
            // progress_2->SetStyle((GW::ProgressBar::ProgressBarStyle)8);
        }

        return (ratio_1 < ratio_2) ? 1 : 0; // higher progress comes first
    }
    GW::ScrollableFrame::SortHandler_pt OriginalSortHandler = 0;
    bool OverrideTitleSortOrder(bool _override = true)
    {
        const auto frame = (GW::ScrollableFrame*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Attributes"), 2, 2, 0);
        if (!frame) return false;
        if (!OriginalSortHandler) {
            OriginalSortHandler = frame->GetSortHandler();
        }
        return frame->SetSortHandler(0) && frame->SetSortHandler(_override ? TitleSortHandler : OriginalSortHandler);
    }

    void RefreshTitleProgress()
    {
        OverrideTitleSortOrder(override_title_sort_order);
        for (auto& t : title_progress_by_id) {
            t->RefreshLabels();
            t->RefreshProgress();
        }
        std::sort(title_progress_by_id.begin(), title_progress_by_id.end(), CompareTitleProgress);
        std::sort(title_progress_by_title.begin(), title_progress_by_title.end(), [](TitleProgress* t1, TitleProgress* t2) {
            return t1->title_label.string() < t2->title_label.string();
        });
        title_for_bounty = automatically_show_title_progress_for_current_map ? GW::Map::GetBountyTitlesForMap(GW::Map::GetMapID()) : std::vector<GW::Constants::TitleID>();
    }

    void TitleProgress::RefreshProgress()
    {
        percent = GetTitleProgressRatio(title_id, GW::PlayerMgr::GetTitleTrack(title_id), show_overall_title_progress);
        std::wstring str_placeholder;
        uint32_t faction_gained = 0;
        uint32_t faction_max = 0;
        const wchar_t* faction_name = nullptr;
        const auto w = GW::GetWorldContext();
        switch (title_id) {
            case GW::Constants::TitleID::Kurzick: {
                faction_gained = w->current_kurzick;
                faction_max = w->max_kurzick;
                faction_name = GW::EncStrings::Faction::Kurzick;
            } break;
            case GW::Constants::TitleID::Luxon: {
                faction_gained = w->current_luxon;
                faction_max = w->max_luxon;
                faction_name = GW::EncStrings::Faction::Luxon;
            } break;
        }
        if (faction_name) {
            secondary_percent = (float)faction_gained / (float)faction_max;
            wchar_t current_points_buf[3];
            GW::UI::UInt32ToEncStr(faction_gained, current_points_buf, _countof(current_points_buf));
            wchar_t points_needed_buf[3];
            GW::UI::UInt32ToEncStr(faction_max, points_needed_buf, _countof(points_needed_buf));
            str_placeholder = std::format(L"{}\x2\x108\x107: \x1\x2\x8101\x29C\x101{}\x102{}", faction_name, current_points_buf, points_needed_buf);
            if (secondary_label) secondary_label->Release();
            secondary_label = new GuiUtils::EncString(str_placeholder.c_str(), false);
        }

        const auto title_info = GW::PlayerMgr::GetTitleTrack(title_id);
        if (title_info && title_info->h0020 && *title_info->h0020) {
            
            if (title_info->has_tiers()) {
                // N/N
                const auto points_needed = title_info->points_needed_next_rank == -1 ? title_info->points_needed_current_rank : title_info->points_needed_next_rank;
                wchar_t current_points_buf[3];
                GW::UI::UInt32ToEncStr(title_info->current_points, current_points_buf, _countof(current_points_buf));
                wchar_t points_needed_buf[3];
                GW::UI::UInt32ToEncStr(points_needed, points_needed_buf, _countof(points_needed_buf));
                str_placeholder = std::format(L"{}\x10a\x8101\x29C\x101{}\x102{}\x1", title_info->h0020, current_points_buf, points_needed_buf);
            }
            else if (title_info->is_percentage_based()) {
                wchar_t percent_buf[3];
                GW::UI::UInt32ToEncStr((uint32_t)(percent * 100), percent_buf, _countof(percent_buf));
                str_placeholder = std::format(L"{}\x10a\x8101\x379\x101{}\x102\x100\x1", title_info->h0020, percent_buf);
            }
            else {
                wchar_t current_points_buf[3];
                GW::UI::UInt32ToEncStr(title_info->current_points, current_points_buf, _countof(current_points_buf));
                str_placeholder = std::format(L"{}\x10a\x104\x101{}\x1", title_info->h0020, current_points_buf);
            }
            if (!str_placeholder.empty()) {
                if (overlay_label) overlay_label->Release();
                overlay_label = new GuiUtils::EncString(str_placeholder.c_str(), false);
            }
        }

    }
    void TitleProgress::RefreshLabels()
    {
        const auto w = GW::GetWorldContext();
        if (!w) return;
        const auto title_data = GW::PlayerMgr::GetTitleData(title_id);
        if (title_data) {
            title_label.reset(title_data->name_id);

        }
        const auto title_info = GW::PlayerMgr::GetTitleTrack(title_id);
        if (!(title_info && w->title_tiers.size() > title_info->current_title_tier_index)) {
            return;
        }
        const auto& current_tier = w->title_tiers[title_info->current_title_tier_index];
        tier_label.reset(current_tier.tier_name_enc);
    }


    GW::HookEntry OnPostUIMessage_HookEntry;

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kExperienceGained: {
                const auto faction_id = (uint32_t)wparam;
                if (faction_id == 1 || faction_id == 3) RefreshTitleProgress();
            } break;
            case GW::UI::UIMessage::kTitleProgressUpdated: {
                RefreshTitleProgress();
            } break;
            case GW::UI::UIMessage::kUIPositionChanged: {
                OverrideTitleSortOrder(override_title_sort_order);
            } break;
        }
    }


}

float TitleTrackerWidget::GetTitleProgressRatio(GW::Constants::TitleID title_id, GW::Title* title, bool current_tier)
{
    return ::GetTitleProgressRatio(title_id, title, current_tier);
}

void TitleTrackerWidget::Initialize()
{
    ToolboxWidget::Initialize();
    pending_title_sort_order = true;
    GW::GameThread::Enqueue([]() {
        OverrideTitleSortOrder(override_title_sort_order);
        pending_title_sort_order = false;
    });
    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kTitleProgressUpdated, GW::UI::UIMessage::kExperienceGained, GW::UI::UIMessage::kUIPositionChanged};
    for (const auto& msg : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, msg, OnPostUIMessage, 0x8000);
    }
    for (size_t i = 0; i != (size_t)GW::Constants::TitleID::Codex; i++) {
        title_progress_by_id.push_back(new TitleProgress((GW::Constants::TitleID)i));
        title_progress_by_title.push_back(title_progress_by_id.back());
    }
    GW::GameThread::Enqueue(RefreshTitleProgress, true);
}
void TitleTrackerWidget::SignalTerminate()
{
    ToolboxWidget::SignalTerminate();

    for (auto t : title_progress_by_id) {
        delete t;
    }
    title_progress_by_id.clear();

    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    pending_title_sort_order = true;
    GW::GameThread::Enqueue([]() {
        OverrideTitleSortOrder(false);
        pending_title_sort_order = false;
    });
}
bool TitleTrackerWidget::CanTerminate()
{
    return !pending_title_sort_order;
}

void TitleTrackerWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    static float calculated_widget_height = 0.0f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, widget_background_color);
    ImGui::SetNextWindowSize(ImVec2(300.f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        progress_bar_height = std::max(progress_bar_height, ImGui::GetTextLineHeight());
        const float available_width = std::max(ImGui::GetContentRegionAvail().x,280.f);
        ImVec2 bar_size(available_width, progress_bar_height);
        const auto draw_list = ImGui::GetWindowDrawList();
        
        // Calculate progress bar colour
        const ImVec4 base_color = ImGui::ColorConvertU32ToFloat4(progress_bar_foreground_color);
        const float lighten_factor = 1.5f; // Adjust to control how much lighter
        ImVec4 light_color(std::min(base_color.x * lighten_factor, 1.0f), std::min(base_color.y * lighten_factor, 1.0f), std::min(base_color.z * lighten_factor, 1.0f), base_color.w);

        // Convert ImVec4 colors to ImU32
        ImU32 color_start = progress_bar_foreground_color;
        ImU32 color_end = ImGui::ColorConvertFloat4ToU32(light_color);

        for (auto p : title_progress_by_id) {
            if (p->percent == 1.f && hide_completed_titles) continue;
            if (!p->show && std::ranges::find(title_for_bounty.begin(), title_for_bounty.end(), p->title_id) == title_for_bounty.end()) {
                continue;
            }
            if (p->overlay_label->encoded().empty()) continue;
            // Get strings from EncString
            const auto& title_text = p->title_label.string();
            const auto& sub_title_text = p->tier_label.string();
            const auto& overlay_text = p->overlay_label->string();

            // Draw title label (left aligned) and sub-title label (right aligned) on same line
            
            ImVec2 label_pos = ImGui::GetCursorPos();
            if (Colors::IsVisible(title_label_color)) {
                ImGui::PushStyleColor(ImGuiCol_Text, title_label_color);
                ImGui::TextShadowed(title_text.c_str());
                ImGui::PopStyleColor();
            }
            
            if (Colors::IsVisible(tier_label_color)) {
                const auto sub_title_size = ImGui::CalcTextSize(sub_title_text.c_str());
                ImGui::SetCursorPos({available_width - sub_title_size.x, label_pos.y});
                ImGui::PushStyleColor(ImGuiCol_Text, tier_label_color);
                ImGui::TextShadowed(sub_title_text.c_str());
                ImGui::PopStyleColor();
            }

            ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            ImVec2 bar_pos_max = ImVec2(bar_pos.x + bar_size.x, bar_pos.y + bar_size.y);
            // Background progress bar
            draw_list->AddRectFilled(bar_pos, bar_pos_max, progress_bar_background_color);

            // Foreground progress bar
            float fill_width = bar_size.x * p->percent;
            draw_list->AddRectFilledMultiColor(bar_pos, ImVec2(bar_pos.x + fill_width, bar_pos.y + bar_size.y), color_start, color_end, color_end, color_start);

            // Secondary progress bar (faction progress)
            if (p->secondary_percent > 0.f) {
                const float secondary_height = bar_size.y / 6.0f;
                ImVec2 secondary_bar_pos(bar_pos.x, bar_pos_max.y - secondary_height);
                float secondary_fill_width = bar_size.x * p->secondary_percent;

                // Determine color based on progress thresholds
                ImU32 secondary_color;
                if (p->secondary_percent >= 1.0f) {
                    secondary_color = IM_COL32(255, 0, 0, 150); // Red when full
                }
                else if (p->secondary_percent >= 0.7f) {
                    secondary_color = IM_COL32(255, 255, 0, 150); // Yellow at 70%+
                }
                else {
                    secondary_color = IM_COL32(0, 255, 0, 150); // Green below 70%
                }

                draw_list->AddRectFilled(secondary_bar_pos, ImVec2(secondary_bar_pos.x + secondary_fill_width, secondary_bar_pos.y + secondary_height), secondary_color);
            }

            // Border
            draw_list->AddRect(bar_pos, bar_pos_max, progress_bar_border_color);

            // Overlay label
            if (Colors::IsVisible(progress_overlay_label_color)) {
                ImVec2 overlay_size = ImGui::CalcTextSize(overlay_text.c_str());
                ImVec2 overlay_pos(bar_pos.x + (bar_size.x - overlay_size.x) * 0.5f, bar_pos.y + (bar_size.y - overlay_size.y) * 0.5f);

                draw_list->AddText(overlay_pos, IM_COL32_BLACK, overlay_text.c_str());
                draw_list->AddText({overlay_pos.x + 1.f, overlay_pos.y + 1.f}, progress_overlay_label_color, overlay_text.c_str());
            }
            if (ImGui::IsMouseHoveringRect(bar_pos, bar_pos_max)) {
                auto label = std::format("{}\n{}\n{}",p->title_label.string(),p->tier_label.string(),p->overlay_label->string());
                if (!p->secondary_label->encoded().empty()) {
                    label += "\n\n";
                    label += p->secondary_label->string();
                }
                ImGui::SetTooltip(label.c_str());
            }

            // Move cursor past the progress bar
            ImGui::Dummy(bar_size);
            ImGui::Spacing();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void TitleTrackerWidget::DrawSettingsInternal()
{
    if (ImGui::Checkbox("Override title sort order in Hero Panel", &override_title_sort_order)) RefreshTitleProgress();
    if (ImGui::Checkbox("Show progress by overall title, or by current tier", &show_overall_title_progress)) RefreshTitleProgress();
    ImGui::Separator();
    ImGui::TextUnformatted("Widget settings");
    ImGui::Checkbox("Hide completed titles", &hide_completed_titles);
    if (ImGui::Checkbox("Automatically show title progress for current map", &automatically_show_title_progress_for_current_map)) {
        RefreshTitleProgress();
    }
    ImGui::ShowHelp("e.g. when in an Asuran area, display title progress for the Asuran title track");
    ImGui::InputFloat("Progress bar height", &progress_bar_height, 1.f, 4.f, "%.f");
    ImGui::Text("Show/Hide Titles:");
    ImGui::Indent();
    ImGui::StartSpacedElements(240.f);
    for (auto p : title_progress_by_title) {
        if (!p->IsValid()) continue;
        const auto& str = p->title_label.string();
        if (str.empty()) continue;
        ImGui::NextSpacedElement();
        ImGui::Checkbox(str.c_str(), &p->show);
    }
    ImGui::Unindent();

    ImGui::TextUnformatted("Widget colors");
    ImGui::StartSpacedElements(240.f);
    ImGui::Indent();
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Widget background color", &progress_bar_background_color, ImGuiColorEditFlags_NoInputs);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Title label color", &title_label_color, ImGuiColorEditFlags_NoInputs);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Tier label color", &tier_label_color, ImGuiColorEditFlags_NoInputs);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Progress bar background color", &progress_bar_background_color, ImGuiColorEditFlags_NoInputs);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Progress bar foreground color", &progress_bar_foreground_color, ImGuiColorEditFlags_NoInputs);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Progress bar overlay text color", &progress_overlay_label_color, ImGuiColorEditFlags_NoInputs);
    ImGui::Unindent();
}

void TitleTrackerWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    std::string show_title_progress_widgets_str;
    std::vector<uint32_t> show_title_progress_widgets;
    for (auto p : title_progress_by_id) {
        if (p->show) {
            show_title_progress_widgets.push_back(std::to_underlying(p->title_id));
        }
    }
    GuiUtils::ArrayToIni(show_title_progress_widgets.data(), show_title_progress_widgets.size(), &show_title_progress_widgets_str);
    SAVE_STRING(show_title_progress_widgets_str);
    SAVE_BOOL(automatically_show_title_progress_for_current_map);
    SAVE_BOOL(hide_completed_titles);
    SAVE_BOOL(override_title_sort_order);
    SAVE_BOOL(show_overall_title_progress);
    SAVE_COLOR(progress_bar_background_color);
    SAVE_COLOR(title_label_color);
    SAVE_COLOR(tier_label_color);
    SAVE_COLOR(progress_bar_background_color);
    SAVE_COLOR(progress_bar_foreground_color);
    SAVE_COLOR(progress_overlay_label_color);
    SAVE_FLOAT(progress_bar_height);
}

void TitleTrackerWidget::LoadSettings(ToolboxIni* ini) {
    ToolboxWidget::LoadSettings(ini);
    std::vector<uint32_t> show_title_progress_widgets;
    std::string show_title_progress_widgets_str;
    LOAD_STRING(show_title_progress_widgets_str);
    GuiUtils::IniToArray(show_title_progress_widgets_str, show_title_progress_widgets);
    for (auto p : title_progress_by_id) {
        p->show = std::ranges::find(show_title_progress_widgets, std::to_underlying(p->title_id)) != show_title_progress_widgets.end();
    }
    LOAD_BOOL(automatically_show_title_progress_for_current_map);
    LOAD_BOOL(hide_completed_titles);
    LOAD_BOOL(override_title_sort_order);
    LOAD_BOOL(show_overall_title_progress);
    LOAD_COLOR(progress_bar_background_color);
    LOAD_COLOR(title_label_color);
    LOAD_COLOR(tier_label_color);
    LOAD_COLOR(progress_bar_background_color);
    LOAD_COLOR(progress_bar_foreground_color);
    LOAD_COLOR(progress_overlay_label_color);
    LOAD_FLOAT(progress_bar_height);
}
