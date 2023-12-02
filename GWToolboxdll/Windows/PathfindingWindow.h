#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWindow.h>
#include <Windows/Pathing.h>

class PathfindingWindow : public ToolboxWindow {
    PathfindingWindow() = default;
    ~PathfindingWindow() override = default;

public:
    static PathfindingWindow& Instance()
    {
        static PathfindingWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pathfinding Window"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_DOOR_OPEN; }

    void Initialize() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;

private:
    GW::GamePos m_saved_pos;
    Pathing::AStar::Path m_path;

public:
    
};
