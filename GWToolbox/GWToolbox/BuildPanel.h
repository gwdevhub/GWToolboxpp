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
		Build(OSHGui::Control* parent, int index, std::wstring name, 
			EditBuild* edit_build, BuildPanel* panel)
			: Panel(parent), index_(index), name_(name),
			edit_build_(edit_build), panel_(panel) { }
		void BuildUI();
		void SendTeamBuild();
	private:
		int index_;
		std::wstring name_;
		EditBuild* edit_build_;
		BuildPanel* panel_;
		std::wstring GetDescription() { return name_; }
	};

	const int MAX_SHOWN = 9;		// number of teambuilds shown in interface
	const int BUILD_HEIGHT = 25;

	int first_shown_;				// index of first one shown
	std::vector<Build*> builds;
	EditBuild* edit_build_;
	clock_t send_timer;
	std::queue<std::wstring> queue;

	virtual bool Intersect(const OSHGui::Drawing::PointI &point) const override;

	inline void Enqueue(std::wstring msg) { queue.push(msg); }
	void CalculateBuildPositions();

public:
	BuildPanel(OSHGui::Control* parent);

	void BuildUI() override;

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override {}

	inline void SendTeamBuild(long index) { builds[index]->SendTeamBuild(); }
	inline void SetPanelPosition(bool left) { 
		edit_build_->SetPanelPosition(left); 
	}
};
