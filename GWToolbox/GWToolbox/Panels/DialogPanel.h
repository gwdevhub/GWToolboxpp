#pragma once

#include <Windows.h>
#include <vector>

#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
public:
	const char* Name() const override { return "Dialog Panel"; }
	const char* TabButtonText() const override { return "Dialogs"; }

	DialogPanel();
	~DialogPanel() {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	inline DWORD QuestAcceptDialog(DWORD quest) { return (quest << 8) | 0x800001; }
	inline DWORD QuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; }

	DWORD IndexToQuestID(int index);
	DWORD IndexToDialogID(int index);

	int fav_count;
	std::vector<int> fav_index;

	bool show_common = true;
	bool show_uwteles = true;
	bool show_favorites = true;
	bool show_custom = true;
};
