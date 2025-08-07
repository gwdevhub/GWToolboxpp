#include "stdafx.h"

#include <GWCA/GameEntities/Title.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameEntities/Frame.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include "TitleOrderModule.h"

namespace {

    bool override_title_sort_order = true;
    bool pending_title_sort_order = false;

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
    float GetTitleProgressRatio(GW::Constants::TitleID title_id, GW::Title* title)
    {
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
        if (!title_max_points_needed) {
            title_max_points_needed = title->points_needed_next_rank;
        }
        return (float)title->current_points / (float)title_max_points_needed;
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

        float ratio_1 = GetTitleProgressRatio(title_id_1, title_1);
        float ratio_2 = GetTitleProgressRatio(title_id_2, title_2);

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

    GW::HookEntry OnPostUIMessage_HookEntry;

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kPlayerTitleChanged:
            case GW::UI::UIMessage::kTitleProgressUpdated:
            case GW::UI::UIMessage::kUIPositionChanged: {
                GW::GameThread::Enqueue([]() {
                    OverrideTitleSortOrder(override_title_sort_order);
                });
            } break;
        }
    }

}

void TitleOrderModule::Initialize()
{
    ToolboxModule::Initialize();
    pending_title_sort_order = true;
    GW::GameThread::Enqueue([]() {
        OverrideTitleSortOrder(override_title_sort_order);
        pending_title_sort_order = false;
    });
    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kPlayerTitleChanged, GW::UI::UIMessage::kTitleProgressUpdated, GW::UI::UIMessage::kUIPositionChanged};
    for (const auto& msg : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, msg, OnPostUIMessage, 0x8000);
    }

}
void TitleOrderModule::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    pending_title_sort_order = true;
    GW::GameThread::Enqueue([]() {
        OverrideTitleSortOrder(false);
        pending_title_sort_order = false;
    });
}
bool TitleOrderModule::CanTerminate()
{
    return !pending_title_sort_order;
}
