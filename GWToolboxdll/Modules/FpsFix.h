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

    [[nodiscard]] const char* Name() const override { return "帧率修复"; }
    [[nodiscard]] const char* Description() const override { return "《激战》将FPS限制为90，如果你的显示器刷新率更高；可以修复了这个问题。"; }

    void Initialize() override;
    void Terminate() override;
    bool HasSettings() override { return false;  }
};
