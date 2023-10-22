#include "stdafx.h"

#include <ToolboxWidget.h>
#include <Defines.h>
#include <Modules/ToolboxSettings.h>

void ToolboxWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxUIElement::LoadSettings(ini);
    LOAD_BOOL(lock_move);
    LOAD_BOOL(lock_size);
}

void ToolboxWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxUIElement::SaveSettings(ini);
    SAVE_BOOL(lock_move);
    SAVE_BOOL(lock_size);
}

ImGuiWindowFlags ToolboxWidget::GetWinFlags(ImGuiWindowFlags flags, const bool noinput_if_frozen) const
{
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoScrollbar;
    if (!ToolboxSettings::move_all) {
        if (lock_move) {
            flags |= ImGuiWindowFlags_NoMove;
        }
        if (lock_size) {
            flags |= ImGuiWindowFlags_NoResize;
        }
        if (noinput_if_frozen && lock_move && lock_size) {
            flags |= ImGuiWindowFlags_NoInputs;
        }
    }
    return flags;
}
