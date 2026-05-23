#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyAction : public TBHotkey {
    enum Action : uint8_t {
        OpenXunlaiChest,
        DropGoldCoin = 2,
        ReapplyTitle,
        EnterChallenge,
    };

    static const char* GetText(void*, int idx);

public:
    Action action = OpenXunlaiChest;

    static const char* IniSection() { return "Action"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyAction(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
