#pragma once

#include <Defines.h>
#include "ToolboxModule.h"

class ToolboxUIElement : public ToolboxModule {
public:
	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw(IDirect3DDevice9* pDevice) {};

	virtual void Initialize() override;

	// save 'visible' field
	virtual void LoadSettings(CSimpleIni* ini) override;

	// load 'visible' field
	virtual void SaveSettings(CSimpleIni* ini) override;

	virtual bool ToggleVisible() { return visible = !visible; }

	bool visible;
	bool lock_move;
	bool lock_size;

protected:
	void ShowVisibleRadio();
};
