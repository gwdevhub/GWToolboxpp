#pragma once

#include <ToolboxWindow.h>

class TradeWindow : public ToolboxWindow {
    TradeWindow() : ToolboxWindow() { show_menubutton = can_show_in_main_window; }
    TradeWindow(const TradeWindow&) = delete;
    ~TradeWindow() override = default;

public:
    static TradeWindow& Instance()
    {
        static TradeWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Trade"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BALANCE_SCALE; }

    struct Settings {
        bool print_game_chat = false;
        bool print_game_chat_asc = false;
        bool filter_alerts = false;
        bool filter_local_trade = false;
        bool is_kamadan_chat = true;
        std::string player_party_search_text;
    };

    void Initialize() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void Terminate() override;
    void RegisterSettingsContent() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    void DrawChatSettings(bool ownwindow = false);

    static void FindPlayerPartySearch(GW::HookStatus* status = nullptr, void* packet = nullptr);

private:
    

    void DrawAlertsWindowContent(bool ownwindow);

    static bool GetInKamadanAE1(bool check_district = true);
    static bool GetInAscalonAE1(bool check_district = true);

    // Since we are connecting in an other thread, the following attributes/methods avoid spamming connection requests
    void AsyncWindowConnect(bool force = false);


    void fetch();

    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs{};
    bool should_stop = false;
    std::thread* worker = nullptr;

    static void ParseBuffer(const char* text, std::vector<std::string>& words);
    static void ParseBuffer(std::fstream stream, std::vector<std::string>& words);

    static void DeleteWebSocket(easywsclient::WebSocket* ws);
    void SwitchSockets();
};
