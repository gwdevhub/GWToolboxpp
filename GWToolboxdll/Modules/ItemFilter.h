#pragma once

#include <vector>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/Packets/StoC.h>
#include <ToolboxModule.h>


class ItemFilter : public ToolboxModule {
public:
    static ItemFilter& Instance() {
        static ItemFilter instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Filter"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COINS; }
    [[nodiscard]] const char* SettingsName() const override { return "Item Settings"; }

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

private:

    
};
