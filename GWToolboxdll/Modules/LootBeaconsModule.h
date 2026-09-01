#pragma once

#include <ToolboxModule.h>

class LootBeaconsModule : public ToolboxModule {
    LootBeaconsModule() = default;
    ~LootBeaconsModule() override = default;

public:
    static LootBeaconsModule& Instance()
    {
        static LootBeaconsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "战利品标记"; }
    [[nodiscard]] const char* Description() const override
    {
        return "根据稀有度或交易价格，在游戏中为高价值掉落物绘制光柱。";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GEM; }

    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawInWorld(IDirect3DDevice9* device);
};
