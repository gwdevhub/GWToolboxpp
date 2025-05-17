#pragma once

#include <ToolboxWidget.h>

class HeroEquipmentModule : public ToolboxWidget {
    HeroEquipmentModule() = default;
    ~HeroEquipmentModule() override = default;

public:
    static HeroEquipmentModule& Instance()
    {
        static HeroEquipmentModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hero Equipment"; }
    [[nodiscard]] const char* Description() const override { return "Allows ability for hero equipment to be viewed separately from the in-game Inventory window.\nAdds an inventory icon to title bar of hero panels."; }

    bool HasSettings() override { return false; }
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Update(float) override;
    void SaveSettings(ToolboxIni*) override;
    void LoadSettings(ToolboxIni*) override;
    void Draw(IDirect3DDevice9*) override;
};
