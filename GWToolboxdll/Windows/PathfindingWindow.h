#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWindow.h>
#include <Windows/Pathing.h>

using CalculatedCallback = std::function<void (const std::vector<GW::GamePos>& waypoints, void* args)>;

class PathfindingWindow : public ToolboxWindow {
    PathfindingWindow() = default;
    ~PathfindingWindow() = default;

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
    bool CanTerminate() override;
    void Terminate() override;
    // False if still calculating current map
    static bool ReadyForPathing();
    // False if still calculating current map
    static bool CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args = nullptr);

private:
    GW::GamePos m_saved_pos;
    
};
