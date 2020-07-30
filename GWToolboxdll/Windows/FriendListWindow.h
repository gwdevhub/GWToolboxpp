#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <GuiUtils.h>
#include <ToolboxWindow.h>

class FriendListWindow : public ToolboxWindow {
private:
    FriendListWindow();
    ~FriendListWindow();
    // Structs because we don't case about public or private; this whole struct is private to this module anyway.
    struct Character {
        std::wstring name;
        uint8_t profession = 0;
    };

    struct Friend {
        Friend(FriendListWindow* _parent) : parent(_parent) {};
        ~Friend() {
            characters.clear();
        };
        std::string uuid;
        UUID uuid_bytes{};
        std::wstring alias;
        FriendListWindow* parent;
        Character* current_char = nullptr;
        char current_map_name[128] = { 0 };
        uint32_t current_map_id = 0;
        std::unordered_map<std::wstring, Character> characters{};
        uint8_t status = static_cast<uint8_t>(GW::FriendStatus::FriendStatus_Offline); // 0 = Offline, 1 = Online, 2 = Do not disturb, 3 = Away
        uint8_t type = static_cast<uint8_t>(GW::FriendType::FriendType_Unknow);
        bool is_tb_friend = false;  // Is this a friend via toolbox, or friend via friend list?
        bool has_tmp_uuid = false;
        clock_t added_via_toolbox = 0; // This friend added via toolbox? When?
        clock_t last_update = 0;
        std::string cached_charnames_hover_str;
        bool cached_charnames_hover = false;
        Character* GetCharacter(const wchar_t*);
        Character* SetCharacter(const wchar_t*, uint8_t);
        GW::Friend* GetFriend();
        void GetMapName();
        const std::string GetCharactersHover(bool include_charname = false);
        void StartWhisper();
        void InviteToParty();
        bool AddGWFriend();
        bool RemoveGWFriend();
        const bool IsOffline() {
            return status < GW::FriendStatus::FriendStatus_Online || status > GW::FriendStatus::FriendStatus_Away;
        };
        const bool NeedToUpdate(clock_t now) {
            return (now - last_update) > 10000; // 10 Second stale.
        }
    };

    Friend* SetFriend(uint8_t*, GW::FriendType, GW::FriendStatus, uint32_t, const wchar_t*, const wchar_t*);
    Friend* SetFriend(GW::Friend*);
    Friend* GetFriend(const wchar_t*);
    Friend* GetFriend(GW::Friend*);
    Friend* GetFriendByUUID(const char*);
    Friend* GetFriend(uint8_t*);

    void RemoveFriend(Friend* f);
    void LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out);
public:
    static FriendListWindow& Instance() {
        static FriendListWindow instance;
        return instance;
    }

    static void FriendStatusCallback(
        GW::HookStatus*,
        GW::Friend* f,
        GW::FriendStatus status,
        const wchar_t* alias,
        const wchar_t* charname);

    const char* Name() const override { return "Friend List"; }

    static void CmdAddFriend(const wchar_t* message, int argc, LPWSTR* argv);
    static void CmdRemoveFriend(const wchar_t* message, int argc, LPWSTR* argv);

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    bool ShowAsWidget() const;
    bool ShowAsWindow() const;
    void DrawHelp() override;
    void RegisterSettingsContent() override;
    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags=0) const;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Check friends list.
    void Poll();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void DrawChatSettings();

    void LoadFromFile();
    void SaveToFile();

private:
    CSimpleIni* inifile = nullptr;
    wchar_t* ini_filename = L"friends.ini";
    bool loading = true; // Loading from disk?
    bool polling = false; // Polling in progress?
    bool poll_queued = false; // Used to avoid overloading the thread queue.
    bool should_stop = false;
    bool friends_changed = false;
    bool first_load = false;
    bool pending_friend_removal = false;
    bool friend_list_ready = false; // Allow processing when this is true.
    bool need_to_reorder_friends = true;
    bool show_alias_on_whisper = false;
    bool show_my_status = true;

    int explorable_show_as = 1;
    int outpost_show_as = 1;

    bool resize_widget = false;

    bool lock_move_as_widget = false;
    bool lock_size_as_widget = true;

    Color hover_background_color = 0x33999999;

    clock_t friends_list_checked = 0;

    uint8_t poll_interval_seconds = 10;

    // Mapping of Name > UUID
    std::unordered_map<std::wstring, std::string> uuid_by_name;

    // Main store of Friend info
    std::unordered_map<std::string, Friend*> friends;

    bool show_location = true;

    GW::HookEntry FriendStatusUpdate_Entry;
    GW::HookEntry ErrorMessage_Entry;
    GW::HookEntry PlayerJoinInstance_Entry;
};
