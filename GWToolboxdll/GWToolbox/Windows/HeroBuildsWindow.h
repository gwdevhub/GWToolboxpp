#pragma once


#include <GWCA\Constants\Constants.h>

#include <GWCA\GameEntities\Party.h>

#include "ToolboxWindow.h"

#include <array>
#include <vector>
#include <queue>
#include <string>

#include "Timer.h"
#include "GuiUtils.h"

class HeroBuildsWindow : public ToolboxWindow {
private:
	// hero_index is:
	// -2 for player (although it doesn't really matter),
	// -1 for 'choose hero', 
	// 0 for 'no hero', 
	// and 1+ for heroes, order is in HeroIndexToID array
	struct HeroBuild {
		HeroBuild(const char* n, const char* c, int index = -1) : hero_index(index) {
			GuiUtils::StrCopy(name, n, sizeof(name));
			GuiUtils::StrCopy(code, c, sizeof(code));
		}
		char name[128];
		char code[128];
		int  hero_index;
	};

	struct TeamHeroBuild {
		static unsigned int cur_ui_id;
		TeamHeroBuild(const char* n)
			: ui_id(++cur_ui_id) {
			GuiUtils::StrCopy(name, n, sizeof(name));
		}
		bool edit_open = false;
        int mode = 0; // 0=don't change, 1=normal mode, 2=hard mode
		char name[128];
		std::vector<HeroBuild> builds;
		unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
	};

	HeroBuildsWindow() {};
	~HeroBuildsWindow();
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
	void DrawSettingInternal() override;

	void LoadFromFile();
	void SaveToFile();

	void Load(unsigned int idx);
	const char* BuildName(unsigned int idx) const;
	inline unsigned int BuildCount() const { return teambuilds.size(); }
private:

	bool hide_when_entering_explorable = false;
	bool one_teambuild_at_a_time = false;

	// Load a teambuild
	void Load(const TeamHeroBuild& tbuild);
	// Load a specific build from a teambuild
	void Load(const TeamHeroBuild& tbuild, unsigned int idx);
	void Send(const TeamHeroBuild& tbuild, size_t idx);
	void Send(const TeamHeroBuild& tbuild);
	void View(const TeamHeroBuild& tbuild, unsigned int idx);
	void HeroBuildName(const TeamHeroBuild& tbuild, unsigned int idx, std::string* out);

	// Returns ptr to party member of this hero, optionally fills out out_hero_index to be the index of this hero for the player.
	static GW::HeroPartyMember* GetPartyHeroByID(GW::Constants::HeroID hero_id, size_t* out_hero_index);
	bool builds_changed = false;
	std::vector<TeamHeroBuild> teambuilds;

	struct CodeOnHero {
		enum Stage : uint8_t {
			Add,
			Load,
			Finished
		} stage = Add;
		char code[128];
		size_t party_hero_index = 0xFFFFFFFF;
		GW::Constants::HeroID heroid = GW::Constants::HeroID::NoHero;
		clock_t started = 0;
		CodeOnHero(const char* c = "", GW::Constants::HeroID i = GW::Constants::HeroID::NoHero) {
			snprintf(code, 128, "%s", c);
			heroid = i;
		}
		// True when processing is done
		bool Process();
	};


	clock_t send_timer = 0;
	clock_t kickall_timer = 0;
	std::vector<CodeOnHero> pending_hero_loads;
	std::queue<std::string> send_queue;

	CSimpleIni* inifile = nullptr;
};
