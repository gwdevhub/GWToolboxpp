#pragma once

#include "ToolboxWindow.h"

class NotePadWindow : public ToolboxWindow {
	NotePadWindow() { strcpy_s(text, ""); };
	~NotePadWindow() {};
public:
	static NotePadWindow& Instance() {
		static NotePadWindow instance;
		return instance;
	}

	const char* Name() const override { return "Notepad"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	char text[2024 * 16]; // 2024 characters max
	bool filedirty = false;
};
