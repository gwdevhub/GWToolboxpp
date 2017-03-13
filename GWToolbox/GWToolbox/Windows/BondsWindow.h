#pragma once

#include "Color.h"
#include "ToolboxWidget.h"

class BondsWindow : public ToolboxWidget {
	static const int MAX_PLAYERS = 12;
	static const int MAX_BONDS = 3;

	BondsWindow() {};
	~BondsWindow() {};
public:
	static BondsWindow& Instance() {
		static BondsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Bonds"; }

	void Initialize() override;
	void Terminate() override;

	// Update. Will always be called every frame.
	void Update() override {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* device) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	void UseBuff(int player, int bond);

	IDirect3DTexture9* textures[MAX_BONDS];
	Color background;

	bool click_to_use = true;
	int row_height = 0;

	static DWORD buff_id[MAX_PLAYERS][MAX_BONDS];
};
