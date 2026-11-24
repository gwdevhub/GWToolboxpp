#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>

#include <ToolboxWindow.h>
#include <optional>
#include <vector>


class ObjectiveTimerWindow : public ToolboxWindow {
    ObjectiveTimerWindow() = default;

public:
    static ObjectiveTimerWindow& Instance()
    {
        static ObjectiveTimerWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Objectives"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BULLSEYE; }

    void Initialize() override;
    void Terminate() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void LoadRuns();
    void SaveRuns();

    struct Settings {
        bool show_decimal = false;
        bool show_start_column = true;
        bool show_end_column = true;
        bool show_time_column = true;
        bool show_current_run_window = false;
        bool auto_send_age = false;
        bool save_to_disk = true;
        bool show_past_runs = false;
        bool show_start_date_time = false;
        bool show_detailed_objectives = true; // currently only for doa
        int websocket_server_port = 9001;
    };

};
