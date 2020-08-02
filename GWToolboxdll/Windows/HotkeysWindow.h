#pragma once 

#include <Logger.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#include <Windows/Hotkeys.h>

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeysWindow : public ToolboxWindow {
    HotkeysWindow() {};
    ~HotkeysWindow() {};
public:
    static HotkeysWindow& Instance() {
        static HotkeysWindow instance;
        return instance;
    }

    const char* Name() const override { return "Hotkeys"; }

    void Initialize() override;
    void Terminate() override;
    
    inline bool ToggleClicker() { return clickerActive = !clickerActive; }
    inline bool ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
    inline bool ToggleRupt() { return ruptActive = !ruptActive; }
    inline void SetUseHotkey(HotkeyUseSkill *hk) { useskill_hotkey = hk; }

    TBHotkey* current_hotkey = nullptr;

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

    long max_id_ = 0;
    bool block_hotkeys = false;

    bool clickerActive = false;             // clicker is active or not
    bool dropCoinsActive = false;           // coin dropper is active or not

    bool map_change_triggered = false;

    clock_t clickerTimer = 0;               // timer for clicker
    clock_t dropCoinsTimer = 0;             // timer for coin dropper

    unsigned int ruptSkillID = 0;           // skill id of the skill to rupt
    unsigned int ruptSkillSlot = 0;         // skill slot of the skill to rupt with
    bool ruptActive = false;                // rupter active or not

    HotkeyUseSkill *useskill_hotkey = nullptr;

    float movementX = 0;                    // X coordinate of the destination of movement macro
    float movementY = 0;                    // Y coordinate of the destination of movement macro

    void MapChanged();
    uint32_t map_id = 0;
    uint32_t prof_id = 0;
};
