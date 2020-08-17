#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <GWToolbox.h>
#include <GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Widgets/PartyDamage.h>

#define INI_FILENAME L"healthlog.ini"
#define IniSection "health"

void PartyDamage::Initialize() {
	ToolboxWidget::Initialize();

	total = 0;
	send_timer = TIMER_INIT();

	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry,
	[this] (GW::HookStatus *status, GW::Packet::StoC::GenericModifier *packet) -> void {
		return DamagePacketCallback(status, packet);
	});

	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry,
	[this] (GW::HookStatus *status, GW::Packet::StoC::MapLoaded *packet) -> void {
		return MapLoadedCallback(status, packet);
	});

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i].damage= 0;
		damage[i].recent_damage = 0;
		damage[i].last_damage = TIMER_INIT();
	}
}

void PartyDamage::Terminate() {
	ToolboxWidget::Terminate();
	if (inifile) {
	    inifile->Reset();
	    delete inifile;
	    inifile = nullptr;
	}
}

void PartyDamage::MapLoadedCallback(GW::HookStatus *, GW::Packet::StoC::MapLoaded *packet) {
    UNREFERENCED_PARAMETER(packet);
	switch (GW::Map::GetInstanceType()) {
	case GW::Constants::InstanceType::Outpost:
		in_explorable = false;
		break;
	case GW::Constants::InstanceType::Explorable:
		party_index.clear();
		if (!in_explorable) {
			in_explorable = true;
			ResetDamage();
		}
		break;
	case GW::Constants::InstanceType::Loading:
	default:
		break;
	}
}

void PartyDamage::DamagePacketCallback(GW::HookStatus *, GW::Packet::StoC::GenericModifier* packet) {

	// ignore non-damage packets
	switch (packet->type) {
	case GW::Packet::StoC::P156_Type::damage:
	case GW::Packet::StoC::P156_Type::critical:
	case GW::Packet::StoC::P156_Type::armorignoring:
		break;
	default:
		return;
	}

	// ignore heals
	if (packet->value >= 0) return;

	GW::AgentArray agents = GW::Agents::GetAgentArray();

	// get cause agent
	if (packet->cause_id >= agents.size()) return;
	if (!agents[packet->cause_id]) return;
	GW::AgentLiving* cause = agents[packet->cause_id]->GetAsAgentLiving();

	if (cause == nullptr) return;
	if (cause->allegiance != 0x1) return;
	auto cause_it = party_index.find(cause->agent_id);
	if (cause_it == party_index.end()) return;  // ignore damage done by non-party members

	// get target agent
	if (!agents[packet->target_id]) return;
	GW::AgentLiving* target = agents[packet->target_id]->GetAsAgentLiving();
	if (target == nullptr) return;
	if (target->login_number != 0) return; // ignore player-inflicted damage
										        // such as Life bond or sacrifice
	if (target->allegiance == 0x1) return; // ignore damage inflicted to allies in general
	// warning: note damage to allied spirits, minions or stones may still trigger
	// you can do damage like that by standing in bugged dart traps in eye of the north
	// or maybe with some skills that damage minions/spirits

	long ldmg;
	if (target->max_hp > 0 && target->max_hp < 100000) {
        ldmg = std::lround(-packet->value * target->max_hp);
		hp_map[target->player_number] = target->max_hp;
	} else {
		auto it = hp_map.find(target->player_number);
		if (it == hp_map.end()) {
			// max hp not found, approximate with hp/lvl formula
            ldmg = std::lround(-packet->value * (target->level * 20 + 100));
		} else {
			// size_t maxhp = it->second;
            ldmg = std::lround(-packet->value * it->second);
		}
	}

    uint32_t dmg = static_cast<uint32_t>(ldmg);

	size_t index = cause_it->second;
	if (index >= MAX_PLAYERS) return; // something went very wrong.
	if (damage[index].damage == 0) {
		damage[index].agent_id = packet->cause_id;
		GW::Agents::AsyncGetAgentName(cause, damage[index].name);
		/*
		if (cause->LoginNumber > 0) {
			damage[index].name = GW::Agents::GetPlayerNameByLoginNumber(cause->LoginNumber);
		} else {
			damage[index].name = L"<A Hero>";
		}
		*/
		damage[index].primary = static_cast<GW::Constants::Profession>(cause->primary);
		damage[index].secondary = static_cast<GW::Constants::Profession>(cause->secondary);
	}

	damage[index].damage += dmg;
	total += dmg;

	if (visible) {
		damage[index].recent_damage += dmg;
		damage[index].last_damage = TIMER_INIT();
	}
}

void PartyDamage::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
	if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
		send_timer = TIMER_INIT();
		if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents::GetPlayer()) {
			GW::Chat::SendChat('#', send_queue.front().c_str());
			send_queue.pop();
		}
	}

	if (party_index.empty()) {
		CreatePartyIndexMap();
	}

	// reset recent if needed
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (TIMER_DIFF(damage[i].last_damage) > recent_max_time) {
			damage[i].recent_damage = 0;
		}
	}
}

