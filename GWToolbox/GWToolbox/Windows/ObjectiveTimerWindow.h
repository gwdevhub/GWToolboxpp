#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>

#include <GWCA\GameContainers\GamePos.h>

#include "ToolboxWindow.h"

class ObjectiveTimerWindow : public ToolboxWindow {
    ObjectiveTimerWindow() {};
	~ObjectiveTimerWindow() {};
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

    // Room 1 Complete = Room 5 open = 12669 + 11692
// Room 2 Complete = Room 5 open = 54552 + 1760
// Room 3 Complete = Room 5 open = 45425 + 48290
// Room 4 Complete = Room 5 open = 40330 + 60114
// Room 5 Complete = Room 6 open = 29594
// Room 6 Complete = Room 7 open = 49742
// Room 7 Complete = Room 8 open = 55680

    enum DoorID {
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
        Deep_room_7 = 55680 // Room 7 Complete = Room 8 open
    };

private:
    class Objective {
    public:
		uint32_t id;
		char     name[126];
		DWORD    start;
		DWORD    done;
        DWORD    duration;
		char cached_done[16];
		char cached_start[16];
        char cached_duration[16];
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

		SYSTEMTIME system_time;
        DWORD time = -1;
        char cached_time[16] = { 0 };

        bool active = true;
        char name[256] = { 0 };
        std::vector<Objective> objectives;

        void CheckSetDone();
        bool Draw(); // returns false when should be deleted
		void StopObjectives();

        void Update();
        // todo: print to file

    private:
        // an internal id to ensure interface consistency
        const unsigned int ui_id = 0;	
        static unsigned int cur_ui_id;
    };

    std::vector<ObjectiveSet *> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);

    void AddDoAObjectiveSet(GW::Vec2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
    void AddDeepObjectiveSet();
    void AddUrgozObjectiveSet();
};
