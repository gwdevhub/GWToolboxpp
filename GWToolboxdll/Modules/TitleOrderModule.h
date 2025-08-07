#pragma once

#include <ToolboxModule.h>

class TitleOrderModule : public ToolboxModule {
    TitleOrderModule() = default;
    ~TitleOrderModule() override = default;

public:
    static TitleOrderModule& Instance()
    {
        static TitleOrderModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Title Order Module"; }
    [[nodiscard]] const char* Description() const override { return "Reorders the entries Titles tab of the Account window in-game"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CHART_LINE; }

    bool HasSettings() override { return false; }

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
};
