#pragma once

#include <Defines.h>

#include <GWCA\Utilities\Hook.h>
#include <GWCA\GameContainers\GamePos.h>
#include <GWCA\Packets\StoC.h>

#include <json.hpp>

#include "ToolboxWindow.h"

class ObjectiveTimerWindow : public ToolboxWindow {
    ObjectiveTimerWindow() {};
	~ObjectiveTimerWindow() {
        ClearObjectiveSets();
    };
public:
	static ObjectiveTimerWindow& Instance() {
		static ObjectiveTimerWindow instance;
		return instance;
	}

	const char* Name() const override { return "Objective Timer"; }

	void Initialize() override;

    void Update(float delta) override;
	void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
    void LoadRuns();
    void SaveRuns();

    const enum RoomID {
        // object_id's for doors opening.
        Deep_room_1_first = 12669, // Room 1 Complete = Room 5 open
        Deep_room_1_second = 11692,// Room 1 Complete = Room 5 open
        Deep_room_2_first = 54552, // Room 2 Complete = Room 5 open
        Deep_room_2_second = 1760, // Room 2 Complete = Room 5 open
        Deep_room_3_first = 45425, // Room 3 Complete = Room 5 open
        Deep_room_3_second = 48290, // Room 3 Complete = Room 5 open
        Deep_room_4_first = 40330, // Room 4 Complete = Room 5 open
        Deep_room_4_second = 60114, // Room 4 Complete = Room 5 open
        Deep_room_5 = 29594, // Room 5 Complete = Room 1,2,3,4,6 open
        Deep_room_6 = 49742, // Room 6 Complete = Room 7 open
        Deep_room_7 = 55680, // Room 7 Complete = Room 8 open
		// NOTE: Room 8 (failure) to room 10 (scorpion), no door.
		Deep_room_9 = 99887, // Trigger on leviathan?
		Deep_room_10 = 99888, // Generic room id for room 10 (dialog used to start)
		Deep_room_11 = 29320, // Room 11 door is always open. Use to START room 11 when it comes into range.
		Deep_room_12 = 99990, // Generic room id for room 12 (dialog used to start)
		Deep_room_13 = 99991, // Generic room id for room 13 (dialog used to start)
		Deep_room_14 = 99992, // Generic room id for room 13 (dialog used to start)
		Deep_room_15 = 99993 // Generic room id for room 15 (dialog used to start)
    };

private:
    class Objective {
    public:
		uint32_t id = 0;
		char     name[126] = "";
		DWORD    start = 0;
		DWORD    done = 0;
        DWORD    duration = 0;
		char cached_done[16] = "";
		char cached_start[16] = "";
        char cached_duration[16] = "";
        enum Status {
            NotStarted,
            Started,
            Completed,
            Failed
        } status = NotStarted;
       
	    Objective(uint32_t id, const char* name = "");

        bool IsStarted() const;
        bool IsDone() const;
        void SetStarted();
        void SetDone();
        void Draw();
        void Update();
	};

    class ObjectiveSet {
    public:
        ObjectiveSet();

        DWORD system_time;
        DWORD instance_time = (DWORD)-1;
        char cached_time[16] = { 0 };
        char cached_start[16] = { 0 };

        bool active = true;
        bool failed = false;
        bool from_disk = false;
        bool need_to_collapse = false;
        char name[256] = { 0 };
        std::vector<Objective> objectives;

        void CheckSetDone();
        bool Draw(); // returns false when should be deleted
		void StopObjectives();
        static ObjectiveSet* FromJson(nlohmann::json* json);
        nlohmann::json ToJson();
        void Update();
        void GetStartTime(struct tm* timeinfo);
        // todo: print to file
		// an internal id to ensure interface consistency
		const unsigned int ui_id = 0;
    private:

        static unsigned int cur_ui_id;
    };

    std::map<DWORD, ObjectiveSet *> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);
	ObjectiveSet* GetCurrentObjectiveSet();
	bool monitor_doors = false;
	bool show_current_run_window = false;
	bool clear_cached_times = false;
	bool auto_send_age = false;
    ObjectiveSet* current_objective_set = nullptr;

    void AddDoAObjectiveSet(GW::Vec2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
    void AddObjectiveSet(ObjectiveSet* os);
	void DoorOpened(uint32_t door_id);
	void DoorClosed(uint32_t door_id);
	void DisplayDialogue(GW::Packet::StoC::DisplayDialogue* packet);
    void AddDeepObjectiveSet();
    void AddUrgozObjectiveSet();
    void ClearObjectiveSets();
    

    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry ObjectiveAdd_Entry;
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry AgentUpdateAllegiance_Entry;
    GW::HookEntry DoACompleteZone_Entry;
	GW::HookEntry DisplayDialogue_Entry;
	GW::HookEntry MessageServer_Entry;
	GW::HookEntry InstanceLoadInfo_Entry;
	GW::HookEntry ManipulateMapObject_Entry;
};
