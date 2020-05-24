#pragma once

#include "ToolboxWindow.h"

class DailyActivitiesWindow : public ToolboxWindow {
    DailyActivitiesWindow() {}
    ~DailyActivitiesWindow() {}
public:
    static DailyActivitiesWindow& Instance() {
        static DailyActivitiesWindow instance;
        return instance;
    }

private: // from ToolboxWindow
    const char* Name() const override { return "Daily Activities"; }

    void Initialize() override;

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

private:
    void DrawLine(
        const char* date,
        const char* mission,
        const char* bounty,
        const char* combat,
        const char* vanquish,
        const char* wanted);

private:
    time_t cycle_begin = 0;

    bool show_mission = true;
    bool show_bounty = true;
    bool show_combat = false;
    bool show_vanquish = false;
    bool show_wanted = false;
    bool show_vanguard = false;
    bool show_nicholas_sandford = false;
};
