#include "stdafx.h"

#include <ToolboxWidget.h>
#include <Modules/ToolboxSettings.h>

ImGuiWindowFlags ToolboxWidget::GetWinFlags(const ImGuiWindowFlags flags) const
{
    return GetWinFlags(flags, false);
}

ImGuiWindowFlags ToolboxWidget::GetWinFlags(ImGuiWindowFlags flags, const bool noinput_if_frozen) const
{
    flags = ToolboxUIElement::GetWinFlags(flags);
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoScrollbar;
    if (noinput_if_frozen && lock_move && lock_size && !ToolboxSettings::move_all) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    return flags;
}
