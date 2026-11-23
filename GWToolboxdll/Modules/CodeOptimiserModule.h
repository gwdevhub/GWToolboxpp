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

    [[nodiscard]] const char* Name() const override { return "Code Optimiser"; }
    bool HasSettings() override { return false; }


    const char* Description() const override { return "Optimises some internal GW functions to improve in-game performance"; }
    static uint32_t Crc32(const void* data, size_t bytes, uint32_t seed = 0);

    void Initialize() override;
    void SignalTerminate() override;
};
