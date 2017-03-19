#include "BondsWindow.h"

#include <sstream>
#include <string>
#include <d3dx9tex.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\GWStructures.h>

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"
#include <OtherModules\Resources.h>

DWORD BondsWindow::buff_id[MAX_PARTYSIZE][MAX_BONDS] = { 0 };

void BondsWindow::Initialize() {
	ToolboxWidget::Initialize();
	for (int i = 0; i < MAX_BONDS; ++i) textures[i] = nullptr;
	Resources::Instance().LoadTextureAsync(&textures[BalthazarSpirit], "Balthazar's_Spirit.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[EssenceBond], "Essence_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[HolyVeil], "Holy_Veil.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeAttunement], "Life_Attunement.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeBarrier], "Life_Barrier.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeBond], "Life_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LiveVicariously], "Live_Vicariously.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Mending], "Mending.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[ProtectiveBond], "Protective_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[PurifyingVeil], "Purifying_Veil.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Retribution], "Retribution.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[StrengthOfHonor], "Strength_of_Honor.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Succor], "Succor.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[VitalBlessing], "Vital_Blessing.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[WatchfulSpirit], "Watchful_Spirit.jpg", "img");
}

void BondsWindow::Terminate() {
	for (int i = 0; i < MAX_BONDS; ++i) {
		if (textures[i]) {
			textures[i]->Release();
			textures[i] = nullptr;
		}
	}
}

void BondsWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	switch (GW::Map().GetInstanceType()) {
	case GW::Constants::InstanceType::Explorable:
		if (update) {
			update = false;
			if (!UpdateSkillbarBonds()) update = true;
			if (!UpdatePartyIndexMap()) update = true;
		}
		break;
	case GW::Constants::InstanceType::Outpost:
		update = true;
		UpdateSkillbarBonds();
		break;
	case GW::Constants::InstanceType::Loading:
		update = true;
		return; // exit here instead
	default:
		break;
	}
	
	GW::PartyInfo* info = GW::Partymgr().GetPartyInfo();
	if (info == nullptr) return;
	GW::gw_array<GW::AgentID>& allies = info->others;
	
	int img_size = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
	unsigned int party = agentids.size();
	unsigned int full_party = party + (show_allies ? allies.size() : 0);

	for (PartyIndex p = 0; p < full_party && p < MAX_PARTYSIZE; ++p) {
		for (BondIndex b = 0; b < n_bonds; ++b) {
			buff_id[p][b] = 0;
		}
	}

	GW::AgentEffectsArray effects = GW::Effects().GetPartyEffectArray();
	if (effects.valid()) {
		GW::BuffArray buffs = effects[0].Buffs;
		GW::AgentArray agents = GW::Agents().GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				PartyIndex p = -1;
				DWORD id = buffs[i].TargetAgentId;
				if (party_index.find(id) != party_index.end()) {
					p = party_index[id];
				} else if (show_allies) {
					// check allies
					for (unsigned int j = 0; j < allies.size(); ++j) {
						if (allies[j] == id) {
							p = party + j;
						}
					}
				}
				if (p == -1) continue;
				if (p >= MAX_PARTYSIZE) continue;

				BondIndex b = -1;
				for (BondIndex b_idx = 0; b_idx < n_bonds; ++b_idx) {
					if (buffs[i].SkillId == skillbar_bond_skillid[b_idx]) {
						b = b_idx;
						break;
					}
				}
				if (b == -1) continue;

				buff_id[p][b] = buffs[i].BuffId;
			}
		}
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background));
	ImGui::SetNextWindowSize(ImVec2((float)(n_bonds * img_size), 
		(float)((full_party + (full_party > party ? 1 : 0)) * img_size)));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(0, !click_to_use))) {
		float x = ImGui::GetWindowPos().x;
		float y = ImGui::GetWindowPos().y;
		for (PartyIndex p = 0; p < full_party && p < MAX_PARTYSIZE; ++p) {
			int extra = p >= party ? 1 : 0;
			float ytop = y + (p + 0 + extra) * img_size;
			float ybot = y + (p + 1 + extra) * img_size;
			for (BondIndex b = 0; b < n_bonds; ++b) {
				ImVec2 tl(x + (b + 0) * img_size, ytop);
				ImVec2 br(x + (b + 1) * img_size, ybot);
				if (buff_id[p][b] > 0) {
					ImGui::GetWindowDrawList()->AddImage(
						(ImTextureID)textures[skillbar_bond_idx[b]],
						ImVec2(tl.x, tl.y),
						ImVec2(br.x, br.y));
				}
				if (click_to_use) {
					if (ImGui::IsMouseHoveringRect(tl, br)) {
						ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
						if (ImGui::IsMouseReleased(0)) {
							if (buff_id[p][b] > 0) {
								GW::Effects().DropBuff(buff_id[p][b]);
							} else {
								GW::AgentID target;
								if (p < agentids.size()) {
									target = agentids[p];
								} else if (p - party >= 0 && p - party < allies.size()) {
									target = allies[p - party];
								}
								if (target > 0) {
									int slot = skillbar_bond_slot[b];
									GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
									if (skillbar.IsValid() && skillbar.Skills[slot].Recharge == 0) {
										GW::Gamethread().Enqueue([slot, target] {
											GW::Skillbarmgr().UseSkill(slot, target);
										});
									}
								}
							}
						}
					}
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleColor(); // window bg
	ImGui::PopStyleVar(2);
}

bool BondsWindow::UpdatePartyIndexMap() {
	party_index.clear();
	agentids.clear();

	GW::PartyInfo* info = GW::Partymgr().GetPartyInfo();
	if (info == nullptr) return false;

	GW::PlayerArray players = GW::Agents().GetPlayerArray();
	if (!players.valid()) return false;

	int index = 0;
	for (GW::PlayerPartyMember& player : info->players) {
		long id = players[player.loginnumber].AgentID;
		party_index[id] = index++;
		agentids.push_back(id);

		for (GW::HeroPartyMember& hero : info->heroes) {
			if (hero.ownerplayerid == player.loginnumber) {
				party_index[hero.agentid] = index++;
				agentids.push_back(hero.agentid);
			}
		}
	}
	for (GW::HenchmanPartyMember& hench : info->henchmen) {
		party_index[hench.agentid] = index++;
		agentids.push_back(hench.agentid);
	}

	return true;
}

void BondsWindow::UseBuff(PartyIndex player, BondIndex bond) {
	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

	DWORD target = agentids[player];
	if (target == 0) return;

	int slot = skillbar_bond_slot[bond];
	GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
	if (!skillbar.IsValid()) return;
	if (skillbar.Skills[slot].Recharge != 0) return;

	GW::Gamethread().Enqueue([slot, target] {
		GW::Skillbarmgr().UseSkill(slot, target);
	});
}

void BondsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	background = Colors::Load(ini, Name(), "background", Colors::ARGB(76, 0, 0, 0));
	click_to_use = ini->GetBoolValue(Name(), "click_to_use", true);
	show_allies = ini->GetBoolValue(Name(), "show_allies", true);
	row_height = ini->GetLongValue(Name(), "row_height", 0);
}

void BondsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	Colors::Save(ini, Name(), "background", background);
	ini->SetBoolValue(Name(), "click_to_use", click_to_use);
	ini->SetBoolValue(Name(), "show_allies", show_allies);
	ini->SetLongValue(Name(), "row_height", row_height);
}

void BondsWindow::DrawSettingInternal() {
	Colors::DrawSetting("Background", &background);
	ImGui::Checkbox("Click to Drop/Use", &click_to_use);
	ImGui::Checkbox("Show bonds for Allies", &show_allies);
	ImGui::InputInt("Row Height", &row_height);
	if (row_height < 0) row_height = 0;
	ImGui::ShowHelp("Height of each row, leave 0 for default");
	if (ImGui::SmallButton("Update bond order")) update = true;
	ImGui::ShowHelp("You can use this button after reordering your build in an\n"
					"explorable area to force a refresh in the bond order.");
}

bool BondsWindow::UpdateSkillbarBonds() {
	n_bonds = 0;
	skillbar_bond_idx.clear();
	skillbar_bond_slot.clear();
	skillbar_bond_skillid.clear();

	GW::Skillbar bar = GW::Skillbar::GetPlayerSkillbar();
	if (!bar.IsValid()) return false;
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < MAX_BONDS; ++j) {
			const Bond bond = static_cast<Bond>(j);
			if (bar.Skills[i].SkillId == (DWORD)GetSkillID(bond)) {
				++n_bonds;
				skillbar_bond_idx.push_back(bond);
				skillbar_bond_slot.push_back(i);
				skillbar_bond_skillid.push_back(bar.Skills[i].SkillId);
			}
		}
	}
	return true;
}

GW::Constants::SkillID BondsWindow::GetSkillID(Bond bond) const {
	using namespace GW::Constants;
	switch (bond) {
	case BondsWindow::BalthazarSpirit: return SkillID::Balthazars_Spirit;
	case BondsWindow::EssenceBond: return SkillID::Essence_Bond;
	case BondsWindow::HolyVeil: return SkillID::Holy_Veil;
	case BondsWindow::LifeAttunement: return SkillID::Life_Attunement;
	case BondsWindow::LifeBarrier: return SkillID::Life_Barrier;
	case BondsWindow::LifeBond: return SkillID::Life_Bond;
	case BondsWindow::LiveVicariously: return SkillID::Live_Vicariously;
	case BondsWindow::Mending: return SkillID::Mending;
	case BondsWindow::ProtectiveBond: return SkillID::Protective_Bond;
	case BondsWindow::PurifyingVeil: return SkillID::Purifying_Veil;
	case BondsWindow::Retribution: return SkillID::Retribution;
	case BondsWindow::StrengthOfHonor: return SkillID::Strength_of_Honor;
	case BondsWindow::Succor: return SkillID::Succor;
	case BondsWindow::VitalBlessing: return SkillID::Vital_Blessing;
	case BondsWindow::WatchfulSpirit: return SkillID::Watchful_Spirit;
	default: return SkillID::No_Skill;
	}
}
