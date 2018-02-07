#pragma once

#include <vector>
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
	};

	static const int MAX_PARTYSIZE = 64;

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
	void Update(float delta) override {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* device) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettings() override;
	void DrawSettingInternal() override;

private:
	typedef unsigned int PartyIndex;
	typedef unsigned int BondIndex;
	typedef DWORD BuffID;

	void UseBuff(PartyIndex player, BondIndex bond);
	GW::Constants::SkillID GetSkillID(Bond bond) const;
	bool UpdateSkillbarBonds();
	bool UpdatePartyIndexMap();

	IDirect3DTexture9* textures[MAX_BONDS];
	Color background;

	// settings
	bool click_to_use = true;
	bool show_allies = true;
	bool flip_bonds = false;
	int row_height = 0;

	static BuffID buff_id[MAX_PARTYSIZE][MAX_BONDS];

	bool update = true;

	unsigned int n_bonds;
	std::vector<BondIndex> skillbar_bond_idx;
	std::vector<BondIndex> skillbar_bond_slot;
	std::vector<BuffID> skillbar_bond_skillid;
	
	// map from agent id to index
	std::map<GW::AgentID, PartyIndex> party_index;
	std::vector<GW::AgentID> agentids;
};
