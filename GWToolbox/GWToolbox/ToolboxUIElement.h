#pragma once

#include <Defines.h>
#include "ToolboxModule.h"

class ToolboxUIElement : public ToolboxModule {
public:
	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw(IDirect3DDevice9* pDevice) {};

	virtual void Initialize() override;
	virtual void Terminate() override {
		if (button_texture) button_texture->Release();
	}

	virtual void LoadSettings(CSimpleIni* ini) override;

	virtual void SaveSettings(CSimpleIni* ini) override;

	// returns true if clicked
	virtual bool DrawTabButton(IDirect3DDevice9* device, 
		bool show_icon = true, bool show_text = true);

	virtual bool ToggleVisible() { return visible = !visible; }

	virtual bool IsWindow() const { return false; }
	virtual bool IsWidget() const { return false; }

	bool visible;
	bool lock_move;
	bool lock_size;
	bool show_menubutton = false;

protected:
	void ShowVisibleRadio();
	IDirect3DTexture9* button_texture = nullptr;
};
