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

    [[nodiscard]] const char* Name() const override { return "登录模块"; }
    [[nodiscard]] const char* Description() const override { return "允许在启动激战时使用charname参数进行重新连接。"; }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Update(float) override;
    void Terminate() override;
};
