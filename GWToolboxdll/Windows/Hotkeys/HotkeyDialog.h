#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyDialog : public TBHotkey {
public:
    uint32_t id = 0;
    char name[140]{};

    static const char* IniSection() { return "Dialog"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyDialog(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
