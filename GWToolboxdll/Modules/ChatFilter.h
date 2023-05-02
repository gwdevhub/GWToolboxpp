#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>

class ChatFilter : public ToolboxModule {
    ChatFilter() {};
    ~ChatFilter() {};

public:
    static ChatFilter& Instance() {
        static ChatFilter instance;
        return instance;
    }

    const char* Name() const override { return "Chat Filter"; }
    const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

    void Update(float delta) override;
};
