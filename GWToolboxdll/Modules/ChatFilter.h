#pragma once

#include <ToolboxModule.h>

class ChatFilter : public ToolboxModule {
    ChatFilter() = default;

    ~ChatFilter() override = default;

public:
    static ChatFilter& Instance()
    {
        static ChatFilter instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Chat Filter"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void Update(float delta) override;
};
