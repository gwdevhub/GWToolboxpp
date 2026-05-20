#pragma once

#include <GWCA/GameEntities/Hero.h>

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyCommandPet : public TBHotkey {
public:
    GW::HeroBehavior behavior = static_cast<GW::HeroBehavior>(0);

    static const char* IniSection() { return "CommandPet"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyCommandPet(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
