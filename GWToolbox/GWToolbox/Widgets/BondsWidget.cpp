#include "BondsWidget.h"

#include <sstream>
#include <string>
#include <d3dx9tex.h>

#include <imgui_internal.h>

#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "GuiUtils.h"
#include "Modules\ToolboxSettings.h"
#include <Modules\Resources.h>

DWORD BondsWidget::buff_id[MAX_PARTYSIZE][MAX_BONDS] = { 0 };

void BondsWidget::Initialize() {
	ToolboxWidget::Initialize();
	for (int i = 0; i < MAX_BONDS; ++i) textures[i] = nullptr;
	auto LoadBondTexture = [](IDirect3DTexture9** tex, const char* name, WORD id) -> void {
		Resources::Instance().LoadTextureAsync(tex, Resources::GetPath("img/bonds", name), id);
	};
	LoadBondTexture(&textures[BalthazarSpirit], "Balthazar's_Spirit.jpg", IDB_Bond_BalthazarsSpirit);
	LoadBondTexture(&textures[EssenceBond], "Essence_Bond.jpg", IDB_Bond_EssenceBond);
	LoadBondTexture(&textures[HolyVeil], "Holy_Veil.jpg", IDB_Bond_HolyVeil);
	LoadBondTexture(&textures[LifeAttunement], "Life_Attunement.jpg", IDB_Bond_LifeAttunement);
	LoadBondTexture(&textures[LifeBarrier], "Life_Barrier.jpg", IDB_Bond_LifeBarrier);
	LoadBondTexture(&textures[LifeBond], "Life_Bond.jpg", IDB_Bond_LifeBond);
	LoadBondTexture(&textures[LiveVicariously], "Live_Vicariously.jpg", IDB_Bond_LiveVicariously);
	LoadBondTexture(&textures[Mending], "Mending.jpg", IDB_Bond_Mending);
	LoadBondTexture(&textures[ProtectiveBond], "Protective_Bond.jpg", IDB_Bond_ProtectiveBond);
	LoadBondTexture(&textures[PurifyingVeil], "Purifying_Veil.jpg", IDB_Bond_PurifyingVeil);
	LoadBondTexture(&textures[Retribution], "Retribution.jpg", IDB_Bond_Retribution);
	LoadBondTexture(&textures[StrengthOfHonor], "Strength_of_Honor.jpg", IDB_Bond_StrengthOfHonor);
	LoadBondTexture(&textures[Succor], "Succor.jpg", IDB_Bond_Succor);
	LoadBondTexture(&textures[VitalBlessing], "Vital_Blessing.jpg", IDB_Bond_VitalBlessing);
	LoadBondTexture(&textures[WatchfulSpirit], "Watchful_Spirit.jpg", IDB_Bond_WatchfulSpirit);
}

void BondsWidget::Terminate() {
	for (int i = 0; i < MAX_BONDS; ++i) {
		if (textures[i]) {
			textures[i]->Release();
			textures[i] = nullptr;
		}
	}
}

void BondsWidget::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	switch (GW::Map::GetInstanceType()) {
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
		UpdatePartyIndexMap();
		break;
	case GW::Constants::InstanceType::Loading:
		update = true;
		return; // exit here instead
	default:
		break;
	}
	
	GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
	if (info == nullptr) return;
	GW::Array<GW::AgentID>& allies = info->others;
	
	int img_size = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
	unsigned int party = agentids.size();
	unsigned int full_party = party;
	static std::vector<DWORD> allies_id;
	if (show_allies) {
		allies_id.clear();
		for (DWORD id : allies) {
			GW::Agent* agent = GW::Agents::GetAgentByID(id);
			if (agent 
				&& agent->GetCanBeViewedInPartyWindow()
				&& !agent->GetIsSpawned()) {
				++full_party;
				allies_id.push_back(id);
			}
		}
	}

	for (PartyIndex p = 0; p < full_party && p < MAX_PARTYSIZE; ++p) {
		for (BondIndex b = 0; b < n_bonds; ++b) {
			buff_id[p][b] = 0;
		}
	}

	GW::AgentEffectsArray effects = GW::Effects::GetPartyEffectArray();
	if (effects.valid()) {
		GW::BuffArray buffs = effects[0].Buffs;
		GW::AgentArray agents = GW::Agents::GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				PartyIndex p = -1;
				DWORD id = buffs[i].TargetAgentId;
				if (party_index.find(id) != party_index.end()) {
					p = party_index[id];
				} else if (show_allies) {
					// check allies
					for (unsigned int j = 0; j < allies_id.size(); ++j) {
						if (allies_id[j] == id) {
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
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);
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
				BondIndex i = (flip_bonds ? (n_bonds - 1 - b) : b);
				ImVec2 tl(x + (i + 0) * img_size, ytop);
				ImVec2 br(x + (i + 1) * img_size, ybot);
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
								GW::Effects::DropBuff(buff_id[p][b]);
							} else {
								GW::AgentID target;
								if (p < agentids.size()) {
									target = agentids[p];
								} else if (p - party >= 0 && p - party < allies_id.size()) {
									target = allies_id[p - party];
								}
								if (target > 0) {
									int slot = skillbar_bond_slot[b];
									GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
									if (skillbar.IsValid() && skillbar.Skills[slot].Recharge == 0) {
										GW::GameThread::Enqueue([slot, target] {
											GW::SkillbarMgr::UseSkill(slot, target);
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

bool BondsWidget::UpdatePartyIndexMap() {
	party_index.clear();
	agentids.clear();

	GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
	if (info == nullptr) return false;

	GW::PlayerArray players = GW::Agents::GetPlayerArray();
	if (!players.valid()) return false;

	bool success = true;
	int index = 0;
	for (GW::PlayerPartyMember& player : info->players) {
		long id = players[player.loginnumber].AgentID;
		party_index[id] = index++;
		agentids.push_back(id);
		if (id == 0) success = false;

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

	return success;
}

void BondsWidget::UseBuff(PartyIndex player, BondIndex bond) {
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

	DWORD target = agentids[player];
	if (target == 0) return;

	int slot = skillbar_bond_slot[bond];
	GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
	if (!skillbar.IsValid()) return;
	if (skillbar.Skills[slot].Recharge != 0) return;

	GW::GameThread::Enqueue([slot, target] {
		GW::SkillbarMgr::UseSkill(slot, target);
	});
}

void BondsWidget::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), true);

	background = Colors::Load(ini, Name(), VAR_NAME(background), Colors::ARGB(76, 0, 0, 0));
	click_to_use = ini->GetBoolValue(Name(), VAR_NAME(click_to_use), true);
	show_allies = ini->GetBoolValue(Name(), VAR_NAME(show_allies), true);
	flip_bonds = ini->GetBoolValue(Name(), VAR_NAME(flip_bonds), false);
	row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), 0);
}

void BondsWidget::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
	
	Colors::Save(ini, Name(), VAR_NAME(background), background);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_use), click_to_use);
	ini->SetBoolValue(Name(), VAR_NAME(show_allies), show_allies);
	ini->SetBoolValue(Name(), VAR_NAME(flip_bonds), flip_bonds);
	ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
}

