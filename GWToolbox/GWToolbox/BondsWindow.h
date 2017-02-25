#pragma once

#include "Color.h"
#include "ToolboxModule.h"

class BondsWindow : public ToolboxModule {
	static const int MAX_PLAYERS = 12;
	static const int MAX_BONDS = 3;

public:
	const char* Name() const override { return "Bonds Window"; }

	BondsWindow(IDirect3DDevice9* device);
	~BondsWindow();

	// Update. Will always be called every frame.
	void Update() override {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* device) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettings() override;

private:
	void UseBuff(int player, int bond);

	IDirect3DTexture9* textures[MAX_BONDS];
	Color background;
};
