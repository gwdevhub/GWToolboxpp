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
    ~HotkeyGroup() override;

    void Save(ToolboxIni* ini, const char* section) const override;

    // Overrides the outer draw to render the group header + its children
    bool Draw(Op* op, bool first = false, bool last = false) override;

    // Inner draw (expanded group settings – rename, etc.)
    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