void BondsWidget::DrawSettings() {
	if (ImGui::CollapsingHeader(Name(), ImGuiTreeNodeFlags_AllowItemOverlap)) {
		ImGui::PushID(Name());
		ShowVisibleRadio();
		ImVec2 pos(0, 0);
		if (ImGuiWindow* window = ImGui::FindWindowByName(Name())) pos = window->Pos;
		if (ImGui::DragFloat2("Position", (float*)&pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowPos(Name(), pos);
		}
		ImGui::ShowHelp("You need to show the widget for this control to work");
		ImGui::Checkbox("Lock Position", &lock_move);
		DrawSettingInternal();
		ImGui::PopID();
	} else {
		ShowVisibleRadio();
	}
}

void BondsWidget::DrawSettingInternal() {
	Colors::DrawSetting("Background", &background);
	ImGui::Checkbox("Click to Drop/Use", &click_to_use);
	ImGui::Checkbox("Show bonds for Allies", &show_allies);
	ImGui::ShowHelp("'Allies' meaning the ones that show in party window, such as summoning stones");
	ImGui::Checkbox("Flip bond order (left/right)", &flip_bonds);
	ImGui::ShowHelp("Bond order is based on your build. Check this to flip them left <-> right");
	ImGui::InputInt("Row Height", &row_height);
	if (row_height < 0) row_height = 0;
	ImGui::ShowHelp("Height of each row, leave 0 for default");
	if (ImGui::SmallButton("Update bond order")) update = true;
	ImGui::ShowHelp("You can use this button after reordering your build in an\n"
					"explorable area to force a refresh in the bond order.");
}

bool BondsWidget::UpdateSkillbarBonds() {
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

GW::Constants::SkillID BondsWidget::GetSkillID(Bond bond) const {
	using namespace GW::Constants;
	switch (bond) {
	case BondsWidget::BalthazarSpirit: return SkillID::Balthazars_Spirit;
	case BondsWidget::EssenceBond: return SkillID::Essence_Bond;
	case BondsWidget::HolyVeil: return SkillID::Holy_Veil;
	case BondsWidget::LifeAttunement: return SkillID::Life_Attunement;
	case BondsWidget::LifeBarrier: return SkillID::Life_Barrier;
	case BondsWidget::LifeBond: return SkillID::Life_Bond;
	case BondsWidget::LiveVicariously: return SkillID::Live_Vicariously;
	case BondsWidget::Mending: return SkillID::Mending;
	case BondsWidget::ProtectiveBond: return SkillID::Protective_Bond;
	case BondsWidget::PurifyingVeil: return SkillID::Purifying_Veil;
	case BondsWidget::Retribution: return SkillID::Retribution;
	case BondsWidget::StrengthOfHonor: return SkillID::Strength_of_Honor;
	case BondsWidget::Succor: return SkillID::Succor;
	case BondsWidget::VitalBlessing: return SkillID::Vital_Blessing;
	case BondsWidget::WatchfulSpirit: return SkillID::Watchful_Spirit;
	default: return SkillID::No_Skill;
	}
}
