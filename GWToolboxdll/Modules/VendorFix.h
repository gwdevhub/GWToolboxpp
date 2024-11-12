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

    [[nodiscard]] const char* Name() const override { return "Vendor Fix"; }
    [[nodiscard]] const char* Description() const override { return "Fixes bug preventing collectable items in latter inventory slots not being recognised by a vendor"; }

    void Initialize() override;
    void Terminate() override;
    bool HasSettings() override { return false;  }
};
