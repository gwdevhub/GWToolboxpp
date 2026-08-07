#pragma once

#include <ToolboxModule.h>

class PartyBroadcast : public ToolboxModule {
    PartyBroadcast() = default;
public:
    static PartyBroadcast& Instance()
    {
        static PartyBroadcast instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "队伍广播"; }
    [[nodiscard]] const char* Description() const override { return "将搜索结果发布至https://party.gwtoolbox.com"; }
    [[nodiscard]] const char* SettingsName() const override { return "游戏设置"; }

    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Update(float) override;
    bool HasSettings() override { return false; }
};
