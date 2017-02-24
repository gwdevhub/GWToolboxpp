#pragma once

#include "ToolboxPanel.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"

class BuildPanel : public ToolboxPanel {
private:
	struct Build {
		Build(const char* n = "", const char* c = "") {
			sprintf_s(name, "%s", n);
			sprintf_s(code, "%s", c);
		}
		char name[64];
		char code[64];
	};
	struct TeamBuild {
		static unsigned int cur_ui_id;
		TeamBuild(const char* n = "")
			: ui_id(++cur_ui_id) {
			sprintf_s(name, "%s", n);
		}
		bool edit_open = false;
		bool show_numbers = false;
		char name[64];
		std::vector<Build> builds;
		unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
	};

public:
	const char* Name() const override { return "Build Panel"; }
	const char* TabButtonText() const override { return "Builds"; }

	BuildPanel(IDirect3DDevice9* pDevice);
	~BuildPanel() {};

	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettings() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) const override;

	void Send(unsigned int idx);
	const char* BuildName(unsigned int idx) const;
	inline unsigned int BuildCount() const { return teambuilds.size(); }
private:
	// Send a teambuild
	void Send(const TeamBuild& tbuild);
	// Send a specific build from a teambuild
	void Send(const TeamBuild& tbuild, unsigned int idx);

	std::vector<TeamBuild> teambuilds;

	clock_t send_timer;
	std::queue<std::string> queue;
};
