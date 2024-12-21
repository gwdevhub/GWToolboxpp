#include "stdafx.h"

#include <ToolboxWidget.h>
#include <Defines.h>
#include <Modules/ToolboxSettings.h>

ImGuiWindowFlags ToolboxWidget::GetWinFlags(ImGuiWindowFlags flags) const
{
    return GetWinFlags(flags, false);
}

ImGuiWindowFlags ToolboxWidget::GetWinFlags(ImGuiWindowFlags flags, const bool noinput_if_frozen) const
{
    flags = ToolboxUIElement::GetWinFlags(flags);
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoScrollbar;
    if (noinput_if_frozen && lock_move && lock_size) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    return flags;
}
