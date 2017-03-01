#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <SimpleIni.h>
#include <imgui.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\GameEntities\Item.h>
#include "Timer.h"

class Pcon {
public:
	static int delay;
	static float size;
	static bool disable_when_not_found;

	static DWORD player_id;

protected:
	Pcon(const char* filename, 
		ImVec2 uv0, ImVec2 uv1, 
		const char* name, int threshold);

public:
	void Draw(IDirect3DDevice9* device);
	void Update();
	void ScanInventory();
	inline void Toggle() { enabled = !enabled; }

	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section);

public:
	bool enabled = false;

protected:
	void CheckUpdateTimer();

	// loops over the inventory, counting the items according to QuantityForEach
	// if 'used' is not null, it will also use the first item found,
	// and, if so, used *used to true
	// returns the number of items found, or -1 in case of error
	int CheckInventory(bool* used = nullptr) const;

	virtual bool CanUseByInstanceType() const;
	virtual bool CanUseByEffect() const = 0;
	virtual int QuantityForEach(const GW::Item* item) const = 0;

	int quantity = 0;

	clock_t timer;

private:
	IDirect3DTexture9* texture = nullptr;
	const ImVec2 uv0;
	const ImVec2 uv1;

	const char* const chatName;
	const int threshold; // quantity at which the number color goes from green to yellow

	GW::Constants::MapID mapid = GW::Constants::MapID::None;
	GW::Constants::InstanceType maptype = GW::Constants::InstanceType::Loading;

};

// A generic Pcon has an item_id and effect_id
class PconGeneric : public Pcon {
public:
	PconGeneric(const char* filename, ImVec2 uv0, ImVec2 uv1,
		const char* name, DWORD item, GW::Constants::SkillID effect, int threshold)
		: Pcon(filename, uv0, uv1, name, threshold),
		itemID(item), effectID(effect) {}

protected:
	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;

private:
	const DWORD itemID;
	const GW::Constants::SkillID effectID;
};

// Same as generic pcon, but with more restrictions on usage
class PconCons : public PconGeneric {
public:
	PconCons(const char* filename, ImVec2 uv0, ImVec2 uv1, 
		const char* name, DWORD item, GW::Constants::SkillID effect, int threshold)
		: PconGeneric(filename, uv0, uv1, name, item, effect, threshold) {}

	bool CanUseByEffect() const override;
};

class PconCity : public Pcon {
public:
	PconCity(const char* filename, ImVec2 uv0, ImVec2 uv1, const char* name, int threshold)
		: Pcon(filename, uv0, uv1, name, threshold) {}

	bool CanUseByInstanceType() const;
	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(const char* filename, 
		ImVec2 uv0, ImVec2 uv1, const char* name, int threshold)
		: Pcon(filename, uv0, uv1, name, threshold) {}

	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
};

class PconLunar : public Pcon {
public:
	PconLunar(const char* filename, 
		ImVec2 uv0, ImVec2 uv1, const char* name, int threshold)
		: Pcon(filename, uv0, uv1, name, threshold) {}

	void Update();
	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
};
