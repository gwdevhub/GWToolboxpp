#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"
#include "GuiUtils.h"

class BuildsWindow : public ToolboxWindow {
private:
	struct Build {
		Build(const char* n, const char* c, bool _has_pcons = false) : has_pcons(_has_pcons) {
			GuiUtils::StrCopy(name, n, sizeof(name));
			GuiUtils::StrCopy(code, c, sizeof(code));

		}
		char name[128];
		char code[128];
        // Does this build have pcons setup?
        bool has_pcons = false;
        // Vector of pcons to use for this build, listed by ini name e.g. "cupcake"
        std::set<std::string> pcons;
	};
	struct TeamBuild {
		static unsigned int cur_ui_id;
		TeamBuild(const char* n)
			: ui_id(++cur_ui_id) {
			GuiUtils::StrCopy(name, n, sizeof(name));
		}
		bool edit_open = false;
        int edit_pcons = -1;
		bool show_numbers = false;
		char name[128];
		std::vector<Build> builds;
		unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
	};

	BuildsWindow() {};
	~BuildsWindow() {};
public:
	static BuildsWindow& Instance() {
		static BuildsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Builds"; }

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
	bool MoveOldBuilds(CSimpleIni* ini);

	void Send(unsigned int idx);
	const char* BuildName(unsigned int idx) const;
	inline unsigned int BuildCount() const { return teambuilds.size(); }
private:
	// Send a teambuild
	void Send(const TeamBuild& tbuild);
	// Send a specific build from a teambuild
	void Send(const TeamBuild& tbuild, unsigned int idx);
    // Load a specific build from a teambuild (and any applicable pcons)
    void Load(const TeamBuild& tbuild, unsigned int idx);
    // Toggle pcons for a specific build
    void LoadPcons(const TeamBuild& tbuild, unsigned int idx);
    // View a teambuild
    void View(const TeamBuild& tbuild);
    // View a specific build from a teambuild
    void View(const TeamBuild& tbuild, unsigned int idx);

    void DrawBuildSection(TeamBuild& tbuild, unsigned int idx);

	bool builds_changed = false;
	std::vector<TeamBuild> teambuilds;
    bool order_by_name = false;
    bool order_by_index = !order_by_name;
    bool auto_load_pcons = true;
	bool delete_builds_without_prompt = false;

	clock_t send_timer = 0;
	std::queue<std::string> queue;

	CSimpleIni* inifile = nullptr;
};
