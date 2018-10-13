#pragma once

#include <Defines.h>

#include <GWCA\GameEntities\Agent.h>
#include <GWCA\Constants\Skills.h>

#include "Color.h"
#include "ToolboxWidget.h"

class BondsWidget : public ToolboxWidget {
	static const int MAX_BONDS = 15;
	enum Bond {
		BalthazarSpirit,
		EssenceBond,
		HolyVeil,
		LifeAttunement,
		LifeBarrier,
		LifeBond,
		LiveVicariously,
		Mending,
		ProtectiveBond,
		PurifyingVeil,
		Retribution,
		StrengthOfHonor,
		Succor,
		VitalBlessing,
		WatchfulSpirit,
        None
	};

	BondsWidget() {};
	~BondsWidget() {};
public:
	static BondsWidget& Instance() {
		static BondsWidget instance;
		return instance;
	}

	const char* Name() const override { return "Bonds"; }

	void Initialize() override;
	void Terminate() override;

	// Update. Will always be called every frame.
    void Update(float delta) override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* device) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettings() override;
	void DrawSettingInternal() override;

private:
	void UseBuff(GW::AgentID target, DWORD buff_skillid);
    Bond GetBondBySkillID(DWORD skillid) const;

	IDirect3DTexture9* textures[MAX_BONDS];
	Color background;

	// settings
	bool click_to_cast = true;
    bool click_to_drop = true;
	bool show_allies = true;
	bool flip_bonds = false;
	int row_height = 0;
};
