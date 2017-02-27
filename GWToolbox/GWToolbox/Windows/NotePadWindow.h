#pragma once

#include "ToolboxWindow.h"

class NotePadWindow : public ToolboxWindow {
public:
	const char* Name() const override { return "Notepad"; }

	NotePadWindow();
	~NotePadWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

private:
	char text[2024 * 16]; // 2024 characters max
};
