#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Party.h>

#include <CircurlarBuffer.h>
#include <ToolboxWindow.h>
#include <Utils/RateLimiter.h>

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

    void Initialize() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void Terminate() override;
    void RegisterSettingsContent() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
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
