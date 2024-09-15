#pragma once

#include <ToolboxModule.h>

class LoginModule : public ToolboxModule {
    LoginModule() = default;
    ~LoginModule() override = default;

public:
    static LoginModule& Instance()
    {
        static LoginModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Login Module"; }
    [[nodiscard]] const char* Description() const override { return "Allows fix to allow reconnect when starting GW with charname argument.\nAdds feature to choose login scene"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_DOOR_OPEN; }

    bool HasSettings() override;

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;
};
