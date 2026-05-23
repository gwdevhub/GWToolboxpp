#pragma once

#include <vector>

#include <Windows/Hotkeys/TBHotkey.h>

// A named group of hotkeys that can be reordered as a unit.
// Extends TBHotkey so it can live in the same flat vector as individual hotkeys.
class HotkeyGroup : public TBHotkey {
public:
    std::vector<TBHotkey*> hotkeys;

    static const char* IniSection() { return "HotkeyGroup"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    // section may be null when creating a new group interactively
    HotkeyGroup(const ToolboxIni* ini, const char* section);
    HotkeyGroup(const char* label);
    ~HotkeyGroup() override;

    // Overrides the outer draw to render the group header + its children
    bool DrawSettings() override;

    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
