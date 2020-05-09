#include "stdafx.h"
#include "BondsWidget.h"

#include <d3dx9tex.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include "GuiUtils.h"
#include "Modules/ToolboxSettings.h"
#include <Modules/Resources.h>

//DWORD BondsWidget::buff_id[MAX_PARTYSIZE][MAX_BONDS] = { 0 };

void BondsWidget::Initialize() {
	ToolboxWidget::Initialize();
	for (int i = 0; i < MAX_BONDS; ++i) textures[i] = nullptr;
	auto LoadBondTexture = [](IDirect3DTexture9** tex, const wchar_t* name, WORD id) -> void {
		Resources::Instance().LoadTextureAsync(tex, Resources::GetPath(L"img/bonds", name), id);
	};
	LoadBondTexture(&textures[BalthazarSpirit], L"Balthazar's_Spirit.jpg", IDB_Bond_BalthazarsSpirit);
	LoadBondTexture(&textures[EssenceBond], L"Essence_Bond.jpg", IDB_Bond_EssenceBond);
	LoadBondTexture(&textures[HolyVeil], L"Holy_Veil.jpg", IDB_Bond_HolyVeil);
	LoadBondTexture(&textures[LifeAttunement], L"Life_Attunement.jpg", IDB_Bond_LifeAttunement);
	LoadBondTexture(&textures[LifeBarrier], L"Life_Barrier.jpg", IDB_Bond_LifeBarrier);
	LoadBondTexture(&textures[LifeBond], L"Life_Bond.jpg", IDB_Bond_LifeBond);
	LoadBondTexture(&textures[LiveVicariously], L"Live_Vicariously.jpg", IDB_Bond_LiveVicariously);
	LoadBondTexture(&textures[Mending], L"Mending.jpg", IDB_Bond_Mending);
	LoadBondTexture(&textures[ProtectiveBond], L"Protective_Bond.jpg", IDB_Bond_ProtectiveBond);
	LoadBondTexture(&textures[PurifyingVeil], L"Purifying_Veil.jpg", IDB_Bond_PurifyingVeil);
	LoadBondTexture(&textures[Retribution], L"Retribution.jpg", IDB_Bond_Retribution);
	LoadBondTexture(&textures[StrengthOfHonor], L"Strength_of_Honor.jpg", IDB_Bond_StrengthOfHonor);
	LoadBondTexture(&textures[Succor], L"Succor.jpg", IDB_Bond_Succor);
	LoadBondTexture(&textures[VitalBlessing], L"Vital_Blessing.jpg", IDB_Bond_VitalBlessing);
	LoadBondTexture(&textures[WatchfulSpirit], L"Watchful_Spirit.jpg", IDB_Bond_WatchfulSpirit);
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
	if (!visible) 
        return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
	const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    const GW::PlayerArray& players = GW::Agents::GetPlayerArray();
    if (info == nullptr) return;
    if (!players.valid()) return;
    if (!info->players.valid()) return;
    // note: info->heroes, ->henchmen, and ->others CAN be invalid during normal use.

    // ==== Get party ====
    std::vector<GW::AgentID> party_list; // index to agent id
    std::unordered_map<GW::AgentID, int> party_map; // agent id to index
    int allies_start = 255;
    for (const GW::PlayerPartyMember& player : info->players) {
        DWORD id = players[player.login_number].agent_id;
        party_map[id] = party_list.size();
        party_list.push_back(id);
        
        if (info->heroes.valid()) {
            for (const GW::HeroPartyMember& hero : info->heroes) {
                if (hero.owner_player_id == player.login_number) {
                    party_map[hero.agent_id] = party_list.size();
                    party_list.push_back(hero.agent_id);
                }
            }
        }
    }
    if (info->henchmen.valid()) {
        for (const GW::HenchmanPartyMember& hench : info->henchmen) {
            party_list.push_back(hench.agent_id);
        }
    }
    if (show_allies && info->others.valid()) {
        allies_start = party_list.size();
        for (const DWORD ally_id : info->others) {
            GW::Agent* agent = GW::Agents::GetAgentByID(ally_id);
            GW::AgentLiving* ally = agent ? agent->GetAsAgentLiving() : nullptr;
            if (ally && ally->GetCanBeViewedInPartyWindow() && !ally->GetIsSpawned()) {
                party_map[ally_id] = party_list.size();
                party_list.push_back(ally_id);
            }
        }
    }
	
    // ==== Get bonds ====
    std::vector<int> bond_list; // index to skill id
    std::unordered_map<DWORD, int> bond_map; // skill id to index
    const GW::Skillbar *bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid()) return;
    for (int slot = 0; slot < 8; ++slot) {
        DWORD SkillID = bar->skills[slot].skill_id;
        Bond bond = GetBondBySkillID(SkillID);
        if (bond != Bond::None) {
            bond_map[SkillID] = bond_list.size();
            bond_list.push_back(SkillID);
        }
    }

    const GW::AgentEffectsArray& effects = GW::Effects::GetPartyEffectArray();
    if (!effects.valid()) return;
    const GW::BuffArray& buffs = effects[0].buffs; // first one is for players, after are heroes

    // ==== Draw ====
    const int img_size = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
    const int height = (party_list.size() + (allies_start < 255 ? 1 : 0)) * img_size;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background).Value);
	ImGui::SetNextWindowSize(ImVec2((float)(bond_list.size() * img_size), (float)height));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(0, !(click_to_cast || click_to_drop)))) {
		float win_x = ImGui::GetWindowPos().x;
		float win_y = ImGui::GetWindowPos().y;

        auto GetGridPos = [&](const int _x, const int _y, bool topleft) -> ImVec2 {
            int x = _x;
            int y = _y;
            if (y >= allies_start) ++y;
            if (!topleft) {
                ++x; ++y;
            }
            return ImVec2(win_x + x * img_size, 
                win_y + y * img_size);
        };

        bool handled_click = false;
        for (unsigned int i = 0; i < buffs.size(); ++i) {
            DWORD agent = buffs[i].target_agent_id;
            DWORD skill = buffs[i].skill_id;
            if (party_map.find(agent) == party_map.end()) continue; // bond target not in party
            int y = party_map[agent];
            if (bond_map.find(skill) == bond_map.end()) continue; // bond with a skill not in skillbar 
            int x = bond_map[skill];
            Bond bond = GetBondBySkillID(skill);
            ImVec2 tl = GetGridPos(x, y, true);
            ImVec2 br = GetGridPos(x, y, false);
            ImGui::GetWindowDrawList()->AddImage((ImTextureID)textures[bond], tl, br);
            if (click_to_drop && ImGui::IsMouseHoveringRect(tl, br) && ImGui::IsMouseReleased(0)) {
                GW::Effects::DropBuff(buffs[i].buff_id);
                handled_click = true;
            }
        }

        if (click_to_cast && !handled_click) {
            for (unsigned int y = 0; y < party_list.size(); ++y) {
                for (unsigned int x = 0; x < bond_list.size(); ++x) {
                    ImVec2 tl = GetGridPos(x, y, true);
                    ImVec2 br = GetGridPos(x, y, false);
                    if (ImGui::IsMouseHoveringRect(tl, br)) {
                        ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
                        if (ImGui::IsMouseReleased(0)) {
                            UseBuff(party_list[y], bond_list[x]);
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

void BondsWidget::UseBuff(GW::AgentID targetId, DWORD buff_skillid) {
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
    if (targetId == 0) return;

    GW::Agent* target = GW::Agents::GetAgentByID(targetId);
    if (target == nullptr) return;

    int slot = GW::SkillbarMgr::GetSkillSlot((GW::Constants::SkillID)buff_skillid);
    if (slot < 0) return;
	GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
	if (!skillbar || !skillbar->IsValid()) return;
	if (skillbar->skills[slot].recharge != 0) return;

    // capture by value!
    GW::GameThread::Enqueue([slot, targetId]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}

void BondsWidget::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), true);

	background = Colors::Load(ini, Name(), VAR_NAME(background), Colors::ARGB(76, 0, 0, 0));
    click_to_cast = ini->GetBoolValue(Name(), VAR_NAME(click_to_cast), click_to_cast);
    click_to_drop = ini->GetBoolValue(Name(), VAR_NAME(click_to_drop), click_to_drop);
    show_allies = ini->GetBoolValue(Name(), VAR_NAME(show_allies), show_allies);
	flip_bonds = ini->GetBoolValue(Name(), VAR_NAME(flip_bonds), flip_bonds);
	row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), row_height);
    hide_in_outpost = ini->GetLongValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
}

void BondsWidget::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
	Colors::Save(ini, Name(), VAR_NAME(background), background);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_cast), click_to_cast);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_drop), click_to_drop);
    ini->SetBoolValue(Name(), VAR_NAME(show_allies), show_allies);
	ini->SetBoolValue(Name(), VAR_NAME(flip_bonds), flip_bonds);
	ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
    ini->SetLongValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
}

