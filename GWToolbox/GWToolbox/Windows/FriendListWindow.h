#pragma once

#include "ToolboxWindow.h"
#include <thread>

#include <GWCA\GameEntities\Friendslist.h>

#include <GWCA\Utilities\Hook.h>

class FriendListWindow : public ToolboxWindow {
private:
    FriendListWindow() {};
    ~FriendListWindow() {
        if (worker.joinable()) worker.join();
		worker.~thread();
    };
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
        std::vector<Character> characters;
        uint8_t status = 255; // 0 = Offline, 1 = Online, 2 = Do not disturb, 3 = Away
        uint8_t type = 255; // 0 = Friend, 1 = Ignore, 2 = Played, 3 = Trade
        bool is_tb_friend = false;  // Is this a friend via toolbox, or friend via friend list?
		bool has_tmp_uuid = false;
		bool added_via_toolbox = false; // This friend added via toolbox?
        clock_t last_update = 0;
		Character* GetCharacter(const wchar_t*);
		GW::Friend* GetFriend();
		void GetMapName();
		void StartWhisper();
		void InviteToParty();
		bool AddGWFriend();
		bool RemoveGWFriend();
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
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Check friends list.
    void Poll();
	// Reorder friends by filter/online status etc
	void Reorder();

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

    std::thread worker;
    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;

    // Mapping of Name > UUID
    std::map<std::wstring, std::string> uuid_by_name;

    // Main store of Friend info
    std::map<std::string, Friend> friends;

	bool show_location = true;

    GW::HookEntry FriendStatusUpdate_Entry;
    GW::HookEntry ErrorMessage_Entry;
	GW::HookEntry PlayerJoinInstance_Entry;
};
