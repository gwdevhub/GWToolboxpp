#include "PartyDamage.h"

#include <sstream>

#include <imgui_internal.h>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include <GWToolbox.h>
#include <GuiUtils.h>
#include <Modules\Resources.h>
#include <Modules\ToolboxSettings.h>

#define INI_FILENAME "healthlog.ini"
#define IniSection "health"

void PartyDamage::Initialize() {
	ToolboxWidget::Initialize();

	total = 0;
	send_timer = TIMER_INIT();

	GW::StoC::AddCallback<GW::Packet::StoC::P151>(
		std::bind(&PartyDamage::DamagePacketCallback, this, std::placeholders::_1));

	GW::StoC::AddCallback<GW::Packet::StoC::P230>(
		std::bind(&PartyDamage::MapLoadedCallback, this, std::placeholders::_1));

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i].damage= 0;
		damage[i].recent_damage = 0;
		damage[i].last_damage = TIMER_INIT();
	}
}

void PartyDamage::Terminate() {
	ToolboxWidget::Terminate();
	inifile->Reset();
	delete inifile;
}

bool PartyDamage::MapLoadedCallback(GW::Packet::StoC::P230* packet) {
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
	return false;
}

bool PartyDamage::DamagePacketCallback(GW::Packet::StoC::P151* packet) {

	// ignore non-damage packets
	switch (packet->type) {
	case GW::Packet::StoC::P151_Type::damage:
	case GW::Packet::StoC::P151_Type::critical:
	case GW::Packet::StoC::P151_Type::armorignoring:
		break;
	default:
		return false;
	}

	// ignore heals
	if (packet->value >= 0) return false;

	GW::AgentArray agents = GW::Agents::GetAgentArray();

	// get cause agent
	if (packet->cause_id >= agents.size()) return false;
	GW::Agent* cause = agents[packet->cause_id];
	
	if (cause == nullptr) return false;
	if (cause->Allegiance != 0x1) return false;
	auto cause_it = party_index.find(cause->Id);
	if (cause_it == party_index.end()) return false;  // ignore damage done by non-party members

	// get target agent
	GW::Agent* target = agents[packet->target_id];
	if (target == nullptr) return false;
	if (target->LoginNumber != 0) return false; // ignore player-inflicted damage
										        // such as Life bond or sacrifice
	if (target->Allegiance == 0x1) return false; // ignore damage inflicted to allies in general
	// warning: note damage to allied spirits, minions or stones may still trigger
	// you can do damage like that by standing in bugged dart traps in eye of the north
	// or maybe with some skills that damage minions/spirits

	long dmg;
	if (target->MaxHP > 0 && target->MaxHP < 100000) {
		dmg = std::lround(-packet->value * target->MaxHP);
		hp_map[target->PlayerNumber] = target->MaxHP;
	} else {
		auto it = hp_map.find(target->PlayerNumber);
		if (it == hp_map.end()) {
			// max hp not found, approximate with hp/lvl formula
			dmg = std::lround(-packet->value * (target->Level * 20 + 100));
		} else {
			long maxhp = it->second;
			dmg = std::lround(-packet->value * it->second);
		}
	}

	int index = cause_it->second;
	if (index >= MAX_PLAYERS) return false; // something went very wrong.
	if (damage[index].damage == 0) {
		damage[index].agent_id = packet->cause_id;
		if (cause->LoginNumber > 0) {
			damage[index].name = GW::Agents::GetPlayerNameByLoginNumber(cause->LoginNumber);
		} else {
			damage[index].name = L"<A Hero>";
		}
		damage[index].primary = static_cast<GW::Constants::Profession>(cause->Primary);
		damage[index].secondary = static_cast<GW::Constants::Profession>(cause->Secondary);
	}

	damage[index].damage += dmg;
	total += dmg;

	if (visible) {
		damage[index].recent_damage += dmg;
		damage[index].last_damage = TIMER_INIT();
	}
	return false;
}

void PartyDamage::Update(float delta) {
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

	int index = 0;
	for (GW::PlayerPartyMember& player : info->players) {
		long id = players[player.loginnumber].AgentID;
		if (id == GW::Agents::GetPlayerId()) player_index = index;
		party_index[id] = index++;

		for (GW::HeroPartyMember& hero : info->heroes) {
			if (hero.ownerplayerid == player.loginnumber) {
				party_index[hero.agentid] = index++;
			}
		}
	}
	for (GW::HenchmanPartyMember& hench : info->henchmen) {
		party_index[hench.agentid] = index++;
	}
}

