#pragma once

#include "ToolboxPanel.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"

class HeroBuildsWindow : public ToolboxPanel {
private:
	struct HeroBuild {
		HeroBuild(const char* n = "", const char* c = "", const int i = 0) {
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

	const char* listbox_items[38] = { "Choose Hero", "Norgu", "Goren", "Tahlkora", "Master Of Whispers", "Acolyte Jin", "Koss", "Dunkoro", "Acolyte Sousuke", "Melonni", "Zhed Shadowhoof", "General Morgahn", "Magrid The Sly", "Zenmai", "Olias", "Razah", "MOX", "Jora", "Keiran Thackeray", "Pyre Fierceshot", "Anton", "Livia", "Hayda", "Kahmu", "Gwen", "Xandra", "Vekk", "Ogden", "Mercenary Hero 1", "Mercenary Hero 2", "Mercenary Hero 3", "Mercenary Hero 4", "Mercenary Hero 5", "Mercenary Hero 6", "Mercenary Hero 7", "Mercenary Hero 8", "Miku", "Zei Ri" };

	clock_t send_timer;
	std::queue<CodeOnHero> queue;
};
