#pragma once

#include "ToolboxWindow.h"
#include <thread>

#include <GWCA\GameEntities\Friendslist.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GuiUtils.h>

#include <GWCA\Utilities\Hook.h>
#include <mutex>
#include "Color.h"

/* Out of scope namespecey lookups */
namespace {
	static ImColor ProfColors[11] = {
		0xFFFFFFFF,
		0xFFEEAA33,
		0xFF55AA00,
		0xFF4444BB,
		0xFF00AA55,
		0xFF8800AA,
		0xFFBB3333,
		0xFFAA0088,
		0xFF00AAAA,
		0xFF996600,
		0xFF7777CC
	};
	static wchar_t* ProfNames[11] = {
		L"Unknown",
		L"Warrior",
		L"Ranger",
		L"Monk",
		L"Necromancer",
		L"Mesmer",
		L"Elementalist",
		L"Assassin",
		L"Ritualist",
		L"Paragon",
		L"Dervish"
	};
	static const ImColor StatusColors[4] = {
		IM_COL32(0x99,0x99,0x99,255), // offline
		IM_COL32(0x0,0xc8,0x0,255),  // online
		IM_COL32(0xc8,0x0,0x0,255), // busy
		IM_COL32(0xc8,0xc8,0x0,255)  // away
	};
	static std::wstring current_map;
	static GW::Constants::MapID current_map_id = GW::Constants::MapID::None;
	static std::wstring* GetCurrentMapName() {
		GW::Constants::MapID map_id = GW::Map::GetMapID();
		if (current_map_id != map_id) {
			GW::AreaInfo* i = GW::Map::GetMapInfo(map_id);
			if (!i) return nullptr;
			wchar_t name_enc[16];
			if (!GW::UI::UInt32ToEncStr(i->name_id, name_enc, 16))
				return nullptr;
			current_map.clear();
			GW::UI::AsyncDecodeStr(name_enc, &current_map);
			current_map_id = map_id;
		}
		return &current_map;
	}
	static char* GetStatusText(uint8_t status) {
		switch (static_cast<GW::FriendStatus>(status)) {
		case GW::FriendStatus::FriendStatus_Offline: return "Offline";
		case GW::FriendStatus::FriendStatus_Online: return "Online";
		case GW::FriendStatus::FriendStatus_DND: return "Do not disturb";
		case GW::FriendStatus::FriendStatus_Away: return "Away";
		}
		return "Unknown";
	}
	static std::map<uint32_t, wchar_t*> map_names;
	static GUID StringToGuid(const std::string& str)
	{
		GUID guid;
		sscanf(str.c_str(),
			"%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
			&guid.Data1, &guid.Data2, &guid.Data3,
			&guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
			&guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

		return guid;
	}

	static void GuidToString(GUID guid, char* guid_cstr)
	{
		snprintf(guid_cstr, 128,
			"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	}
}

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
		UUID uuid_bytes;
		std::wstring alias;
		FriendListWindow* parent;
		Character* current_char = nullptr;
		char current_map_name[128] = { 0 };
		uint32_t current_map_id = 0;
		std::unordered_map<std::wstring, Character> characters;
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
		std::string GetCharactersHover(bool include_charname = false, bool include_location = false) {
			if (!cached_charnames_hover) {
				std::wstring cached_charnames_hover_ws = L"Characters for ";
				cached_charnames_hover_ws += alias;
				cached_charnames_hover_ws += L":";
				for (std::unordered_map<std::wstring, Character>::iterator it2 = characters.begin(); it2 != characters.end(); ++it2) {
					cached_charnames_hover_ws += L"\n  ";
					cached_charnames_hover_ws += it2->first;
					if (it2->second.profession) {
						cached_charnames_hover_ws += L" (";
						cached_charnames_hover_ws += ProfNames[it2->second.profession];
						cached_charnames_hover_ws += L")";
					}
				}
				cached_charnames_hover_str = GuiUtils::WStringToString(cached_charnames_hover_ws);
				cached_charnames_hover = true;
			}
			std::string str;
			if (include_charname && current_char) {
				str += GuiUtils::WStringToString(current_char->name);
				str += "\n";
			}
			if (include_charname && current_map_name[0]) {
				str += current_map_name;
				str += "\n";
			}
			if (str.size())
				str += "\n";
			str += cached_charnames_hover_str;
			return str;
		};
		void StartWhisper();
		void InviteToParty();
		bool AddGWFriend();
		bool RemoveGWFriend();
		bool IsOffline() {
			return status < GW::FriendStatus::FriendStatus_Online || status > GW::FriendStatus::FriendStatus_Away;
		};
		bool NeedToUpdate(clock_t now) {
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
