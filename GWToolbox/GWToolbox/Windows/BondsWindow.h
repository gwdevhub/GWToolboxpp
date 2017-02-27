#pragma once

#include "Color.h"
#include "ToolboxWindow.h"

class BondsWindow : public ToolboxWindow {
	static const int MAX_PLAYERS = 12;
	static const int MAX_BONDS = 3;

public:
	const char* Name() const override { return "Bonds Window"; }

	BondsWindow();
	~BondsWindow();

	// Update. Will always be called every frame.
	void Update() override {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* device) override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	void UseBuff(int player, int bond);

	IDirect3DTexture9* textures[MAX_BONDS];
	Color background;
};
