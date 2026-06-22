#pragma once

#include <ToolboxModule.h>

// =============================================================================
// TestHarness  (DEBUG-only autonomous test driver)
//
// Lets an external operator drive the toolbox without manual in-game input, so a
// pathfinder can be iterated on in a tight loop: inject -> auto-login + set a
// fixed waypoint -> read log.txt -> signal shutdown (unloads the DLL, GW stays
// open) -> rebuild -> re-inject.
//
// Channel: three text files in the GWToolbox computer folder (next to log.txt):
//   harness_command.txt  - one command, consumed (deleted) once executed:
//                            waypoint <x> <y> <plane> | mode <0|1|2> | login | shutdown
//   harness_config.txt    - key=value, read each poll: char=, waypoint=, mode=, autostart=
//   harness_status.txt    - last action / state, written by the harness for the operator
//
// Update() runs on the game thread, so it calls game/pathing APIs directly.
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

    [[nodiscard]] const char* Name() const override { return "Test Harness"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Autonomous test driver (debug): file-command channel, auto-login, auto-waypoint.";
    }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Update(float delta) override;
    void Terminate() override;
};
