#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"

class HeroBuildsWindow : public ToolboxWindow {
private:
	struct HeroBuild {
		char name[128] = "";
		char code[128] = "";
		int  hero_index = 0;
	};

	struct TeamHeroBuild {
		size_t id;
		char name[128] = "";
		bool hardmode = false;
		std::vector<HeroBuild> builds;
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
	void Update(DWORD delta) override;

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

	TeamHeroBuild *build_in_edit;
	bool edit_open; // This may seem redundant, but that's how we know if the user close the window.

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
