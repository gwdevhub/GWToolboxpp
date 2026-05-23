#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
    const uint32_t types[3] = {0xDB, 0x200, 0x400};
    const char* type_labels[3] = {"NPC", "Signpost", "Item"};
    const char* identifier_labels[3] = {"Model ID or Name", "Gadget ID or Name", "Item ModelID or Name"};

    enum HotkeyTargetType : int {
        NPC,
        Signpost,
        Item,
        Count
    } type = NPC;

    char id[140] = "nearest";
    char name[140] = "";

public:
    static const char* IniSection() { return "Target"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyTarget(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
