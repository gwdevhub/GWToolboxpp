#pragma once

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
    char message[139]{};
    char channel = '/';

public:
    static const char* IniSection() { return "SendChat"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeySendChat(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
