#pragma once

#include <Defines.h>

#include <GWCA\Utilities\Hook.h>
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

		SYSTEMTIME system_time;
        DWORD time = -1;
        char cached_time[16];

        bool active = true;
        char name[256];
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

    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry ObjectiveAdd_Entry;
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry AgentUpdateAllegiance_Entry;
    GW::HookEntry DoACompleteZone_Entry;
};
