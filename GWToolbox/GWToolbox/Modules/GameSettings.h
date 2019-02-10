#pragma once

class GameSettings : public ToolboxModule {
    GameSettings() {};
    ~GameSettings() {};
public:
    static GameSettings& Instance() {
        static GameSettings instance;
        return instance;
    }

    const char* Name() const override { return "Game Settings"; }

    void Initialize() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

    void DrawBorderlessSetting();
    void DrawFOVSetting();
    bool maintain_fov = false;
    float fov = 1.308997f; // default fov

    // some settings that are either referenced from multiple places
    // or have nowhere else to be
    enum { 
        Ok,
        WantBorderless,
        WantWindowed
    } borderless_status = Ok; // current actual status of borderless
    bool borderlesswindow = false; // status of the borderless checkbox and setting
    bool tick_is_toggle = false;

    bool openlinks = false;
    bool auto_url = false;
    // bool select_with_chat_doubleclick = false;
    bool move_item_on_ctrl_click = false;

    bool flash_window_on_pm = false;
    bool flash_window_on_party_invite = false;
    bool flash_window_on_zoning = false;
    bool focus_window_on_zoning = false;

    bool auto_set_away = false;
    int auto_set_away_delay = 10;
    bool auto_set_online = false;
    clock_t activity_timer = 0;

    bool auto_skip_cinematic = false;
    bool show_unlearned_skill = false;

    std::wstring afk_message;
    clock_t afk_message_time;

    void ApplyBorderless(bool value);
    void SetAfkMessage(std::wstring&& message);
    static void ItemClickCallback(uint32_t type, uint32_t slot, GW::Bag *bag);

    bool show_timestamps;
    Color timestamps_color;

private:
    void UpdateBorderless();
    void UpdateFOV();
    std::vector<GW::MemoryPatcher*> patches;
    GW::MemoryPatcher *ctrl_click_patch;
    GW::MemoryPatcher *tome_patch;

    void DrawChannelColor(const char *name, GW::Chat::Channel chan);
};
