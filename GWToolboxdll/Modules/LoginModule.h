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
    const char* Name() const override { return "Login Screen"; }

    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
};
