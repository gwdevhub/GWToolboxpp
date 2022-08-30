#pragma once

#include <ToolboxUIElement.h>

/*
A ToolboxWindow is a module which also has an interface
*/
class ToolboxWindow : public ToolboxUIElement {
public:
    bool IsWindow() const override { return true; }
    const char* TypeName() const override { return "window"; }

    virtual void Initialize() override {
        ToolboxUIElement::Initialize();
        has_closebutton = true;
    }

    virtual void LoadSettings(CSimpleIni* ini) override {
        ToolboxUIElement::LoadSettings(ini);
        lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
        lock_size = ini->GetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
        show_closebutton = ini->GetBoolValue(Name(), VAR_NAME(show_closebutton), show_closebutton);
    }

    virtual void SaveSettings(CSimpleIni* ini) override {
        ToolboxUIElement::SaveSettings(ini);
        ini->SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
        ini->SetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
        ini->SetBoolValue(Name(), VAR_NAME(show_closebutton), show_closebutton);
    }

    virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const;
};
