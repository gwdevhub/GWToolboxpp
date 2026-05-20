#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyGWKey : public TBHotkey {
    GW::UI::ControlAction action = GW::UI::ControlAction::ControlAction_ActivateWeaponSet1;
    int action_idx = -1;

public:
    static std::vector<std::pair<GW::UI::ControlAction, std::unique_ptr<GuiUtils::EncString>>> control_labels;

    static const char* IniSection() { return "GWHotkey"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyGWKey(const ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};
