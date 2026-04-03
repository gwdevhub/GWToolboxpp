#pragma once

#include <ToolboxModule.h>

typedef long clock_t;

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
    static void BlockMessageForMs(const wchar_t* message_contains, clock_t ms);
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void Update(float delta) override;
};
