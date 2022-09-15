#include "stdafx.h"

#include <ToolboxWidget.h>

#include <Modules/ToolboxSettings.h>

ImGuiWindowFlags ToolboxWidget::GetWinFlags(
    ImGuiWindowFlags flags, bool noinput_if_frozen) const {
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoScrollbar;
    if (!ToolboxSettings::move_all) {
        if (lock_move) flags |= ImGuiWindowFlags_NoMove;
        if (lock_size) flags |= ImGuiWindowFlags_NoResize;
        if (noinput_if_frozen && lock_move && lock_size) flags |= ImGuiWindowFlags_NoInputs;
    }
    return flags;
}
