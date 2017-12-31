#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"

class HeroBuildsWindow : public ToolboxWindow {
private:
	struct HeroBuild {
		HeroBuild(const char* n = "", const char* c = "", const int i = -1) {
			_snprintf_s(name, 128, "%s", n);
			_snprintf_s(code, 128, "%s", c);
			heroid = i;
		}
		char name[128];
		char code[128];
		int heroid;
	};
	struct TeamHeroBuild {
		static unsigned int cur_ui_id;
		TeamHeroBuild(const char* n = "")
			: ui_id(++cur_ui_id) {
			_snprintf_s(name, 128, "%s", n);
		}
		bool edit_open = false;
		char name[128];
		std::vector<HeroBuild> builds;
		bool hardmode = false;
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
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

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
			_snprintf_s(code, 128, "%s", c);
			heroind = i;
		}
		char code[128];
		int heroind;
	};


	clock_t send_timer;
	std::queue<CodeOnHero> queue;
};
