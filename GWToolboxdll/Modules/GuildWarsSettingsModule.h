#pragma once

#include <ToolboxModule.h>

class GuildWarsSettingsModule : public ToolboxModule {
    GuildWarsSettingsModule() = default;
    ~GuildWarsSettingsModule() override = default;

public:
    static GuildWarsSettingsModule& Instance()
    {
        static GuildWarsSettingsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_CHECK_SQUARE; }
    [[nodiscard]] const char* Name() const override { return "激战游戏设置"; }
    [[nodiscard]] const char* Description() const override { return "将《激战》游戏设置保存或加载到本地磁盘上的文件中。"; }

    void Initialize() override;
    void Terminate() override;
    void DrawSettingsInternal() override;
};
