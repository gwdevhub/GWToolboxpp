#pragma once

#include "imgui.h"

#include <module_base.h>
#include <SimpleIni.h>

class InstanceTimer : public ToolboxPlugin {

public:
    InstanceTimer();
    ~InstanceTimer() override = default;

	const char* Name() const override { return "Timer"; }

	void LoadSettings() override;
	void SaveSettings() override;
    void DrawSettings() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

private:
    // those function write to extra_buffer and extra_color.
    // they return true if there is something to draw.
    bool GetUrgozTimer();
    bool GetDeepTimer();
    bool GetDhuumTimer();
    bool GetTrapTimer();

	bool click_to_print_time = false;
    bool show_extra_timers = false;
    bool visible = true;

    char timer_buffer[32] = "";
    char extra_buffer[32] = "";
    ImColor extra_color = 0;
    CSimpleIniA ini{};
};
