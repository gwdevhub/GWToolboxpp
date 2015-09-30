#pragma once

#include <Windows.h>
#include <string>
#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
private:
	const int n_quests = 29;
	const int n_dialogs = 11;

	inline DWORD QuestAcceptDialog(DWORD quest) { return (quest << 8) | 0x800001; }
	DWORD QuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; }

	void CreateButton(int grid_x, int grid_y, int hor_amount, 
		std::string text, DWORD dialog);

	std::string IndexToQuestName(int index);
	DWORD IndexToQuestID(int index);

	std::string IndexToDialogName(int index);
	DWORD IndexToDialogID(int index);

public:
	static const int SPACE = DefaultBorderPadding;
	static const int BUTTON_HEIGHT = 25;

	DialogPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

