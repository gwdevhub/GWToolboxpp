#pragma once

#include <ToolboxModule.h>

class VendorFix : public ToolboxModule {
    VendorFix() = default;
    ~VendorFix() override = default;

public:
    static VendorFix& Instance()
    {
        static VendorFix instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "商店修复"; }
    [[nodiscard]] const char* Description() const override { return "修复了后部物品栏中的可收集物品无法被商贩识别的问题"; }

    void Initialize() override;
    void Terminate() override;
    bool HasSettings() override { return false;  }
};
