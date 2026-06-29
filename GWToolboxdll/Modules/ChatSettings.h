#pragma once

#include <ToolboxModule.h>

namespace GW::Chat {
    enum Channel : int;
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
    [[nodiscard]] bool IsDecoded() const { return !output_message.empty() && !output_sender.empty(); }

    bool Consume()
    {
        if (print) {
            return PrintMessage();
        }
        if (send) {
            return Send();
        }
        return false;
    }

    [[nodiscard]] bool IsSend() const { return send; }
    static bool Cooldown();
    bool invalid = true; // Set when we can't find the agent name for some reason, or arguments passed are empty.

protected:
    std::vector<std::wstring> SanitiseForSend() const;
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
    ~ChatSettings() override = default;

public:
    static ChatSettings& Instance()
    {
        static ChatSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Chat Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COMMENTS; }

    struct Settings {
        bool show_timestamps = false;
        bool hide_player_speech_bubbles = false;
        bool hide_all_friendly_speech_bubbles = false;
        bool show_timestamp_seconds = false;
        bool show_timestamp_24h = false;
        bool npc_speech_bubbles_as_chat = false;
        bool redirect_npc_messages_to_emote_chat = false;
        bool redirect_outgoing_whisper_to_whisper_channel = false;
        bool openlinks = true;
        bool auto_url = true;
        bool clear_chat_message_when_hiding_chat = false;
        int chat_window_font_id_index = 0;
        Colors::SettingColor timestamps_color = Colors::RGB(0xc0, 0xc0, 0xbf);
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static void AddPendingMessage(PendingChatMessage* pending_chat_message);
    static void SetAfkMessage(std::wstring&& message);
};
