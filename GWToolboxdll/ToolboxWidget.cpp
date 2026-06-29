#include "stdafx.h"

#include <ToolboxWidget.h>
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
    if (noinput_if_frozen && IsMoveLocked() && IsSizeLocked() && !ToolboxSettings::move_all) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    return flags;
}
// Everything needs a window in ImGui, even if its a widget that technically isn't contrained to a window like space. If you dont do it, ImGui will create a Debug window on-screen. Don't forget ImGui::End() when done.
bool ToolboxWidget::DummyWindow(ImGuiWindowFlags flags)
{
    ImGui::SetNextWindowSize({0, 0});
    ImGui::SetNextWindowBgAlpha(0.f);
    return ImGui::Begin(Name(), nullptr, GetWinFlags(flags | ImGuiWindowFlags_NoInputs,false));
}
