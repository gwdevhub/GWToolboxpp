#pragma once

#include "ToolboxPanel.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"
#include "EditBuild.h"

class BuildPanel : public ToolboxPanel {
public:
	static const int N_BUILDS = 16;

private:
	class Build : public OSHGui::Panel {
	public:
		Build(int index, std::string name, 
			EditBuild* edit_build, BuildPanel* panel)
			: index_(index), name_(name), 
			edit_build_(edit_build), panel_(panel) { }
		void BuildUI();
		void SendTeamBuild();
	private:
		int index_;
		std::string name_;
		EditBuild* edit_build_;
		BuildPanel* panel_;
		std::string GetDescription() { return name_; }
	};

	const int MAX_SHOWN = 9;		// number of teambuilds shown in interface
	const int BUILD_HEIGHT = 25;

	int first_shown_;				// index of first one shown
	std::vector<Build*> builds;
	OSHGui::ScrollBar* scrollbar_;
	EditBuild* edit_build_;
	clock_t send_timer;
	std::queue<std::wstring> queue;

	inline void Enqueue(std::wstring msg) { queue.push(msg); }
	void CalculateBuildPositions();
	inline void set_first_shown(int first);

protected:
	bool OnMouseScroll(const OSHGui::MouseMessage &mouse) override;

public:
	BuildPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override;
	inline void SendTeamBuild(long index) { builds[index]->SendTeamBuild(); }
	inline void SetPanelPosition(bool left) { 
		edit_build_->SetPanelPosition(left); 
	}
};
