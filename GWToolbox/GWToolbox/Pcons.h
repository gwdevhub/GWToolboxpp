#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <imgui.h>
#include <GWCA\Constants\Skills.h>

#include "Timer.h"

class Pcon {
public:
	static const float size;

public:
	Pcon(IDirect3DDevice9* device, const char* filename, ImVec2 uv0, ImVec2 uv1, 
		const char* name, DWORD item, GW::Constants::SkillID effect, int threshold);

	void Draw(IDirect3DDevice9* device);

	virtual bool checkAndUse();		// checks if need to use pcon, uses if needed. Returns true if was used.
	virtual void scanInventory();	// scans inventory, updates quantity field
	void toggleActive();			// disables if enabled, enables if disabled

public:
	bool enabled = false;

protected:
	void CheckUpdateTimer();

	const char* const chatName;
	const DWORD itemID;
	const GW::Constants::SkillID effectID;
	const int threshold; // quantity at which the number color goes from green to yellow
	const ImVec2 uv0;
	const ImVec2 uv1;

	IDirect3DTexture9* texture = nullptr;

	int quantity = 0;

	clock_t timer;
	clock_t update_timer;
};

class PconCons : public Pcon {
public:
	PconCons(IDirect3DDevice9* device, const char* filename, ImVec2 uv0, ImVec2 uv1, 
		const char* name, DWORD item, GW::Constants::SkillID effect, int threshold)
		: Pcon(device, filename, uv0, uv1, name, item, effect, threshold) {}

	bool checkAndUse() override;
};

class PconCity : public Pcon {
public:
	PconCity(IDirect3DDevice9* device, const char* filename, ImVec2 uv0, ImVec2 uv1, const char* name, int threshold)
		: Pcon(device, filename, uv0, uv1, name, 0, (GW::Constants::SkillID)0, threshold) {}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(IDirect3DDevice9* device, const char* filename, ImVec2 uv0, ImVec2 uv1, const char* name, int threshold)
		: Pcon(device, filename, uv0, uv1, name, 0, (GW::Constants::SkillID)0, threshold) {}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconLunar : public Pcon {
public:
	PconLunar(IDirect3DDevice9* device, const char* filename, ImVec2 uv0, ImVec2 uv1, const char* name,
		GW::Constants::SkillID effect, int threshold)
		: Pcon(device, filename, uv0, uv1, name, 0, effect, threshold) {}

	bool checkAndUse() override;
	void scanInventory() override;
};
