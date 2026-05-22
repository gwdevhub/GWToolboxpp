#pragma once

#include <GWCA/Constants/Skills.h>

#include <Windows/Hotkeys/TBHotkey.h>

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
    enum SkillIndex {
        Recall,
        UA,
        HolyVeil,
        Other
    };

    static bool GetText(void* data, int idx, const char** out_text);
    [[nodiscard]] SkillIndex GetIndex() const;

public:
    GW::Constants::SkillID id;

    static const char* IniSection() { return "DropUseBuff"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyDropUseBuff(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
