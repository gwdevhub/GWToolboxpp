#pragma once

#include <ToolboxModule.h>


class CodeOptimiserModule : public ToolboxModule {
    CodeOptimiserModule() = default;
    ~CodeOptimiserModule() override = default;

public:
    static CodeOptimiserModule& Instance()
    {
        static CodeOptimiserModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "CodeOptimiserModule"; }
    bool HasSettings() override { return false; }


    const char* Description() const override { return "Optimises some internal GW functions to improve in-game performance"; }
    void Initialize() override;
    void SignalTerminate() override;
};