void PartyDamage::CreatePartyIndexMap() {
	if (!GW::PartyMgr::GetIsPartyLoaded()) return;
	
	GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
	if (info == nullptr) return;

	GW::PlayerArray players = GW::Agents::GetPlayerArray();
	if (!players.valid()) return;

	size_t index = 0;
	for (GW::PlayerPartyMember& player : info->players) {
		uint32_t id = players[player.login_number].agent_id;
		if (id == GW::Agents::GetPlayerId()) player_index = index;
		party_index[id] = index++;

		for (GW::HeroPartyMember& hero : info->heroes) {
			if (hero.owner_player_id == player.login_number) {
				party_index[hero.agent_id] = index++;
			}
		}
	}
	for (GW::HenchmanPartyMember& hench : info->henchmen) {
		party_index[hench.agent_id] = index++;
	}
}

void PartyDamage::Draw(IDirect3DDevice9* device) {	
    UNREFERENCED_PARAMETER(device);
	if (!visible) return;
	if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		return;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
	
	size_t line_height = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
	uint32_t size = GW::PartyMgr::GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;

	uint32_t max_recent = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max_recent < damage[i].recent_damage) {
			max_recent = damage[i].recent_damage;
		}
	}

	uint32_t max = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max < damage[i].damage) {
			max = damage[i].damage;
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(color_background).Value);
    ImGui::SetNextWindowSize(ImVec2(width, static_cast<float>(size * line_height)));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(0, true))) {
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f16));
		const float &x = ImGui::GetWindowPos().x;
        const float &y = ImGui::GetWindowPos().y;
        const float &_width = ImGui::GetWindowWidth();
        const Color damage_col_from = Colors::Add(color_damage, Colors::ARGB(0, 20, 20, 20));
        const Color damage_col_to = Colors::Sub(color_damage, Colors::ARGB(0, 20, 20, 20));
        const Color damage_recent_from = Colors::Add(color_recent, Colors::ARGB(0, 20, 20, 20));
        const Color damage_recent_to = Colors::Sub(color_recent, Colors::ARGB(0, 20, 20, 20));
        const float height_diff = (line_height - ImGui::GetTextLineHeight()) / 2;
		const int BUF_SIZE = 16;
		char buf[BUF_SIZE];
        float part_of_max = 0, part_of_recent = 0, 
			bar_left = 0, bar_right = 0, 
			recent_left = 0, recent_right = 0, 
			perc_of_total = 0;
		for (size_t i = 0; i < size; ++i) {
            const float &damage_float = static_cast<float>(damage[i].damage);

            part_of_max = max > 0 ? damage_float / max : 0;
			bar_left = bars_left ? (x + _width * (1.0f - part_of_max)) : (x);
            bar_right = bars_left ? (x + _width) : (x + _width * part_of_max);
			ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
				ImVec2(bar_left, y + i * line_height),
				ImVec2(bar_right, y + (i + 1) * line_height), 
				damage_col_from, damage_col_from, damage_col_to, damage_col_to);

            part_of_recent = max_recent > 0 ? static_cast<float>(damage[i].recent_damage) / max_recent : 0;
			recent_left = bars_left ? (x + _width * (1.0f - part_of_recent)) : (x);
			recent_right = bars_left ? (x + _width) : (x + _width * part_of_recent);
			ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
				ImVec2(recent_left, y + (i + 1) * line_height - 6), 
				ImVec2(recent_right, y + (i + 1) * line_height), 
				damage_recent_from, damage_recent_from, damage_recent_to, damage_recent_to);

			if (damage[i].damage < 1000) {
				snprintf(buf, BUF_SIZE, "%d", damage[i].damage);
			} else if (damage[i].damage < 1000 * 10) {
                snprintf(buf, BUF_SIZE, "%.2f k", damage_float / 1000);
			} else if (damage[i].damage < 1000 * 1000) {
                snprintf(buf, BUF_SIZE, "%.1f k", damage_float / 1000);
			} else {
                snprintf(buf, BUF_SIZE, "%.2f m", damage_float / (1000 * 1000));
			}
            
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(x + ImGui::GetStyle().ItemSpacing.x, y + (i * line_height) + height_diff),
				IM_COL32(255, 255, 255, 255), buf);

			perc_of_total = GetPercentageOfTotal(damage[i].damage);
			snprintf(buf, BUF_SIZE, "%.1f %%", perc_of_total);
            ImGui::GetWindowDrawList()->AddText(
				ImVec2(x + _width / 2, y + (i * line_height) + height_diff),
				IM_COL32(255, 255, 255, 255), buf);
		}
		ImGui::PopFont();
	}
	ImGui::End();
	ImGui::PopStyleColor(); // window bg
	ImGui::PopStyleVar(2);
}

