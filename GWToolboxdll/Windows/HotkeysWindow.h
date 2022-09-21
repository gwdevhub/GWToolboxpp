#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#include <Utils/GuiUtils.h>

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
    const char8_t* Icon() const override { return ICON_FA_KEYBOARD; }

    void Initialize() override;
    void Terminate() override;

    inline bool ToggleClicker() { return clickerActive = !clickerActive; }
    inline bool ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }

    TBHotkey* current_hotkey = nullptr;

    // Repopulates applicable_hotkeys based on current character/map context.
    // Used because its not necessary to check these vars on every keystroke, only when they change
    bool CheckSetValidHotkeys();

    bool IsMapReady();
    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

private:
    std::vector<TBHotkey*> hotkeys;             // list of hotkeys
    // Subset of hotkeys that are valid to current character/map combo
    std::vector<TBHotkey*> valid_hotkeys;

    // Ordered subsets
    enum GroupBy : int {
        None,
        Profession,
        Map,
        PlayerName,
    } group_by = None;
    std::map<int, std::vector<TBHotkey*>> by_profession;
    std::map<int, std::vector<TBHotkey*>> by_map;
    std::map<int, std::vector<TBHotkey*>> by_instance_type;
    std::map<std::string, std::vector<TBHotkey*>> by_player_name;
    bool need_to_check_valid_hotkeys = true;

    bool IsPvPCharacter();

    long max_id_ = 0;
    bool block_hotkeys = false;

    bool clickerActive = false;             // clicker is active or not
    bool dropCoinsActive = false;           // coin dropper is active or not

    bool map_change_triggered = false;

    clock_t clickerTimer = 0;               // timer for clicker
    clock_t dropCoinsTimer = 0;             // timer for coin dropper

    float movementX = 0;                    // X coordinate of the destination of movement macro
    float movementY = 0;                    // Y coordinate of the destination of movement macro

    bool HandleMapChange();
};
