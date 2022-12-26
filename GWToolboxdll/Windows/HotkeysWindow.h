#pragma once

#include <Timer.h>
#include <ToolboxWindow.h>

#include <Windows/Hotkeys.h>

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeysWindow : public ToolboxWindow {
    HotkeysWindow() = default;
    ~HotkeysWindow() = default;

public:
    static HotkeysWindow& Instance() {
        static HotkeysWindow instance;
        return instance;
    }

    const char* Name() const override { return "Hotkeys"; }
    const char* Icon() const override { return ICON_FA_KEYBOARD; }

    void Initialize() override;
    void Terminate() override;

    static bool ToggleClicker();
    static bool ToggleCoinDrop();

    static const TBHotkey* CurrentHotkey();

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;



private:



};
