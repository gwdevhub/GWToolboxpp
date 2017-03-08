#include "BondsWindow.h"

#include <sstream>
#include <string>
#include <d3dx9tex.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\GWStructures.h>

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"
#include <OtherModules\Resources.h>

DWORD BondsWindow::buff_id[MAX_PLAYERS][MAX_BONDS] = { 0 };

void BondsWindow::Initialize() {
	ToolboxWidget::Initialize();
	for (int i = 0; i < MAX_BONDS; ++i) textures[i] = nullptr;
	Resources::Instance().LoadTextureAsync(&textures[0], "balthspirit.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[1], "lifebond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[2], "protbond.jpg", "img");

	int img_size = GuiUtils::GetPartyHealthbarHeight();
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
	
	int img_size = GuiUtils::GetPartyHealthbarHeight();
	int party_size = GW::Partymgr().GetPartySize();

	int size = GW::Partymgr().GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;
	
	memset(buff_id, 0, sizeof(DWORD) * MAX_PLAYERS * MAX_BONDS);

	GW::AgentEffectsArray effects = GW::Effects().GetPartyEffectArray();
	if (effects.valid()) {
		GW::BuffArray buffs = effects[0].Buffs;
		GW::AgentArray agents = GW::Agents().GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				int player = -1;
				DWORD target_id = buffs[i].TargetAgentId;
				if (target_id < agents.size() && agents[target_id]) {
					player = agents[target_id]->PlayerNumber;
					if (player == 0 || player > MAX_PLAYERS) continue;
					--player;	// player numbers are from 1 to partysize in party list
				}

				int bond = -1;
				switch (static_cast<GW::Constants::SkillID>(buffs[i].SkillId)) {
				case GW::Constants::SkillID::Balthazars_Spirit: bond = 0; break;
				case GW::Constants::SkillID::Life_Bond: bond = 1; break;
				case GW::Constants::SkillID::Protective_Bond: bond = 2; break;
				default: break;
				}
				if (bond == -1) continue;

				buff_id[player][bond] = buffs[i].BuffId;
			}
		}
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background));
	ImGui::SetNextWindowSize(ImVec2((float)(MAX_BONDS * img_size), (float)(party_size * img_size)));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar, true))) {
		float x = ImGui::GetWindowPos().x;
		float y = ImGui::GetWindowPos().y;
		for (int player = 0; player < party_size; ++player) {
			for (int bond = 0; bond < MAX_BONDS; ++bond) {
				ImVec2 tl(x + (bond + 0) * img_size, y + (player + 0) * img_size);
				ImVec2 br(x + (bond + 1) * img_size, y + (player + 1) * img_size);
				if (buff_id[player][bond] > 0) {
					ImGui::GetWindowDrawList()->AddImage((ImTextureID)textures[bond],
						ImVec2(tl.x + 1, tl.y + 1),
						ImVec2(br.x - 2, br.y - 2));
				}
				if (ImGui::IsMouseHoveringRect(tl, br)) {
					ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
					if (ImGui::IsMouseReleased(0)) {
						if (buff_id[player][bond] > 0) {
							GW::Effects().DropBuff(buff_id[player][bond]);
						} else {
							UseBuff(player, bond);
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

void BondsWindow::UseBuff(int player, int bond) {
	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

	const GW::Constants::SkillID buff = [](int bond) -> GW::Constants::SkillID {
		switch (bond) {
		case 0: return GW::Constants::SkillID::Balthazars_Spirit;
		case 1: return GW::Constants::SkillID::Life_Bond;
		default: return GW::Constants::SkillID::Protective_Bond;
		}
	}(bond);

	DWORD target = GW::Agents().GetAgentIdByLoginNumber(player + 1);
	if (target == 0) return;

	int slot = GW::Skillbarmgr().GetSkillSlot(buff);
	if (slot < 0) return;
	if (GW::Skillbar::GetPlayerSkillbar().Skills[slot].Recharge != 0) return;

	GW::Skillbarmgr().UseSkill(slot, target);
}

void BondsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	background = Colors::Load(ini, Name(), "background", Colors::ARGB(76, 0, 0, 0));
}

void BondsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	Colors::Save(ini, Name(), "background", background);
}

void BondsWindow::DrawSettingInternal() {
	Colors::DrawSetting("Background", &background);
}
