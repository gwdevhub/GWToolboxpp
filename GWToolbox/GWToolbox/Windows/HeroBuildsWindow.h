#pragma once

#include "ToolboxWindow.h"

#include <array>
#include <vector>
#include <queue>
#include <string>

#include "Timer.h"

class HeroBuildsWindow : public ToolboxWindow {
private:
	// hero_index is:
	// -2 for player (although it doesn't really matter),
	// -1 for 'choose hero', 
	// 0 for 'no hero', 
	// and 1+ for heroes, order is in HeroIndexToID array
	struct HeroBuild {
		HeroBuild(const char* n = "", const char* c = "", int index = -1) {
			strncpy(name, n, 128);
			strncpy(code, c, 128);
		}
		char name[128];
		char code[128];
		int  hero_index;
	};

	struct TeamHeroBuild {
		static unsigned int cur_ui_id;
		TeamHeroBuild(const char* n = "")
			: ui_id(++cur_ui_id) {
			strncpy(name, n, 128);
		}
		bool edit_open = false;
		bool hardmode = false;
		char name[128];
		std::vector<HeroBuild> builds;
		unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
	};

	HeroBuildsWindow() {};
	~HeroBuildsWindow() {};
public:
	static HeroBuildsWindow& Instance() {
		static HeroBuildsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Hero Builds"; }

	void Initialize() override;
	void Terminate() override;

	// Update. Will always be called every frame.
	void Update(float delta) override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	void LoadFromFile();
	void SaveToFile();

	void Load(unsigned int idx);
	const char* BuildName(unsigned int idx) const;
	inline unsigned int BuildCount() const { return teambuilds.size(); }
private:
	// Load a teambuild
	void Load(const TeamHeroBuild& tbuild);
	// Load a specific build from a teambuild
	void Load(const TeamHeroBuild& tbuild, unsigned int idx);

	bool builds_changed = false;
	std::vector<TeamHeroBuild> teambuilds;

	struct CodeOnHero {
		CodeOnHero(const char* c = "", int i = 0) {
			snprintf(code, 128, "%s", c);
			heroind = i;
		}
		char code[128];
		int heroind;
	};


	clock_t send_timer;
	std::queue<CodeOnHero> queue;

	CSimpleIni* inifile = nullptr;
};
