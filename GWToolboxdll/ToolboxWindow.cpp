#include "stdafx.h"

#include <ImGuiAddons.h>
#include <ToolboxWindow.h>

#include <Modules/ToolboxSettings.h>
#include <Windows/SettingsWindow.h>

ImGuiWindowFlags ToolboxWindow::GetWinFlags(ImGuiWindowFlags flags) const {
    if (!ToolboxSettings::move_all) {
        if (lock_move) flags |= ImGuiWindowFlags_NoMove;
        if (lock_size) flags |= ImGuiWindowFlags_NoResize;
    }
    return flags;
}
