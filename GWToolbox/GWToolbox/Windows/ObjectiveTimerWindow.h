#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>

#include <GWCA\GameEntities\Position.h>

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

	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
    class Objective {
    public:
		uint32_t    id;
		char        name[126];
		DWORD       start;
		DWORD       done;
        DWORD       duration;
		char cached_done[16];
		char cached_start[16];
        char cached_duration[16];
		bool cancelled;

        Objective(uint32_t id, const char* name = "");
        void SetStarted(DWORD start_time = -1);
        void SetDone();
        void Draw();

        static void PrintTime(char* buf, size_t size, DWORD time);
	};

    class ObjectiveSet {
    public:
        ObjectiveSet();

		SYSTEMTIME system_time;
        char name[256];
        std::vector<Objective> objectives;
		unsigned int ui_id;

        void Draw();
		void StopObjectives();
		

		static void PrintTime(char* buf, size_t size, SYSTEMTIME time);
        // todo: print to file
		static unsigned int next_ui_id;
    };

    std::vector<ObjectiveSet *> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);

    void AddDoAObjectiveSet(GW::Vector2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
};
