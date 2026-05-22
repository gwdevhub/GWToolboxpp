#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
    UINT item_id = 0;
    char name[140]{};

    UINT bag_idx = 0;
    UINT slot_idx = 0;

    enum UseBy : int {
        ITEM,
        SLOT
    } use_by = ITEM;

public:
    static const char* IniSection() { return "UseItem"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyUseItem(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool CanUse() override { return TBHotkey::CanUse() && (use_by == SLOT || item_id != 0); }

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
