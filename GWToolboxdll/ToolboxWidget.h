#pragma once

#include <ToolboxUIElement.h>

class ToolboxWidget : public ToolboxUIElement {
public:
    ToolboxWidget() { lock_move = lock_size = true; }
    ~ToolboxWidget() override = default;

    [[nodiscard]] bool IsWidget() const override { return true; }
    [[nodiscard]] const char* TypeName() const override { return "widget"; }

    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const override;

    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags, bool noinput_if_frozen) const;
};
