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

/* Out of scope namespecey lookups */
namespace {
	static ImColor ProfColors[10] = {
		IM_COL32_WHITE,
		IM_COL32(0xb4, 0x82, 0x46, 255),
		IM_COL32(0x91, 0xc0, 0x4c, 255),
		IM_COL32(0x7a, 0xbb, 0xd1, 255),
		IM_COL32(0x4c, 0xA2, 0x64, 255),
		IM_COL32(0x71, 0x35, 0x66, 255),
		IM_COL32(0xc4, 0x4b, 0x4b, 255),
		IM_COL32(0xe1, 0x18, 0x7c, 255),
		IM_COL32(0x1e, 0xd6, 0xba, 255),
		IM_COL32(0xea, 0xde, 0x00, 255)/*,
		IM_COL32(0x57, 0x68, 0x95, 255)*/
	};
	static wchar_t* ProfNames[10] = {
		L"Unknown",
		L"Warrior",
		L"Ranger"
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
		IM_COL32(0x99,0x99,0x99,0), // offline
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
		switch (status) {
		case 0: return "Offline";
		case 1: return "Online";
		case 2: return "Do not disturb";
		case 3: return "Away";
		}
		return "Unknown";
	}
	static std::map<uint32_t, wchar_t*> map_names;
}

class FriendListWindow : public ToolboxWindow {
private:
    FriendListWindow() {};
    ~FriendListWindow() {};
	// Structs because we don't case about public or private; this whole struct is private to this module anyway.
	struct Character {
		std::wstring name;
		uint8_t profession = 0;
	};
	
    struct Friend {
        std::string uuid;
        std::wstring alias;
		Character* current_char = nullptr;
		char current_map_name[128] = { 0 };
		uint32_t current_map_id = 0;
        std::map<std::wstring,Character> characters;
        uint8_t status = 0; // 0 = Offline, 1 = Online, 2 = Do not disturb, 3 = Away
        uint8_t type = 255; // 0 = Friend, 1 = Ignore, 2 = Played, 3 = Trade
        bool is_tb_friend = false;  // Is this a friend via toolbox, or friend via friend list?
		bool has_tmp_uuid = false;
		bool added_via_toolbox = false; // This friend added via toolbox?
        clock_t last_update = 0;
		std::string cached_charnames_hover_str;
		bool cached_charnames_hover = false;
		Character* GetCharacter(const wchar_t*);
		Character* SetCharacter(const wchar_t*, uint8_t);
		GW::Friend* GetFriend();
		void GetMapName();
		std::string GetCharactersHover() {
			if (!cached_charnames_hover) {
				std::wstring cached_charnames_hover_ws = L"Characters for ";
				cached_charnames_hover_ws += alias;
				cached_charnames_hover_ws += L":";
				for (std::map<std::wstring, Character>::iterator it2 = characters.begin(); it2 != characters.end(); ++it2) {
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
			
			return cached_charnames_hover_str;
		};
		void StartWhisper();
		void InviteToParty();
		bool AddGWFriend();
		bool RemoveGWFriend();
		bool IsOffline() {
			return status < 1 || status > 3;
		};
        bool NeedToUpdate(clock_t now) {
            return (now - last_update) > 10000; // 10 Second stale.
        }
    };

	// Functions for sorting friend list by online status etc.
	typedef std::function<bool(std::pair<std::string, Friend>, std::pair<std::string, Friend>)> Comparator;
	Comparator compFunctor = [](std::pair<std::string, Friend> elem1, std::pair<std::string, Friend> elem2) {
		return elem1.second.status < elem1.second.status;
	};

	Friend* SetFriend(uint8_t*, uint8_t, uint8_t, uint32_t, const wchar_t*, const wchar_t*);
	Friend* SetFriend(GW::Friend*);
	Friend* GetFriend(const wchar_t*);
	Friend* GetFriend(GW::Friend*);
	Friend* GetFriend(uint8_t*);
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

    void Initialize() override;
	void SignalTerminate() override;
	bool CanTerminate() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Check friends list.
    void Poll();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

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


    clock_t friends_list_checked = 0;

	uint8_t poll_interval_seconds = 10;

    // tasks to be done async by the worker thread
	class WorkerThread {
		std::queue<std::function<void()>> thread_jobs;
		std::thread thread;
		std::mutex mutex;
		bool running = false;
		bool need_to_stop = false;
	public:
		~WorkerThread() {
			need_to_stop = true;
			if (thread.joinable())
				thread.join();
		}
		bool IsRunning() {
			return running;
		}
		void Add(std::function<void()> func) {
			if (need_to_stop)
				return;
			std::lock_guard<std::mutex> lock(mutex);
			thread_jobs.push(func);
			Run();
		}
		void Stop() {
			need_to_stop = true;
		}
		void Run() {
			if (need_to_stop || IsRunning())
				return;
			running = true;
			thread = std::thread([this]() {
				std::unique_lock lock(mutex);
				lock.unlock();
				while (!need_to_stop) {
					lock.lock();
					if (thread_jobs.empty()) {
						lock.unlock();
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}
					else {
						thread_jobs.front()();
						thread_jobs.pop();
						lock.unlock();
					}
				}
				running = false;
			});
		}
	};
	WorkerThread worker;
    

    // Mapping of Name > UUID
    std::map<std::wstring, std::string> uuid_by_name;

    // Main store of Friend info
    std::map<std::string, Friend> friends;

	bool show_location = true;

    GW::HookEntry FriendStatusUpdate_Entry;
    GW::HookEntry ErrorMessage_Entry;
	GW::HookEntry PlayerJoinInstance_Entry;
};
