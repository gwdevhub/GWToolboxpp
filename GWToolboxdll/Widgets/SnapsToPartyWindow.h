#pragma once

#include <ToolboxWidget.h>

class SnapsToPartyWindow : public ToolboxWidget {
protected:
    std::pair<ImVec2, ImVec2> party_health_bars_position;
    std::unordered_map<uint32_t, std::pair<ImVec2, ImVec2>> agent_health_bar_positions;
    bool RecalculatePartyPositions();
public:
    virtual void Initialize() override;
    virtual void Terminate() override;

    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const override;

    virtual void Draw(IDirect3DDevice9* device) override;
};
