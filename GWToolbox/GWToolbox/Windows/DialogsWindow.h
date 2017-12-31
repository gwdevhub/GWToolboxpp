#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>

#include "ToolboxWindow.h"

class DialogsWindow : public ToolboxWindow {
	DialogsWindow() {};
	~DialogsWindow() {};
public:
	static DialogsWindow& Instance() {
		static DialogsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Dialogs"; }

	void Initialize() override;

	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
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
