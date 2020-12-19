#pragma once

#include <GWCA/Packets/StoC.h>

#include <CircurlarBuffer.h>
#include <ToolboxWindow.h>
#include <Utils/RateLimiter.h>

class PartySearchWindow : public ToolboxWindow {
    PartySearchWindow() {};
    PartySearchWindow(const PartySearchWindow&) = delete;
    ~PartySearchWindow();
public:
    static PartySearchWindow& Instance() {
        static PartySearchWindow instance;
        return instance;
    }

    const char* Name() const override { return "Party Search"; }
    const char* Icon() const override { return ICON_FA_PEOPLE_ARROWS; }

    void Initialize() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

private:
    struct Message {
        uint32_t    timestamp = 0;
        std::string name;
        std::string message;
    };
    struct TBParty {
        TBParty() {
            message[0] = 0;
            player_name[0] = 0;
        }
        static uint32_t IdFromRegionParty(uint32_t party_id);
        static uint32_t IdFromLocalParty(uint32_t party_id);
        bool FromRegionParty(GW::PartySearch*);
        bool FromLocalParty(GW::PartyInfo*);
        uint32_t concat_party_id;
        uint8_t party_size;
        uint8_t hero_count;
        uint8_t search_type; // 0 = hunting, 1 = mission, 2 = quest, 3 = trade, 4 = guild
        uint8_t is_hard_mode;
        uint16_t map_id;
        int8_t region_id; // This is set by the player's map
        int8_t district;
        int8_t language;
        std::string message;
        std::string player_name;
    };
    GW::HookEntry OnMessageLocal_Entry;

    std::unordered_map<uint32_t,TBParty*> party_advertisements;

    WSAData wsaData = {0};

    bool show_alert_window = false;
    std::recursive_mutex party_mutex;

    // Window could be visible but collapsed - use this var to check it.
    bool collapsed = false;

    #define ALERT_BUF_SIZE 1024 * 16
    char alert_buf[ALERT_BUF_SIZE] = "";
    // set when the alert_buf was modified
    bool alertfile_dirty = false;
    bool print_game_chat = false;
    bool filter_alerts = false;
    char search_buffer[256] = { 0 };
    std::vector<std::string> alert_words;
    std::vector<std::string> searched_words;
    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;
    bool should_stop = false;
    std::thread worker;
    bool ws_window_connecting = false;
    easywsclient::WebSocket* ws_window = NULL;
    RateLimiter window_rate_limiter;
    
    CircularBuffer<Message> messages;

    TBParty* GetParty(uint32_t party_id);
    void ClearParties();
    void FillParties();
    void DrawAlertsWindowContent(bool ownwindow);
    void AsyncWindowConnect(bool force = false);
    void fetch();
    static bool parse_json_message(const nlohmann::json& js, Message* msg);
    void ParseBuffer(const char *text, std::vector<std::string> &words);
    static void DeleteWebSocket(easywsclient::WebSocket *ws);
    bool IsLfpAlert(std::string& message);
    static void OnRegionPartyUpdated(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet);
};