void BondsWidget::DrawSettingInternal() {
    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    Colors::DrawSettingHueWheel("Background", &background, 0);
	ImGui::Checkbox("Click to cast bond", &click_to_cast);
    ImGui::Checkbox("Click to cancel bond", &click_to_drop);
	ImGui::Checkbox("Show bonds for Allies", &show_allies);
	ImGui::ShowHelp("'Allies' meaning the ones that show in party window, such as summoning stones");
	ImGui::Checkbox("Flip bond order (left/right)", &flip_bonds);
	ImGui::ShowHelp("Bond order is based on your build. Check this to flip them left <-> right");
	ImGui::InputInt("Row Height", &row_height);
	if (row_height < 0) row_height = 0;
	ImGui::ShowHelp("Height of each row, leave 0 for default");
}

BondsWidget::Bond BondsWidget::GetBondBySkillID(DWORD skillid) const {
    using namespace GW::Constants;
    switch ((GW::Constants::SkillID)skillid) {
    case SkillID::Balthazars_Spirit: return Bond::BalthazarSpirit;
    case SkillID::Essence_Bond: return Bond::EssenceBond;
    case SkillID::Holy_Veil: return Bond::HolyVeil;
    case SkillID::Life_Attunement: return Bond::LifeAttunement;
    case SkillID::Life_Barrier: return Bond::LifeBarrier;
    case SkillID::Life_Bond: return Bond::LifeBond;
    case SkillID::Live_Vicariously: return Bond::LiveVicariously;
    case SkillID::Mending: return Bond::Mending;
    case SkillID::Protective_Bond: return Bond::ProtectiveBond;
    case SkillID::Purifying_Veil: return Bond::PurifyingVeil;
    case SkillID::Retribution: return Bond::Retribution;
    case SkillID::Strength_of_Honor: return Bond::StrengthOfHonor;
    case SkillID::Succor: return Bond::Succor;
    case SkillID::Vital_Blessing: return Bond::VitalBlessing;
    case SkillID::Watchful_Spirit: return Bond::WatchfulSpirit;
    default: return Bond::None;
    }
}
