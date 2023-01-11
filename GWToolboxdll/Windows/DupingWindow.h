#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <ToolboxWindow.h>

class DupingWindow : public ToolboxWindow {
    DupingWindow() = default;
    ~DupingWindow() = default;
public:
    static DupingWindow& Instance() {
        static DupingWindow instance;
        return instance;
    }

    const char* Name() const override { return "Duping"; }
    const char* Icon() const override { return ICON_FA_COPY; }

    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override {}

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
