#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>

#include "ToolboxWindow.h"

class InstanceTimerWindow : public ToolboxWindow {
	InstanceTimerWindow() {};
	~InstanceTimerWindow() {};
public:
	static InstanceTimerWindow& Instance() {
		static InstanceTimerWindow instance;
		return instance;
	}

	const char* Name() const override { return "Instance Timer"; }

	void Initialize() override;

	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	bool doareset = false;
	bool uwreset = false;
	bool fowreset = false;
	bool show_uwtimer = true;
	bool show_doatimer = true;
	bool show_fowtimer = true;
	char buf[256];

	struct Objective {
		uint32_t    id;
		const char *name;
		DWORD       start;
		DWORD       done;
		char cached_done[10];
		char cached_start[10];
	};

	Objective obj_uw[12];
	Objective obj_fow[11];
	Objective obj_doa[4];

	Objective *get_objective(uint32_t obj_id);
	void DrawObjectives(IDirect3DDevice9* pDevice, const char *title, Objective *objectives, size_t count);
};