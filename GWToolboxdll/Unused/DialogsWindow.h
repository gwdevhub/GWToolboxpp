#pragma once

#include <ToolboxWindow.h>

class DialogsWindow : public ToolboxWindow {
    DialogsWindow() = default;
    ~DialogsWindow() = default;

public:
    static DialogsWindow& Instance() {
        static DialogsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Dialogs"; }
    const char* Icon() const override { return ICON_FA_COMMENT_DOTS; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
};
