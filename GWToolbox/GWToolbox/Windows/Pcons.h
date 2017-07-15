#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <SimpleIni.h>
#include <imgui.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\GameEntities\Item.h>
#include "Timer.h"
#include <Color.h>

class Pcon {
public:
	static int pcons_delay;
	static int lunar_delay;
	static float size;
	static bool disable_when_not_found;
	static Color enabled_bg_color;
	static DWORD player_id;

	static DWORD alcohol_level;
	static bool suppress_drunk_effect;
	static bool suppress_drunk_text;
	static bool suppress_drunk_emotes;
	static bool suppress_lunar_skills;

protected:
	Pcon(const char* chatname,
		const char* ininame, 
		const char* filename, 
		WORD res_id, // you can use 0 and it will not load texture from resource, only from file.
		ImVec2 uv0, ImVec2 uv1, int threshold);

public:
	void Draw(IDirect3DDevice9* device);
	virtual void Update(int delay = -1);
	void ScanInventory();
	void AfterUsed(bool used, int qty);
	inline void Toggle() { enabled = !enabled; }

	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section);

public:
	bool visible = true;
	bool enabled = false;
	int threshold; // quantity at which the number color goes from green to yellow and warning starts being displayed
	int quantity = 0;

	clock_t timer;

	const char* const chat;
	const char* const ini;

protected:
	void CheckUpdateTimer();

	// loops over the inventory, counting the items according to QuantityForEach
	// if 'used' is not null, it will also use the first item found,
	// and, if so, used *used to true
	// returns the number of items found, or -1 in case of error
	int CheckInventory(bool* used = nullptr, int* used_qty = nullptr) const;

	virtual bool CanUseByInstanceType() const;
	virtual bool CanUseByEffect() const = 0;
	virtual int QuantityForEach(const GW::Item* item) const = 0;

private:
	IDirect3DTexture9* texture = nullptr;
	const ImVec2 uv0;
	const ImVec2 uv1;

	GW::Constants::MapID mapid = GW::Constants::MapID::None;
	GW::Constants::InstanceType maptype = GW::Constants::InstanceType::Loading;
};

// A generic Pcon has an item_id and effect_id
class PconGeneric : public Pcon {
public:
	PconGeneric(const char* chat,
		const char* ini,
		const char* file,
		WORD res_id,
		ImVec2 uv0, ImVec2 uv1,
		DWORD item, GW::Constants::SkillID effect, 
		int threshold)
		: Pcon(chat, ini, file, res_id, uv0, uv1, threshold),
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
	PconCons(const char* chat,
		const char* ini,
		const char* file, 
		WORD res_id,
		ImVec2 uv0, ImVec2 uv1,
		DWORD item, GW::Constants::SkillID effect, 
		int threshold)
		: PconGeneric(chat, ini, file, res_id, uv0, uv1, item, effect, threshold) {}

	bool CanUseByEffect() const override;
};

class PconCity : public Pcon {
public:
	PconCity(const char* chat,
		const char* ini,
		const char* file, 
		WORD res_id,
		ImVec2 uv0, ImVec2 uv1, 
		int threshold)
		: Pcon(chat, ini, file, res_id, uv0, uv1, threshold) {}

	bool CanUseByInstanceType() const;
	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(const char* chat,
		const char* ini,
		const char* file,
		WORD res_id,
		ImVec2 uv0, ImVec2 uv1,
		int threshold)
		: Pcon(chat, ini, file, res_id, uv0, uv1, threshold) {}

	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
	void ForceUse();
};

class PconLunar : public Pcon {
public:
	PconLunar(const char* chat,
		const char* ini,
		const char* file,
		WORD res_id,
		ImVec2 uv0, ImVec2 uv1, 
		int threshold)
		: Pcon(chat, ini, file, res_id, uv0, uv1, threshold) {}

	void Update(int delay = -1) override;
	bool CanUseByEffect() const override;
	int QuantityForEach(const GW::Item* item) const override;
};
