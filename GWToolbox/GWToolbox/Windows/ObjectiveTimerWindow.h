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
		const char *name;
		DWORD       start;
		DWORD       done;
        DWORD       duration;
		char cached_done[16];
		char cached_start[16];
        char cached_duration[16];

        Objective(uint32_t id, const char* name);
        bool IsStarted() const { return start != -1; }
        bool IsDone() const { return done != -1; }
        void SetStarted(DWORD start_time = -1);
        void SetDone();
        void Draw();

        static void PrintTime(char* buf, size_t size, DWORD time);
	};

    class ObjectiveSet {
    public:
        ObjectiveSet(const char* _name) {
            strncpy(name, _name, sizeof(name));
        }
        ~ObjectiveSet() { for (auto* o : objectives) delete o; }

        char name[256];
        std::vector<Objective*> objectives;

        void Draw();
        // todo: print to file
    };

    std::vector<ObjectiveSet*> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);

    void AddDoAObjectiveSet(GW::Vector2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
};
