#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>

class HintsModule : public ToolboxModule {
protected:
    bool block_repeat_attack_hint = true;
public:
    static HintsModule& Instance() {
        static HintsModule instance;
        return instance;
    }

    const char* Name() const override { return "Hints"; }
    const char* SettingsName() const override { return "In-Game Hints"; }

    void Initialize() override;
    void DrawSettingInternal() override;

    static void OnUIMessage(GW::HookStatus*, uint32_t message_id, void* wparam, void* lparam);

    GW::HookEntry hints_entry;
};
