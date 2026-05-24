#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyFlagHero : public TBHotkey {
public:
    float degree = 0.0;
    float distance = 0.0;
    int hero = 0;

    static const char* IniSection() { return "FlagHero"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyFlagHero(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
