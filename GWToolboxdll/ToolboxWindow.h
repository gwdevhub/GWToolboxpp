#pragma once

#include <Defines.h>
#include <ToolboxUIElement.h>

class ToolboxWindow : public ToolboxUIElement {
public:
    [[nodiscard]] bool IsWindow() const override { return true; }
    [[nodiscard]] const char* TypeName() const override { return "window"; }

    void Initialize() override
    {
        ToolboxUIElement::Initialize();
        has_closebutton = true;
        has_titlebar = true;
    }
};
