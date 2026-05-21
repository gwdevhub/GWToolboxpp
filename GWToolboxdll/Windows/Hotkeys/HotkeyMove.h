#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
public:
    enum class MoveType : int {
        Location,
        Target
    } type = MoveType::Location;

    float x = 0.0;
    float y = 0.0;
    float range = 0.0;
    float distance_from_location = 0.0;
    char name[140]{};

    static const char* IniSection() { return "Move"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyMove(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
