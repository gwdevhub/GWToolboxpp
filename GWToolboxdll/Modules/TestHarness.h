#pragma once

#include <ToolboxModule.h>

// =============================================================================
// TestHarness  (DEBUG-only autonomous test driver)
//
// Lets an external operator drive the toolbox without in-game input, so an
// investigation can run as a loop: inject -> command -> read log.txt ->
// shutdown (unloads the DLL, GW stays open) -> rebuild -> re-inject.
//
// Channel: two text files in the GWToolbox computer folder (next to log.txt):
//   harness_command.txt  - one command, consumed once executed:
//                            shutdown | status | travel <mapid> | cartoprobe <cx> <cy>
//   harness_status.txt   - last action / state, written for the operator
//
// Update() runs on the game thread, so it calls game APIs directly.
// =============================================================================
class TestHarness : public ToolboxModule {
    TestHarness() = default;
    ~TestHarness() override = default;

public:
    static TestHarness& Instance()
    {
        static TestHarness instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "测试工具集"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Autonomous test driver (debug): file-command channel.";
    }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Update(float delta) override;
    void Terminate() override;
};
