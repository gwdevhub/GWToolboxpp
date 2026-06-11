#include "stdafx.h"

#include <charconv>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <Modules/HeroPanelPositionModule.h>
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

    GW::UI::Frame* GetCommanderFrame(uint32_t commander_index)
    {
        wchar_t label[] = L"AgentCommander0";
        label[_countof(label) - 2] = static_cast<wchar_t>(L'0' + commander_index);
        return GW::UI::GetFrameByLabel(label);
    }

    // AgentCommander{N} belongs to the player's (N+1)-th hero; GetHeroAgentID is 1-based.
    GW::Constants::HeroID GetHeroIdForCommanderIndex(uint32_t commander_index)
    {
        const uint32_t agent_id = GW::Agents::GetHeroAgentID(commander_index + 1);
        if (!agent_id) return GW::Constants::HeroID::NoHero;
        const auto* hero = ToolboxUtils::GetHeroPartyMember(agent_id);
        return hero ? hero->hero_id : GW::Constants::HeroID::NoHero;
    }

    // User dragged a floating window; if it's a hero command panel, remember where.
    void OnPanelMoved(uint32_t frame_id)
    {
        for (uint32_t i = 0; i < hero_panel_count; i++) {
            const auto frame = GetCommanderFrame(i);
            if (!frame || frame->frame_id != frame_id) continue;
            const auto hero_id = GetHeroIdForCommanderIndex(i);
            if (hero_id != GW::Constants::HeroID::NoHero) {
                saved_positions[static_cast<uint32_t>(hero_id)] = frame->position;
            }
            return;
        }
    }

    // Hero panel (re)shown - e.g. after a party edit or character swap - so put it back.
    void OnPanelShown(GW::Constants::HeroID hero_id)
    {
        // Run on next frame so the commander panel gets created
        GW::GameThread::Enqueue(
            [hero_id]() {
                const auto found = saved_positions.find(static_cast<uint32_t>(hero_id));
                for (uint32_t i = 0; i < hero_panel_count; i++) {
                    if (GetHeroIdForCommanderIndex(i) != hero_id) continue;
                    const auto frame = GetCommanderFrame(i);
                    if (!frame) continue;
                    if (found == saved_positions.end()) {
                        saved_positions[static_cast<uint32_t>(hero_id)] = frame->position;
                    }
                    else {
                        frame->position = found->second;
                        GW::UI::TriggerFrameRedraw(frame);
                    }
                    return;
                }
            },
            true
        );
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kFloatingWindowMoved:
                OnPanelMoved(*static_cast<uint32_t*>(wparam));
                break;
            case GW::UI::UIMessage::kShowHeroPanel:
                OnPanelShown(static_cast<GW::Constants::HeroID>(reinterpret_cast<uintptr_t>(wparam)));
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
