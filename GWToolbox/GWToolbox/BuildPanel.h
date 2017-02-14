#pragma once

#include "ToolboxPanel.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"
#include "EditBuild.h"

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
		TeamBuild(const char* n = "") {
			sprintf_s(name, "%s", n);
		}
		bool edit_open = false;
		bool show_numbers = false;
		char name[64];
		std::vector<Build> builds;
	};

public:
	const char* Name() override { return "Build Panel"; }

	BuildPanel();

	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettings() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	void Send(unsigned int idx);
private:
	// Send a teambuild
	void Send(const TeamBuild& tbuild);
	// Send a specific build from a teambuild
	void Send(const TeamBuild& tbuild, unsigned int idx);

	std::vector<TeamBuild> teambuilds;

	clock_t send_timer;
	std::queue<std::string> queue;
};
