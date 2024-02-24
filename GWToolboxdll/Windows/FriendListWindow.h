#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Friendslist.h>

#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>

class FriendListWindow final : public ToolboxWindow {
public:
    static FriendListWindow& Instance()
    {
        static FriendListWindow instance;
        return instance;
    }

    // Structs because we don't case about public or private; this whole struct is private to this module anyway.
    struct Character {
    private:
        std::wstring name;
        std::string name_str;

    public:
        uint8_t profession = 0;

        const std::string& GetNameA()
        {
            if (name_str.empty() && !name.empty()) {
                name_str = GuiUtils::WStringToString(name);
            }
            return name_str;
        }

        [[nodiscard]] const std::wstring& getNameW() const
        {
            return name;
        }

        void SetName(std::wstring _name)
        {
            if (name == _name) {
                return;
            }
            name = std::move(_name);
            name_str.clear();
        }
    };

    struct UIChatMessage {
        uint32_t channel;
        wchar_t* message;
        uint32_t channel2;
    };

    struct Friend {
        Friend(FriendListWindow* _parent)
            : parent(_parent) { }

        ~Friend()
        {
            characters.clear();
        }

        Friend(const Friend&) = delete;

    private:
        std::wstring alias;
        std::string alias_str;

    public:
        std::string uuid;
        UUID uuid_bytes = {0};

        FriendListWindow* parent = nullptr;
        Character* current_char = nullptr;
        GuiUtils::EncString* current_map_name = nullptr;
        uint32_t current_map_id = 0;
        std::unordered_map<std::wstring, Character> characters{};
        GW::FriendStatus status = GW::FriendStatus::Offline;
        GW::FriendType type = GW::FriendType::Unknow;
        clock_t last_update = 0;
        std::string cached_charnames_hover_str;
        bool cached_charnames_hover = false;

        Character* GetCharacter(const wchar_t*);
        Character* SetCharacter(const wchar_t*, uint8_t profession = 0);
        GW::Friend* GetFriend();
        std::string GetCharactersHover(bool include_charname = false);
        void StartWhisper() const;
        bool RemoveGWFriend();
        bool ValidUuid();

        [[nodiscard]] bool IsOffline() const
        {
            return status == GW::FriendStatus::Offline;
        }

        [[nodiscard]] bool NeedToUpdate(const clock_t now) const
        {
            return now - last_update > 10000; // 10 Second stale.
        }

        [[nodiscard]] std::string& GetAliasA()
        {
            if (alias_str.empty() && !alias.empty()) {
                alias_str = GuiUtils::WStringToString(alias);
            }
            return alias_str;
        }

        [[nodiscard]] const std::wstring& GetAliasW() const
        {
            return alias;
        }

        void setAlias(const std::wstring_view _alias)
        {
            if (alias == _alias) {
                return;
            }
            alias = _alias;
            alias_str.clear();
        }
    };

    static Friend* GetFriend(const wchar_t*);
    static Friend* GetFriend(const GW::Friend*);
    static Friend* GetFriendByUUID(const std::string&);
    static Friend* GetFriend(const uint8_t*);
    static bool RemoveFriend(const Friend* f);

    static bool GetIsPlayerIgnored(GW::Packet::StoC::PacketBase* pak);
    static bool GetIsPlayerIgnored(uint32_t player_number);
    static bool GetIsPlayerIgnored(const std::wstring& player_name);

    static void AddFriendAliasToMessage(wchar_t** message_ptr);

    [[nodiscard]] const char* Name() const override { return "Friend List"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USER_FRIENDS; }

    [[nodiscard]] bool IsWidget() const override;
    [[nodiscard]] bool IsWindow() const override;

    static void CmdAddFriend(const wchar_t* message,const int argc, const LPWSTR* argv);
    static void CmdRemoveFriend(const wchar_t* message,const int argc, const LPWSTR* argv);
    static void CmdWhisper(const wchar_t* message, const int argc, const LPWSTR* argv);

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void DrawHelp() override;
    void RegisterSettingsContent() override;
    [[nodiscard]] ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Check friends list.
    static void Poll();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void DrawChatSettings();

    void LoadFromFile();
    void SaveToFile();
};
