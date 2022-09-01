#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Color.h>
#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>

class FriendListWindow : public ToolboxWindow {
    FriendListWindow() {
        inifile = new CSimpleIni(false, false, false);
    }
    ~FriendListWindow() {
        if (settings_thread.joinable())
            settings_thread.join();
        delete inifile;
    }

    std::thread settings_thread;
    // Structs because we don't case about public or private; this whole struct is private to this module anyway.
    struct Character {
        std::wstring name;
        uint8_t profession = 0;
    };
    struct UIChatMessage {
        uint32_t channel;
        wchar_t* message;
        uint32_t channel2;
    };
    struct Friend {
        Friend(FriendListWindow* _parent) : parent(_parent) {};
        ~Friend() {
            characters.clear();
        };
        std::string uuid;
        UUID uuid_bytes;
        std::wstring alias;
        FriendListWindow* parent;
        Character* current_char = nullptr;
        GuiUtils::EncString current_map_name;
        uint32_t current_map_id = 0;
        std::unordered_map<std::wstring, Character> characters;
        GW::FriendStatus status = GW::FriendStatus::Offline;
        GW::FriendType type = GW::FriendType::Unknow;
        clock_t last_update = 0;
        std::string cached_charnames_hover_str;
        bool cached_charnames_hover = false;
        Character* GetCharacter(const wchar_t*);
        Character* SetCharacter(const wchar_t*, uint8_t);
        GW::Friend* GetFriend();
        const std::string GetCharactersHover(bool include_charname = false);
        void StartWhisper();
        bool RemoveGWFriend();
        bool ValidUuid();
        const bool IsOffline() {
            return status == GW::FriendStatus::Offline;
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

    bool RemoveFriend(Friend* f);
    void LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out);

    std::unordered_map<uint32_t,bool> ignored_parties{};
    bool ignore_trade = false;
public:
    static FriendListWindow& Instance() {
        static FriendListWindow instance;
        return instance;
    }
    // Encoded message types as received via encoded chat message
    enum class MessageType : wchar_t {
        CANNOT_ADD_YOURSELF_AS_A_FRIEND = 0x2f3,
        EXCEEDED_MAX_NUMBER_OF_FRIENDS,
        CHARACTER_NAME_X_DOES_NOT_EXIST,
        FRIEND_ALREADY_ADDED_AS_X,
        INCOMING_WHISPER = 0x76d,
        OUTGOING_WHISPER,
        PLAYER_NAME_IS_INVALID = 0x880,
        PLAYER_X_NOT_ONLINE
    };
    bool WriteError(MessageType message_type, const wchar_t* character_name);

    // Callback functions
    static void OnPlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak);
    static void OnOutgoingWhisper(GW::HookStatus *status, int channel, wchar_t *message);
    static void OnOutgoingWhisperSuccess(GW::HookStatus *status, wchar_t *message);
    static void OnFriendAlreadyAdded(GW::HookStatus *status, wchar_t *message);
    static void OnPlayerNotOnline(GW::HookStatus *status, wchar_t *message);
    static void OnFriendUpdated(GW::HookStatus*, GW::Friend* f, GW::FriendStatus status, const wchar_t* alias, const wchar_t* charname);
    static void OnAddFriendError(GW::HookStatus* status, wchar_t* message);
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage , void*, void*);
    static void OnPrintChat(GW::HookStatus*, GW::Chat::Channel channel, wchar_t** message_ptr, FILETIME, int);
    static void OnChatMessage(GW::HookStatus* status, GW::Packet::StoC::PacketBase* packet);
    // Ignore party invitations from players on my ignore list

    static bool GetIsPlayerIgnored(GW::Packet::StoC::PacketBase* pak);
    static bool GetIsPlayerIgnored(uint32_t player_id);
    static bool GetIsPlayerIgnored(const std::wstring& player_name);

    static void OnPostPartyInvite(GW::HookStatus* status, GW::Packet::StoC::PartyInviteReceived_Create* pak);
    static void OnPostTradePacket(GW::HookStatus* status, GW::Packet::StoC::TradeStart* pak);
    static void AddFriendAliasToMessage(wchar_t** message_ptr);

    const char* Name() const override { return "Friend List"; }
    const char8_t* Icon() const override { return ICON_FA_USER_FRIENDS; }

    bool IsWidget() const override;
    bool IsWindow() const override;

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

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void DrawChatSettings();

    void LoadFromFile();
    void SaveToFile();

private:
    CSimpleIni* inifile = nullptr;
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

    // When a whisper is being redirected by this module, this flag is set. Stops infinite redirect loops.
    bool is_redirecting_whisper = false;
    struct PendingWhisper {
        std::wstring charname;
        std::wstring message;
        clock_t pending_add = 0;
        inline void reset(std::wstring _charname = L"", std::wstring _message = L"") {
            charname = _charname;
            message = _message;
            pending_add = 0;
        }
    };

    PendingWhisper pending_whisper;

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
    std::unordered_map<std::wstring, std::string> uuid_by_name{};

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
