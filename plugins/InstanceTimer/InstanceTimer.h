#pragma once

#include <ToolboxUIPlugin.h>

#include <imgui.h>
#include <SimpleIni.h>

class InstanceTimer : public ToolboxUIPlugin {
public:
    InstanceTimer()
    {
        can_show_in_main_window = false;
        show_title = false;
        can_collapse = false;
        can_close = false;
    }
    ~InstanceTimer() override = default;

    const char* Name() const override { return "Plugin Timer"; }

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;
    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

private:
    // those function write to extra_buffer and extra_color.
    // they return true if there is something to draw.
    bool GetUrgozTimer();
    bool GetDeepTimer();
    static bool GetDhuumTimer();
    bool GetTrapTimer();

    bool click_to_print_time = true;
    bool show_extra_timers = false;

    char timer_buffer[32] = "";
    char extra_buffer[32] = "";
    ImColor extra_color = 0;
};
