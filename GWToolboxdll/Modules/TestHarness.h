#pragma once

#include <ToolboxModule.h>

class TestHarness : public ToolboxModule {
    TestHarness() = default;
    ~TestHarness() override = default;

public:
    static TestHarness& Instance()
    {
        static TestHarness instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Test Harness"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Autonomous test driver (debug): file-command channel.";
    }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Update(float delta) override;
    void Terminate() override;
};
