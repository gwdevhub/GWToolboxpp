#pragma once

#include <ToolboxModule.h>

namespace GW {
    namespace Chat {
        enum Channel : int;
    }
}

class PendingChatMessage {
protected:
    bool printed = false;
    bool print = true;
    bool send = false;

    wchar_t encoded_message[256] = {'\0'};
    wchar_t encoded_sender[32] = {'\0'};
    std::wstring output_message;
    std::wstring output_sender;
    GW::Chat::Channel channel;

public:
    PendingChatMessage(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    static PendingChatMessage* queueSend(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    static PendingChatMessage* queuePrint(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    void SendIt()
    {
        print = false;
        send = true;
    }
    static bool IsStringEncoded(const wchar_t* str) { return str && (str[0] < L' ' || str[0] > L'~'); }
    bool IsDecoded() const { return !output_message.empty() && !output_sender.empty(); }
    bool Consume()
    {
        if (print) return PrintMessage();
        if (send) return Send();
        return false;
    }
    bool IsSend() const { return send; }
    static bool Cooldown();
    bool invalid = true; // Set when we can't find the agent name for some reason, or arguments passed are empty.

protected:
    std::vector<std::wstring> SanitiseForSend();
    bool PrintMessage();
    bool Send();
    void Init();
};

struct PlayerChatMessage {
    uint32_t channel;
    wchar_t* message;
    uint32_t player_number;
};

class ChatSettings : public ToolboxModule {
    ChatSettings() = default;
    ~ChatSettings() = default;

public:
    static ChatSettings& Instance()
    {
        static ChatSettings instance;
        return instance;
    }

    const char* Name() const override { return "Chat Settings"; }
    const char* Icon() const override { return ICON_FA_COMMENTS; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static void AddPendingMessage(PendingChatMessage* pending_chat_message);
    static void SetAfkMessage(std::wstring&& message);
};
