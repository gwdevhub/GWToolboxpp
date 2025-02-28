#pragma once

#include "IRC.h"
#include <ToolboxModule.h>

class TwitchModule : public ToolboxModule {
    TwitchModule() = default;
    TwitchModule(const TwitchModule&) = delete;

    ~TwitchModule() override = default;

public:
    static TwitchModule& Instance()
    {
        static TwitchModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Twitch"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }
    [[nodiscard]] const char* Description() const override { return " - Show the live chat from a running Twitch stream directly in chat.\n - Send a whisper to 'Twitch' to send a message to Twitch from GW"; }
    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    static bool Connect();
    static void Disconnect();
    static bool IsConnected();
    static IRC* irc();
};
