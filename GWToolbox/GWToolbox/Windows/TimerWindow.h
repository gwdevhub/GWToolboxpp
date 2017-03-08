#pragma once

#include "ToolboxWidget.h"

class TimerWindow : public ToolboxWidget {
	TimerWindow() {};
	~TimerWindow() {};
public:
	static TimerWindow& Instance() {
		static TimerWindow instance;
		return instance;
	}
	const char* Name() const override { return "Timer"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
