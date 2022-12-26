#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Party.h>

#include <CircurlarBuffer.h>
#include <ToolboxWindow.h>
#include <Utils/RateLimiter.h>

class TradeWindow : public ToolboxWindow {
    TradeWindow() = default;
    TradeWindow(const TradeWindow&) = delete;
    ~TradeWindow() = default;

public:
    static TradeWindow& Instance() {
        static TradeWindow instance;
        return instance;
    }

    const char* Name() const override { return "Trade"; }
    const char* Icon() const override { return ICON_FA_BALANCE_SCALE; }

    void Initialize() override;
    static void CmdPricecheck(const wchar_t* message, int argc, LPWSTR* argv);
    static void OnMessageLocal(GW::HookStatus *status, GW::Packet::StoC::MessageLocal *pak);

    bool IsTradeAlert(std::string &message);
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;
    void RegisterSettingsContent() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    void DrawChatSettings(bool ownwindow = false);

    static void FindPlayerPartySearch(GW::HookStatus* status = nullptr, void* packet = nullptr);

private:
    struct Message {
        uint32_t    timestamp = 0;
        std::string name;
        std::string message;
    };

    GW::HookEntry OnMessageLocal_Entry;
    GW::HookEntry OnPartySearch_Entry;
    GW::PartySearch player_party_search = { 0 };
    char player_party_search_text[64] = { 0 };

    WSAData wsaData = {0};

    bool is_kamadan_chat = true;
    bool refresh_footer = false;

    bool show_alert_window = false;

    // Window could be visible but collapsed - use this var to check it.
    bool collapsed = false;

    // if we need to print in the chat
    bool print_game_chat = false;
    bool print_game_chat_asc = false;

    // if enable, we won't print the messages containing word from alert_words
    bool filter_alerts = false;

    // if enabled, will also apply the trade alerts filter to incoming local trade chat messages.
    bool filter_local_trade = false;

    #define ALERT_BUF_SIZE 1024 * 16
    char alert_buf[ALERT_BUF_SIZE] = "";
    // set when the alert_buf was modified
    bool alertfile_dirty = false;

    std::string pending_query_string;
    clock_t pending_query_sent = 0;
    bool print_search_results = false;

    char search_buffer[256] = { 0 };

    std::vector<std::string> alert_words;
    std::vector<std::string> searched_words;

    void DrawAlertsWindowContent(bool ownwindow);

    static bool GetInKamadanAE1(bool check_district = true);
    static bool GetInAscalonAE1(bool check_district = true);

    // Since we are connecting in an other thread, the following attributes/methods avoid spamming connection requests
    void AsyncWindowConnect(bool force = false);
    bool ws_window_connecting = false;

    easywsclient::WebSocket *ws_window = NULL;

    RateLimiter window_rate_limiter;

    void search(std::string, bool print_results_in_chat = false);
    void fetch();

    static bool parse_json_message(const nlohmann::json& js, Message* msg);
    CircularBuffer<Message> messages;

    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;
    bool should_stop = false;
    std::thread worker;

    void ParseBuffer(const char *text, std::vector<std::string> &words);
    void ParseBuffer(std::fstream stream, std::vector<std::string>& words);

    static void DeleteWebSocket(easywsclient::WebSocket *ws);
    void SwitchSockets();
};
