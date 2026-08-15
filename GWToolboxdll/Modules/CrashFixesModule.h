#pragma once

#include <ToolboxModule.h>

class CrashFixesModule : public ToolboxModule {
    CrashFixesModule() = default;
    ~CrashFixesModule() override = default;

public:
    static CrashFixesModule& Instance()
    {
        static CrashFixesModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Crash Fixes"; }
    [[nodiscard]] const char* Description() const override { return "Patches bugs/crashes with the base game, added to toolbox to document it for fixing in the base game."; }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
};
