#pragma once

#include <unordered_map>

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to toggle a toolbox function
class HotkeyToggle : public TBHotkey {
    enum ToggleTarget {
        Clicker,
        Pcons,
        CoinDrop,
        Tick,
        Count
    } target = Clicker;

    static const char* GetText(void*, int idx);

public:
    static bool IsValid(const ToolboxIni* ini, const char* section);
    static const char* IniSection() { return "Toggle"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyToggle(const ToolboxIni* ini, const char* section);
    ~HotkeyToggle() override;
    void Save(ToolboxIni* ini, const char* section) const override;

    // Used to ensure that clicker doesn't clog up the event queue
    static inline bool processing = false;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
    void Toggle() override;
    bool IsToggled(bool refresh = false);

private:
    [[nodiscard]] bool HasInterval() const { return target == Clicker || target == CoinDrop; }
    WORD togglekey = VK_LBUTTON;
    bool is_key_down = false;
    clock_t last_use = 0;
    // ms between uses - min 50ms, max 30s
    int interval = 50;
    static std::unordered_map<WORD, HotkeyToggle*> toggled;
};
