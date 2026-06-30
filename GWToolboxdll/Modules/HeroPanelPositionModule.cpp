#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <ImGuiAddons.h>
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

    // How a remembered position is keyed; persisted, defaults to by party slot.
    enum PositionKeyMode : int { ByHeroIndex, ByHeroId };
    int position_key_mode = ByHeroIndex;

    // Kept separate per keying so switching modes never reinterprets one mode's keys as the other's.
    std::map<uint32_t, GW::UI::FramePosition> saved_positions_by_id;
    std::map<uint32_t, GW::UI::FramePosition> saved_positions_by_index;

    GW::HookEntry ui_message_entry;

    // Visible hero command panel for the given index; fills the panel's hero id (0 if unknown).
    GW::UI::Frame* GetCommanderFrame(uint32_t commander_index, uint32_t* hero_id_out)
    {
        wchar_t label[] = L"AgentCommander0";
        label[_countof(label) - 2] = static_cast<wchar_t>(L'0' + commander_index);
        const auto frame = GW::UI::GetFrameByLabel(label);
        if (!(frame && frame->IsCreated() && frame->IsVisible())) return nullptr;
        const auto context = (uint32_t*)GW::UI::GetFrameContext(frame);
        const auto* hero = context ? ToolboxUtils::GetHeroPartyMember(context[1]) : 0;
        *hero_id_out = hero ? static_cast<uint32_t>(hero->hero_id) : 0;
        return frame;
    }

    // Record into both maps regardless of mode, so toggling restores from the keying kept current.
    void RememberPosition(uint32_t commander_index, uint32_t hero_id, const GW::UI::FramePosition& position)
    {
        saved_positions_by_index[commander_index] = position;
        if (hero_id) saved_positions_by_id[hero_id] = position;
    }

    // User dragged a floating window; if it's a hero command panel, remember where.
    void SavePanelPositions()
    {
        uint32_t hero_id = 0;
        for (uint32_t i = 0; i < hero_panel_count; i++) {
            const auto frame = GetCommanderFrame(i, &hero_id);
            if (frame) RememberPosition(i, hero_id, frame->position);
        }
    }

    // Restore each hero panel, or learn its current placement the first time we see it.
    void RestorePanelPositions()
    {
        const bool by_id = position_key_mode == ByHeroId;
        auto& positions = by_id ? saved_positions_by_id : saved_positions_by_index;
        uint32_t hero_id = 0;
        for (uint32_t i = 0; i < hero_panel_count; i++) {
            const auto frame = GetCommanderFrame(i, &hero_id);
            if (!frame || (by_id && !hero_id)) continue;
            const auto found = positions.find(by_id ? hero_id : i);
            if (found == positions.end()) {
                RememberPosition(i, hero_id, frame->position);
            }
            else {
                // HERO-PANEL REGRESSION TEST: position restore disabled so the module never writes
                // frame->position. If hero panels render correctly with this, this is the regression.
                // Restore by un-commenting the two lines below.
                // frame->position = found->second;
                // GW::UI::TriggerFrameRedraw(frame);
            }
        }
    }

    void LoadPositions(const SettingsDoc& doc, const char* section, const char* key, std::map<uint32_t, GW::UI::FramePosition>& out)
    {
        std::map<std::string, FramePosWords> serialized;
        if (!doc.Get(section, key, serialized)) return;
        out.clear();
        for (const auto& [id_str, words] : serialized) {
            uint32_t id = 0;
            const auto* first = id_str.data();
            const auto* last = first + id_str.size();
            if (std::from_chars(first, last, id).ec != std::errc{}) continue;
            GW::UI::FramePosition position{};
            memcpy(&position, words.data(), sizeof(position));
            out[id] = position;
        }
    }

    void SavePositions(SettingsDoc& doc, const char* section, const char* key, const std::map<uint32_t, GW::UI::FramePosition>& in)
    {
        std::map<std::string, FramePosWords> serialized;
        for (const auto& [id, position] : in) {
            FramePosWords words{};
            memcpy(words.data(), &position, sizeof(position));
            serialized[std::to_string(id)] = words;
        }
        doc.Set(section, key, serialized);
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

void HeroPanelPositionModule::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent(
        "Party Settings", nullptr,
        [](const std::string&, const bool is_showing) {
            if (!is_showing) return;
            ImGui::Text("Remember hero command panel positions by:");
            ImGui::RadioButton("Party slot", &position_key_mode, ByHeroIndex);
            ImGui::ShowHelp("Each party position keeps its panel placement, regardless of which hero is in that slot. Matches the base game's intended behaviour.");
            ImGui::SameLine();
            ImGui::RadioButton("Hero", &position_key_mode, ByHeroId);
            ImGui::ShowHelp("Each hero keeps its panel placement, so it follows the hero across party slots and characters.");
        },
        1.1f
    );
}

void HeroPanelPositionModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    // "positions" predates the keying mode and was always by hero id.
    LoadPositions(doc, Name(), "positions", saved_positions_by_id);
    LoadPositions(doc, Name(), "positions_by_index", saved_positions_by_index);
    doc.Get(Name(), "position_key_mode", position_key_mode);
}

void HeroPanelPositionModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    SavePositions(doc, Name(), "positions", saved_positions_by_id);
    SavePositions(doc, Name(), "positions_by_index", saved_positions_by_index);
    doc.Set(Name(), "position_key_mode", position_key_mode);
}
