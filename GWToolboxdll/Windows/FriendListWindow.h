#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>

class FriendListWindow : public ToolboxWindow {
public:
    static FriendListWindow& Instance() {
        static FriendListWindow instance;
        return instance;
    }
protected:
    // Structs because we don't case about public or private; this whole struct is private to this module anyway.
    struct Character {
    private:
        std::wstring name;
        std::string name_str;

    public:
        uint8_t profession = 0;
        const std::string& getNameA() {
            if (name_str.empty() && !name.empty()) {
                name_str = GuiUtils::WStringToString(name);
            }
            return name_str;
        }
        const std::wstring& getNameW() const {
            return name;
        }
        void setName(std::wstring _name) {
            if (name == _name)
                return;
            name = std::move(_name);
            name_str.clear();
        }
    };
    struct UIChatMessage {
        uint32_t channel;
        wchar_t* message;
        uint32_t channel2;
    };
public:
    struct Friend {
        Friend(FriendListWindow* _parent) : parent(_parent) {};
        ~Friend() {
            characters.clear();
        };
        Friend(const Friend&) = delete;
    private:
        std::wstring alias;
        std::string alias_str;
    public:
        std::string uuid;
        UUID uuid_bytes = { 0 };

        FriendListWindow* parent = 0;
        Character* current_char = nullptr;
        GuiUtils::EncString* current_map_name = nullptr;
        uint32_t current_map_id = 0;
        std::unordered_map<std::wstring, Character> characters;
        GW::FriendStatus status = GW::FriendStatus::Offline;
        GW::FriendType type = GW::FriendType::Unknow;
        clock_t last_update = 0;
        std::string cached_charnames_hover_str;
        bool cached_charnames_hover = false;

        Character* GetCharacter(const wchar_t*);
        Character* SetCharacter(const wchar_t*, uint8_t profession = 0);
        GW::Friend* GetFriend();
        std::string GetCharactersHover(bool include_charname = false);
        void StartWhisper();
        bool RemoveGWFriend();
        bool ValidUuid();

        [[nodiscard]] bool IsOffline() const
        {
            return status == GW::FriendStatus::Offline;
        };
        [[nodiscard]] bool NeedToUpdate(clock_t now) const
        {
            return (now - last_update) > 10000; // 10 Second stale.
        }
        [[nodiscard]] std::string& getAliasA()
        {
            if (alias_str.empty() && !alias.empty()) {
                alias_str = GuiUtils::WStringToString(alias);
            }
            return alias_str;
        }

        [[nodiscard]] const std::wstring& getAliasW() const {
            return alias;
        }
        void setAlias(const std::wstring_view _alias) {
            if (alias == _alias)
                return;
            alias = _alias;
            alias_str.clear();
        }
    };
protected:
    Friend* SetFriend(const uint8_t*, GW::FriendType, GW::FriendStatus, uint32_t, const wchar_t*, const wchar_t*);
    Friend* SetFriend(const GW::Friend*);



    void LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out);

    std::unordered_map<uint32_t,bool> ignored_parties{};
    bool ignore_trade = false;
public:

    Friend* GetFriend(const wchar_t*);
    Friend* GetFriend(const GW::Friend*);
    Friend* GetFriendByUUID(const std::string&);
    Friend* GetFriend(const uint8_t*);
    bool RemoveFriend(Friend* f);

    // Callback functions
    static void OnPlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak);
    // Handle friend updated (map change, char change, online status change, added or removed from fl)
    static void OnFriendUpdated(GW::HookStatus*, const GW::Friend* old_state, const GW::Friend* new_state);
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage , void*, void*);
    static void OnPrintChat(GW::HookStatus*, GW::Chat::Channel channel, wchar_t** message_ptr, FILETIME, int);
    // Ignore party invitations from players on my ignore list

    static bool GetIsPlayerIgnored(GW::Packet::StoC::PacketBase* pak);
    static bool GetIsPlayerIgnored(uint32_t player_number);
    static bool GetIsPlayerIgnored(const std::wstring& player_name);

    static void OnPostPartyInvite(GW::HookStatus* status, GW::Packet::StoC::PartyInviteReceived_Create* pak);
    static void OnPostTradePacket(GW::HookStatus* status, GW::Packet::StoC::TradeStart* pak);
    static void AddFriendAliasToMessage(wchar_t** message_ptr);

    const char* Name() const override { return "Friend List"; }
    const char* Icon() const override { return ICON_FA_USER_FRIENDS; }

    const bool IsWidget() const override;
    const bool IsWindow() const override;

    static void CmdAddFriend(const wchar_t* message, int argc, LPWSTR* argv);
    static void CmdRemoveFriend(const wchar_t* message, int argc, LPWSTR* argv);
    static void CmdWhisper(const wchar_t* message, int argc, LPWSTR* argv);

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void DrawHelp() override;
    void RegisterSettingsContent() override;
    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags=0) const override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Check friends list.
    void Poll();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    void DrawChatSettings();

    void LoadFromFile();
    void SaveToFile();

protected:
    ToolboxIni* inifile = nullptr;
    const wchar_t* ini_filename = L"friends.ini";
    bool loading = false; // Loading from disk?
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
    int loading_show_as = 1;

    bool resize_widget = false;

    bool lock_move_as_widget = false;
    bool lock_size_as_widget = true;

    Color hover_background_color = 0x33999999;
    bool friend_name_tag_enabled = false;
    Color friend_name_tag_color = 0xff6060ff;

    clock_t friends_list_checked = 0;

    uint8_t poll_interval_seconds = 10;

    // Mapping of Name > UUID
    std::unordered_map<std::wstring, Friend*> uuid_by_name{};

    // Main store of Friend info
    std::unordered_map<std::string, Friend*> friends{};

    bool show_location = true;

    GW::HookEntry FriendStatusUpdate_Entry;
    GW::HookEntry ErrorMessage_Entry;
    GW::HookEntry PlayerJoinInstance_Entry;
    GW::HookEntry SendChat_Entry;
    GW::HookEntry OnFriendCreated_Entry;
    GW::HookEntry OnUIMessage_Entry;
};
