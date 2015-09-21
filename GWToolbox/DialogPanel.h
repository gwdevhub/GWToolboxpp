#pragma once

#include <Windows.h>
#include <string>
#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
private:
	const int n_quests = 29;

	inline DWORD QuestAcceptDialog(DWORD quest) { return (quest << 8) | 0x800001; }
	DWORD QuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; }

	void CreateButton(int grid_x, int grid_y, int hor_amount, 
		std::string text, DWORD dialog);

	std::string IndexToQuestName(int index);
	DWORD IndexToQuestID(int index);

public:
	static const int WIDTH = 300;
	static const int HEIGHT = 300;
	static const int SPACE = DefaultBorderPadding;
	static const int BUTTON_HEIGHT = 25;

	DialogPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

