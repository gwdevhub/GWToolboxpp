#pragma once

#include <map>
#include <string>

#include <ToolboxWindow.h>

#include <Windows/Hotkeys.h>

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeysWindow : public ToolboxWindow {
    HotkeysWindow() = default;
    ~HotkeysWindow() override = default;

public:
    static HotkeysWindow& Instance()
    {
        static HotkeysWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hotkeys"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_KEYBOARD; }

    struct Settings {
        bool show_active_in_header = false;
        bool show_run_in_header = false;
        int clicker_delay_ms = 20;
    };
    Settings settings;

    // One hotkey in persisted form: legacy type name + the key/value pairs its INI serializer writes
    struct HotkeyEntry {
        std::string type;
        std::map<std::string, std::string> fields;
    };

    void Initialize() override;
    void Terminate() override;

    static bool ToggleClicker();
    static bool ToggleCoinDrop();

    static void ChooseKeyCombo(TBHotkey* hotkey);

    static const TBHotkey* CurrentHotkey();

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
