#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>
#include <minwindef.h>

namespace GW::Chat {
    struct ChatMessage;
}

class ChatLog : public ToolboxModule {
    ChatLog() = default;

    ~ChatLog() override;
public:
    static ChatLog& Instance()
    {
        static ChatLog instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Chat Log"; }
    [[nodiscard]] const char* Description() const override { return "Guild Wars doesn't save your chat history or sent messages if you log out of the game.\nTurn this feature on to let GWToolbox keep better track of your chat history between logins"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void RegisterSettingsContent() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void SetEnabled(bool _enabled);

};
