#pragma once

#include <ToolboxUIElement.h>

class ToolboxWidget : public ToolboxUIElement {
public:
    ToolboxWidget() { lock_move = lock_size = true; }
    ~ToolboxWidget() override = default;

    [[nodiscard]] bool IsWidget() const override { return true; }
    [[nodiscard]] const char* TypeName() const override { return "widget"; }

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const;
};
