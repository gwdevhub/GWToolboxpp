#pragma once

#include <ToolboxModule.h>

class ItemTooltipModule : public ToolboxModule {
    ItemTooltipModule() = default;
    ~ItemTooltipModule() = default;

public:
    static ItemTooltipModule& Instance()
    {
        static ItemTooltipModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Tooltip"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TAG; }

    void Initialize() override;
    void Terminate() override;

    void Update(float) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    void DrawSettingsInternal() override;
    void RegisterSettingsContent() override;
};
