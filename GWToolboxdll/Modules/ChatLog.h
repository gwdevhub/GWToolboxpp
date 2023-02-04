#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>
#include <minwindef.h>

namespace GW {
    namespace Chat {
        struct ChatMessage;
    }
}

class ChatLog : public ToolboxModule {
    ChatLog() {};
    ~ChatLog() {
        Reset();
    };
private:
    struct TBChatMessage {
        TBChatMessage* next;
        TBChatMessage* prev;
        std::wstring msg;
        uint32_t channel;
        FILETIME timestamp;
        TBChatMessage(wchar_t* _message, uint32_t _channel, FILETIME _timestamp) {
            msg = _message;
            timestamp = _timestamp;
            channel = _channel;
        }
    };
    struct TBSentMessage {
        TBSentMessage* next;
        TBSentMessage* prev;
        std::wstring msg;
        uint32_t gw_message_address = 0; // Used to ensure that messages aren't logged twice
        TBSentMessage(wchar_t* _message, uint32_t addr = 0) {
            msg = _message;
            gw_message_address = addr ? addr : (uint32_t)_message;
        }
    };
    TBChatMessage* recv_first = 0;
    TBChatMessage* recv_last = 0;
    TBSentMessage* sent_first = 0;
    TBSentMessage* sent_last = 0;
    size_t recv_count = 0;
    size_t sent_count = 0;
    std::wstring account;
    bool injecting = false;
    bool enabled = true;
    bool pending_inject = false;
    void SetEnabled(bool _enabled);
    void Reset();
    // Collect current in-game logs and combine them with the tb logs
    void Fetch();
    // Remove message from incoming log
    void Remove(TBChatMessage* message);
    // Add message to incoming log
    void Add(GW::Chat::ChatMessage* in);
    // Add message to outgoing log
    void AddSent(wchar_t* _message, uint32_t addr = 0);
    // Remove message from outgoing log
    void RemoveSent(TBSentMessage* message);
    // Add message to incoming log
    void Add(wchar_t* _message, uint32_t _channel, FILETIME _timestamp);
    void Save();
    // Path to chat log file on disk
    std::filesystem::path LogPath(const wchar_t* prefix);
    // Load chat log from file via account email address
    void Load(const std::wstring& _account);
    // Clear current chat log and prefill from tb chat log; chat box will update on map change
    void Inject();
    void InjectSent();
    // Set up chat log, load from file if applicable. Returns true if initialised
    bool Init();
    // Check outgoing log to see if message has already been added
    bool IsAdded(wchar_t* _message, uint32_t addr) {
        if (!addr)
            addr = (uint32_t)_message;
        TBSentMessage* sent = sent_last;
        while (sent) {
            // NB: GW uses TList in memory which means the only time the address will be nuked is when the log is cleared anyway
            if (sent->gw_message_address == addr && wcscmp(sent->msg.c_str(), _message) == 0)
                return true;
            if (sent == sent_first)
                break;
            sent = sent->prev;
        }
        return false;
    }

    
#pragma warning(push)
#pragma warning(disable: 4200)
    struct GWSentMessage {
        GWSentMessage* prev;
        GWSentMessage* next;
        wchar_t message[0];
    };
#pragma warning(pop)

    struct GWSentLog {
        size_t count;
        GWSentMessage* prev;
    };

    uintptr_t gw_sent_log_ptr = 0;
    GWSentLog* GetSentLog() {
        return gw_sent_log_ptr ? (GWSentLog*)(gw_sent_log_ptr - 0x4) : 0;
    }

    TBChatMessage* timestamp_override_message = 0;

    GW::HookEntry PreAddToChatLog_entry;
    GW::HookEntry PostAddToChatLog_entry;
    GW::HookEntry UIMessage_Entry;

public:
    static ChatLog& Instance() {
        static ChatLog instance;
        return instance;
    }

    const char* Name() const override { return "Chat Log"; }
    const char* Description() const override { return "Guild Wars doesn't save your chat history or sent messages if you log out of the game.\nTurn this feature on to let GWToolbox keep better track of your chat history between logins"; }
    const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void RegisterSettingsContent() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    static void OnAddToSentLog(wchar_t* message);
    // On fresh chat log, inject toolbox message history
    static void OnPreAddToChatLog(GW::HookStatus*, wchar_t*, uint32_t, GW::Chat::ChatMessage*);
    // After adding chat log message, also add it to tb chat log.
    static void OnPostAddToChatLog(GW::HookStatus*, wchar_t*, uint32_t, GW::Chat::ChatMessage* logged_message);


};
