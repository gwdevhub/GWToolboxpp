#pragma once

#include <Windows.h>
#include <string>

#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
private:
	const int n_quests = 29;
	const int n_dialogs = 15;

	inline DWORD QuestAcceptDialog(DWORD quest) { return (quest << 8) | 0x800001; }
	DWORD QuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; }

	void CreateButton(int grid_x, int grid_y, int hor_amount, 
		std::wstring text, DWORD dialog);

	std::wstring IndexToQuestName(int index);
	DWORD IndexToQuestID(int index);

	std::wstring IndexToDialogName(int index);
	DWORD IndexToDialogID(int index);

public:
	static const int SPACE = Padding;
	static const int BUTTON_HEIGHT = 25;

	DialogPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;
	
	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override {}
};

