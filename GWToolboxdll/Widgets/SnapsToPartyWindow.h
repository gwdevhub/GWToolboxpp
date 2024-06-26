#pragma once

#include <ToolboxWidget.h>

#include <Utils/GuiUtils.h>

class SnapsToPartyWindow : public ToolboxWidget {
protected:
    struct PartyFramePosition {
        ImVec2 top_left;
        ImVec2 bottom_right;
    };
    static PartyFramePosition party_health_bars_position;
    static std::unordered_map<uint32_t, PartyFramePosition> agent_health_bar_positions;

    static uint32_t henchmen_start_idx;
    static uint32_t pets_start_idx;
    static uint32_t allies_start_idx;
    static std::unordered_map<uint32_t,uint32_t> party_indeces_by_agent_id;
    static std::vector<uint32_t> party_agent_ids_by_index;
    static std::vector<GuiUtils::EncString*> party_names_by_index;

    static bool RecalculatePartyPositions();
    static bool FetchPartyInfo();
    static PartyFramePosition* GetAgentHealthBarPosition(uint32_t agent_id);
public:
    void Initialize() override;
    void Terminate() override;

    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const override;

    void Draw(IDirect3DDevice9* device) override;
};
