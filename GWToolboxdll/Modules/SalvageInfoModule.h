#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameEntities/Item.h>
#include <string.h>

class SalvageInfoModule : public ToolboxModule {
    SalvageInfoModule() = default;
    ~SalvageInfoModule() override = default;

public:
    static SalvageInfoModule& Instance()
    {
        static SalvageInfoModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Salvage Info"; }
    [[nodiscard]] const char* Description() const override { return "Adds salvage information into crafting materials to the hover description of an item"; }
    bool HasSettings() override { return true; }
    [[nodiscard]] const char* SettingsName() const override { return "Inventory Settings"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(ToolboxIni* ini) override;
};
