#pragma once

#include <ToolboxWindow.h>
#include <Windows/Pathfinding/Pathing.h>

using CalculatedCallback = std::function<void (std::vector<GW::GamePos>& waypoints, void* args)>;

/*
    This should really have been a module to just manage pathing - its used in a lot of places.
    Instead, disable the Draw function in release
*/
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

    bool HasSettings() { return false; }

    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Initialize() override;
    void Terminate() override;
    // False if still calculating current map
    static bool ReadyForPathing();
    // False if still calculating current map
    static clock_t CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args = nullptr);

private:
    GW::GamePos m_saved_pos;
    
};
