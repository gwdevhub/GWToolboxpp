#pragma once

#include <Windows.h>
#include <string>

#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
public:
	const char* Name() override { return "Dialog Panel"; }

	DialogPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}
	
	// Update. Will always be called every frame.
	void Update() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	inline DWORD QuestAcceptDialog(DWORD quest) { return (quest << 8) | 0x800001; }
	inline DWORD QuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; }

	DWORD IndexToQuestID(int index);
	DWORD IndexToDialogID(int index);

#define NUM_DIALOG_FAV_QUESTS 3
	int favindex[NUM_DIALOG_FAV_QUESTS];
};