float PartyDamage::GetPartOfTotal(uint32_t dmg) const {
	if (total == 0) return 0;
	return (float)dmg / total;
}

void PartyDamage::WritePartyDamage() {
	std::vector<size_t> idx(MAX_PLAYERS);
	for (size_t i = 0; i < MAX_PLAYERS; ++i) idx[i] = i;
	sort(idx.begin(), idx.end(), [this](size_t i1, size_t i2) {
		return damage[i1].damage > damage[i2].damage;
	});

	for (size_t i = 0; i < idx.size(); ++i) {
		WriteDamageOf(idx[i], i + 1);
	}
	send_queue.push(L"Total ~ 100 % ~ " + std::to_wstring(total));
}

void PartyDamage::WriteDamageOf(size_t index, uint32_t rank) {
	if (index >= MAX_PLAYERS) return;
	if (damage[index].damage <= 0) return;

	if (rank == 0) {
		rank = 1; // start at 1, add 1 for each player with higher damage
		for (size_t i = 0; i < MAX_PLAYERS; ++i) {
			if (i == index) continue;
			if (damage[i].agent_id == 0) continue;
			if (damage[i].damage > damage[index].damage) ++rank;
		}
	}

	const size_t size = 130;
	wchar_t buff[size];
	swprintf_s(buff, size, L"#%2d ~ %3.2f %% ~ %ls/%ls %ls ~ %d",
		rank,
		GetPercentageOfTotal(damage[index].damage),
		GW::Constants::GetWProfessionAcronym(damage[index].primary).c_str(),
		GW::Constants::GetWProfessionAcronym(damage[index].secondary).c_str(),
		damage[index].name.c_str(),
		damage[index].damage);

	send_queue.push(buff);
}


void PartyDamage::WriteOwnDamage() {
	WriteDamageOf(player_index);
}

void PartyDamage::ResetDamage() {
	total = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i].Reset();
	}
}

void PartyDamage::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), true);
	width = (float)ini->GetDoubleValue(Name(), VAR_NAME(width), 100.0f);
	bars_left = ini->GetBoolValue(Name(), VAR_NAME(bars_left), true);
	row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), 0);
	recent_max_time = ini->GetLongValue(Name(), VAR_NAME(recent_max_time), 7000);
	color_background = Colors::Load(ini, Name(), VAR_NAME(color_background), Colors::ARGB(76, 0, 0, 0));
	color_damage = Colors::Load(ini, Name(), VAR_NAME(color_damage), Colors::ARGB(102, 205, 102, 51));
	color_recent = Colors::Load(ini, Name(), VAR_NAME(color_recent), Colors::ARGB(205, 102, 153, 230));
	hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());
	CSimpleIni::TNamesDepend keys;
	inifile->GetAllKeys(IniSection, keys);
	for (CSimpleIni::Entry key : keys) {
		int lkey;
		if (GuiUtils::ParseInt(key.pItem, &lkey)) {
			if (lkey <= 0) continue;
			long lval = inifile->GetLongValue(IniSection, key.pItem, 0);
			if (lval <= 0) continue;
			hp_map[static_cast<size_t>(lkey)] = static_cast<uint32_t>(lval);
		}
	}
}

void PartyDamage::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), "lock_move", lock_move);

	ini->SetDoubleValue(Name(), VAR_NAME(width), width);
	ini->SetBoolValue(Name(), VAR_NAME(bars_left), bars_left);
	ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
	ini->SetLongValue(Name(), VAR_NAME(recent_max_time), recent_max_time);
	Colors::Save(ini, Name(), VAR_NAME(color_background), color_background);
	Colors::Save(ini, Name(), VAR_NAME(color_damage), color_damage);
	Colors::Save(ini, Name(), VAR_NAME(color_recent), color_recent);
	ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);

	for (const std::pair<DWORD, long>& item : hp_map) {
		std::string key = std::to_string(item.first);
		inifile->SetLongValue(IniSection, key.c_str(), item.second, 0, false, true);
	}
	inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
}

void PartyDamage::DrawSettingInternal() {
	ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
	ImGui::Checkbox("Bars towards the left", &bars_left);
	ImGui::ShowHelp("If unchecked, they will expand to the right");
	ImGui::DragFloat("Width", &width, 1.0f, 50.0f, 0.0f, "%.0f");
	ImGui::InputInt("Row Height", &row_height);
	ImGui::ShowHelp("Height of each row, leave 0 for default");
	if (width <= 0) width = 1.0f;
	ImGui::DragInt("Timeout", &recent_max_time, 10.0f, 1000, 10 * 1000, "%.0f milliseconds");
	if (recent_max_time < 0) recent_max_time = 0;
	ImGui::ShowHelp("After this amount of time, each player recent damage (blue bar) will be reset");
	Colors::DrawSettingHueWheel("Background", &color_background);
	Colors::DrawSettingHueWheel("Damage", &color_damage);
	Colors::DrawSettingHueWheel("Recent", &color_recent);
}
