#pragma once

#include <ToolboxModule.h>

class FpsFix : public ToolboxModule {
    FpsFix() = default;
    ~FpsFix() override = default;

public:
    static FpsFix& Instance()
    {
        static FpsFix instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Fps Fix"; }
    [[nodiscard]] const char* Description() const override { return "Guild Wars limits FPS to 90 even if your monitor refresh rate is higher.\nThis module fixes it to allow 'Monitor Refresh Rate' in F11 settings to work."; }

    void Initialize() override;
    void Terminate() override;
    bool HasSettings() override { return false;  }
};
