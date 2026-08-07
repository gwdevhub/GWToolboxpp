#pragma once

#include <ToolboxModule.h>


class ResignLogModule : public ToolboxModule {
    ResignLogModule() = default;
    ~ResignLogModule() override = default;

public:
    static ResignLogModule& Instance()
    {
        static ResignLogModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "登出记录"; }
    [[nodiscard]] const char* Description() const override { return "跟踪玩家在可探索区域的登出状态，添加 /resignlog 命令"; }
    bool HasSettings() override { return false; }

    struct Settings {
        bool show_last_to_resign_message = false;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void SignalTerminate() override;
    void Update(float) override;

    void RegisterSettingsContent() override;

    static bool PrintResignStatus(const uint32_t player_number, std::wstring& out, bool include_timestamp = false);
};
