#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <Modules/HeroPanelPositionModule.h>
#include <Timer.h>
#include <Utils/SettingsDoc.h>
#include <Utils/ToolboxUtils.h>

namespace {
    // One AgentCommander{N} frame per hero command panel (N = 0..6).
    constexpr uint32_t hero_panel_count = 7;

    constexpr size_t frame_pos_words = sizeof(GW::UI::FramePosition) / sizeof(uint32_t);
    static_assert(sizeof(GW::UI::FramePosition) % sizeof(uint32_t) == 0);
    using FramePosWords = std::array<uint32_t, frame_pos_words>;

    // hero id -> last known panel position. Keyed by hero id so it survives party reordering.
    std::map<uint32_t, GW::UI::FramePosition> saved_positions;

    GW::HookEntry ui_message_entry;

    GW::UI::Frame* GetCommanderFrame(uint32_t commander_index, GW::Constants::HeroID* hero_id_out)
    {
        wchar_t label[] = L"AgentCommander0";
        label[_countof(label) - 2] = static_cast<wchar_t>(L'0' + commander_index);
        const auto frame = GW::UI::GetFrameByLabel(label);
        if (!(frame && frame->IsCreated() && frame->IsVisible())) return nullptr;
        const auto context = (uint32_t*)GW::UI::GetFrameContext(frame);
        const auto* hero = context ? ToolboxUtils::GetHeroPartyMember(context[1]) : 0;
        return hero ? (*hero_id_out = hero->hero_id, frame) : nullptr;
    }

    // User dragged a floating window; if it's a hero command panel, remember where.
    void SavePanelPositions()
    {
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        for (uint32_t i = 0; i < hero_panel_count; i++) {
            const auto frame = GetCommanderFrame(i, &hero_id);
            if (!frame) continue;
            saved_positions[static_cast<uint32_t>(hero_id)] = frame->position;
        }
    }

    // Restore (or, the first time we see it, learn) a hero panel's position.
    // Returns true once the frame exists and has been handled; false while it's not created yet.
    void RestorePanelPositions()
    {
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        for (uint32_t i = 0; i < hero_panel_count; i++) {
            const auto frame = GetCommanderFrame(i, &hero_id);
            if (!frame) continue;
            const auto found = saved_positions.find(static_cast<uint32_t>(hero_id));
            if (found == saved_positions.end()) {
                saved_positions[static_cast<uint32_t>(hero_id)] = frame->position;
            }
            else {
                frame->position = found->second;
                GW::UI::TriggerFrameRedraw(frame);
            }
        }
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void*, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kFloatingWindowMoved:
                // NB: *wparam is supposed to be frame_id, and it is, but its bollocks.
                GW::GameThread::Enqueue(SavePanelPositions, true);
                break;
            case GW::UI::UIMessage::kShowHeroPanel:
                GW::GameThread::Enqueue(RestorePanelPositions, true);
                break;
            default:
                break;
        }
    }
} // namespace

void HeroPanelPositionModule::Initialize()
{
    ToolboxModule::Initialize();
    const GW::UI::UIMessage messages[] = {
        GW::UI::UIMessage::kFloatingWindowMoved,
        GW::UI::UIMessage::kShowHeroPanel,
    };
    for (const auto message_id : messages) {
        GW::UI::RegisterUIMessageCallback(&ui_message_entry, message_id, OnPostUIMessage, 0x4000);
    }
}

void HeroPanelPositionModule::SignalTerminate()
{
    GW::UI::RemoveUIMessageCallback(&ui_message_entry);
}

void HeroPanelPositionModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    std::map<std::string, FramePosWords> serialized;
    if (!doc.Get(Name(), "positions", serialized)) return;
    saved_positions.clear();
    for (const auto& [key, words] : serialized) {
        uint32_t hero_id = 0;
        const auto* first = key.data();
        const auto* last = first + key.size();
        if (std::from_chars(first, last, hero_id).ec != std::errc{}) continue;
        GW::UI::FramePosition position{};
        memcpy(&position, words.data(), sizeof(position));
        saved_positions[hero_id] = position;
    }
}

void HeroPanelPositionModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    std::map<std::string, FramePosWords> serialized;
    for (const auto& [hero_id, position] : saved_positions) {
        FramePosWords words{};
        memcpy(words.data(), &position, sizeof(position));
        serialized[std::to_string(hero_id)] = words;
    }
    doc.Set(Name(), "positions", serialized);
}