void PartyDamage::Draw(IDirect3DDevice9* device) {	
	if (!visible) return;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
	
	int line_height = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
	int size = GW::PartyMgr::GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;

	long max_recent = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max_recent < damage[i].recent_damage) {
			max_recent = damage[i].recent_damage;
		}
	}

	long max = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max < damage[i].damage) {
			max = damage[i].damage;
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(color_background).Value);
	ImGui::SetNextWindowSize(ImVec2(width, (float)(size * line_height)));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(0, true))) {
		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f16));
		float x = ImGui::GetWindowPos().x;
		float y = ImGui::GetWindowPos().y;
		float width = ImGui::GetWindowWidth();
		const int BUF_SIZE = 16;
		char buf[BUF_SIZE];
		for (int i = 0; i < size; ++i) {
			float part_of_max = 0;
			if (max > 0) {
				part_of_max = (float)(damage[i].damage) / max;
			}
			float bar_left = bars_left ? (x + width * (1.0f - part_of_max)) : (x);
			float bar_right = bars_left ? (x + width) : (x + width * part_of_max);
			ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
				ImVec2(bar_left, y + i * line_height),
				ImVec2(bar_right, y + (i + 1) * line_height),
				Colors::Add(color_damage, Colors::ARGB(0, 20, 20, 20)),
				Colors::Add(color_damage, Colors::ARGB(0, 20, 20, 20)),
				Colors::Sub(color_damage, Colors::ARGB(0, 20, 20, 20)),
				Colors::Sub(color_damage, Colors::ARGB(0, 20, 20, 20)));

			float part_of_recent = 0;
			if (max_recent > 0) {
				part_of_recent = (float)(damage[i].recent_damage) / max_recent;
			}
			float recent_left = bars_left ? (x + width * (1.0f - part_of_recent)) : (x);
			float recent_right = bars_left ? (x + width) : (x + width * part_of_recent);
			ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
				ImVec2(recent_left, y + (i + 1) * line_height - 6),
				ImVec2(recent_right, y + (i + 1) * line_height),
				Colors::Add(color_recent, Colors::ARGB(0, 20, 20, 20)),
				Colors::Add(color_recent, Colors::ARGB(0, 20, 20, 20)),
				Colors::Sub(color_recent, Colors::ARGB(0, 20, 20, 20)),
				Colors::Sub(color_recent, Colors::ARGB(0, 20, 20, 20)));

			if (damage[i].damage < 1000) {
				snprintf(buf, BUF_SIZE, "%d", damage[i].damage);
			} else if (damage[i].damage < 1000 * 10) {
				snprintf(buf, BUF_SIZE, "%.2f k", (float)damage[i].damage / 1000);
			} else if (damage[i].damage < 1000 * 1000) {
				snprintf(buf, BUF_SIZE, "%.1f k", (float)damage[i].damage / 1000);
			} else {
				snprintf(buf, BUF_SIZE, "%.2f m", (float)damage[i].damage / (1000 * 1000));
			}
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(x + ImGui::GetStyle().ItemSpacing.x, y + i * line_height),
				IM_COL32(255, 255, 255, 255), buf);

			float perc_of_total = GetPercentageOfTotal(damage[i].damage);
			snprintf(buf, BUF_SIZE, "%.1f %%", perc_of_total);
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(x + width / 2, y + i * line_height),
				IM_COL32(255, 255, 255, 255), buf);
		}
		ImGui::PopFont();
	}
	ImGui::End();
	ImGui::PopStyleColor(); // window bg
	ImGui::PopStyleVar(2);
}

float PartyDamage::GetPartOfTotal(long dmg) const {
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

void PartyDamage::WriteDamageOf(int index, int rank) {
	if (index >= MAX_PLAYERS) return;
	if (index < 0) return;
	if (damage[index].damage <= 0) return;

	if (rank == 0) {
		rank = 1; // start at 1, add 1 for each player with higher damage
		for (int i = 0; i < MAX_PLAYERS; ++i) {
			if (i == index) continue;
			if (damage[i].agent_id == 0) continue;
			if (damage[i].damage > damage[index].damage) ++rank;
		}
	}

	const int size = 130;
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

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());
	CSimpleIni::TNamesDepend keys;
	inifile->GetAllKeys(IniSection, keys);
	for (CSimpleIni::Entry key : keys) {
	#if 1
		int lkey;
		if (GuiUtils::ParseInt(key.pItem, &lkey)) {
			if (lkey <= 0) continue;
			long lval = inifile->GetLongValue(IniSection, key.pItem, 0);
			if (lval <= 0) continue;
			hp_map[lkey] = lval;
		}
	#else
		try {
			long lkey = std::stol(key.pItem);
			if (lkey <= 0) continue;
			long lval = inifile->GetLongValue(IniSection, key.pItem, 0);
			if (lval <= 0) continue;
			hp_map[lkey] = lval;
		} catch (...) {
			// no big deal
		}
	#endif
	}
}

void PartyDamage::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), "lock_move", lock_move);

	ini->SetDoubleValue(Name(), VAR_NAME(width), width);
	ini->SetBoolValue(Name(), VAR_NAME(bars_left), bars_left);
	ini->SetLongValue(Name(), VAR_NAME(row_height), row_height);
	ini->GetLongValue(Name(), VAR_NAME(recent_max_time), recent_max_time);
	Colors::Save(ini, Name(), VAR_NAME(color_background), color_background);
	Colors::Save(ini, Name(), VAR_NAME(color_damage), color_damage);
	Colors::Save(ini, Name(), VAR_NAME(color_recent), color_recent);

	for (const std::pair<DWORD, long>& item : hp_map) {
		std::string key = std::to_string(item.first);
		inifile->SetLongValue(IniSection, key.c_str(), item.second, 0, false, true);
	}
	inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
}

void PartyDamage::DrawSettings() {
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

void PartyDamage::DrawSettingInternal() {
	ImGui::Checkbox("Bars towards the left", &bars_left);
	ImGui::ShowHelp("If unchecked, they will expand to the right");
	ImGui::DragFloat("Width", &width, 1.0f, 50.0f, 0.0f, "%.0f");
	ImGui::InputInt("Row Height", &row_height);
	ImGui::ShowHelp("Height of each row, leave 0 for default");
	if (width <= 0) width = 1.0f;
	ImGui::DragInt("Timeout", &recent_max_time, 10.0f, 1000, 10 * 1000, "%.0f milliseconds");
	if (recent_max_time < 0) recent_max_time = 0;
	ImGui::ShowHelp("After this amount of time, each player recent damage (blue bar) will be reset");
	Colors::DrawSetting("Background", &color_background);
	Colors::DrawSetting("Damage", &color_damage);
	Colors::DrawSetting("Recent", &color_recent);
}
