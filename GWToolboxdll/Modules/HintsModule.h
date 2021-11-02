#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>

class HintsModule : public ToolboxModule {
protected:
    bool only_show_hints_once = false;
public:
    static HintsModule& Instance() {
        static HintsModule instance;
        return instance;
    }

    const char* Name() const override { return "Hints"; }
    const char* SettingsName() const override { return "In-Game Hints"; }

    void Initialize() override;
    void Update(float) override;
    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

    static void OnUIMessage(GW::HookStatus*, uint32_t message_id, void* wparam, void* lparam);

    GW::HookEntry hints_entry;
};
