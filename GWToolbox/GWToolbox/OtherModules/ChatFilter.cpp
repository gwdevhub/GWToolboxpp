#include "ChatFilter.h"

#include <locale>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\StoCMgr.h>

#include <imgui.h>
#include "GuiUtils.h"

void ChatFilter::Initialize() {
	ToolboxModule::Initialize();

	strcpy_s(bycontent_buf, "");
	strcpy_s(byauthor_buf, "");

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P081>(
		[&](GW::Packet::StoC::P081* pak) -> bool {
		if ((ally_common_drops
			|| self_common_drops
			|| ally_rare_drops
			|| self_rare_drops
			|| skill_points
			|| pvp_messages
			|| hoh
			|| favor
			|| ninerings
			|| messagebycontent) 
			&& ShouldIgnore(pak)) {
			suppress_next_message = true;
			return true;
		}
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P082>(
		[&](GW::Packet::StoC::P082* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P085>([&](GW::Packet::StoC::P085* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
	});
}

void ChatFilter::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	self_common_drops = ini->GetBoolValue(Name(), "self_common_drops", false);
	ally_common_drops = ini->GetBoolValue(Name(), "ally_common_drops", false);
	self_rare_drops = ini->GetBoolValue(Name(), "self_rare_drops", false);
	ally_rare_drops = ini->GetBoolValue(Name(), "ally_rare_drops", false);
	skill_points = ini->GetBoolValue(Name(), "skill_points", false);
	pvp_messages = ini->GetBoolValue(Name(), "pvp_messages", true);
	hoh = ini->GetBoolValue(Name(), "hoh_messages", false);
	favor = ini->GetBoolValue(Name(), "favor", false);
	ninerings = ini->GetBoolValue(Name(), "ninerings", true);
	noonehearsyou = ini->GetBoolValue(Name(), "noonehearsyou", true);
	lunars = ini->GetBoolValue(Name(), "lunars", true);
	playeraway = ini->GetBoolValue(Name(), "playeraway", false);

	messagebycontent = ini->GetBoolValue(Name(), "messagebycontent", false);
	strcpy_s(bycontent_buf, ini->GetValue(Name(), "bycontentwords", ""));
	ByContent_ParseBuf();

	messagebyauthor = ini->GetBoolValue(Name(), "messagebyauthor", false);
	strcpy_s(byauthor_buf, ini->GetValue(Name(), "byauthorwords", ""));
	ByAuthor_ParseBuf();
}

void ChatFilter::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "self_common_drops", self_common_drops);
	ini->SetBoolValue(Name(), "ally_common_drops", ally_common_drops);
	ini->SetBoolValue(Name(), "self_rare_drops", self_rare_drops);
	ini->SetBoolValue(Name(), "ally_rare_drops", ally_rare_drops);
	ini->SetBoolValue(Name(), "skill_points", skill_points);
	ini->SetBoolValue(Name(), "pvp_messages", pvp_messages);
	ini->SetBoolValue(Name(), "hoh_messages", hoh);
	ini->SetBoolValue(Name(), "favor", favor);
	ini->SetBoolValue(Name(), "ninerings", ninerings);
	ini->SetBoolValue(Name(), "noonehearsyou", noonehearsyou);
	ini->SetBoolValue(Name(), "lunars", lunars);
	ini->SetBoolValue(Name(), "playeraway", playeraway);
	ini->SetBoolValue(Name(), "messagebycontent", messagebycontent);
	ini->SetBoolValue(Name(), "messagebyauthor", messagebyauthor);
	ini->SetValue(Name(), "bycontentwords", bycontent_buf);
	ini->SetValue(Name(), "byauthorwords", "");
}


const wchar_t* ChatFilter::Get1stSegment(GW::Packet::StoC::P081* pak) const {
	for (size_t i = 0; pak->message[i] != 0; ++i) {
		if (pak->message[i] == 0x10A) return pak->message + i + 1;
	}
	return nullptr;
};

const wchar_t* ChatFilter::Get2ndSegment(GW::Packet::StoC::P081* pak) const {
	for (size_t i = 0; pak->message[i] != 0; ++i) {
		if (pak->message[i] == 0x10B) return pak->message + i + 1;
	}
	return nullptr;
}

DWORD ChatFilter::GetNumericSegment(GW::Packet::StoC::P081* pak) const {
	for (size_t i = 0; pak->message[i] != 0; ++i) {
		if (pak->message[i] == 0x10F) return (pak->message[i + 1] & 0xFF);
	}
	return 0;
}

bool ChatFilter::ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const {
	if (agent_segment == nullptr) return false;		// something went wrong, don't ignore
	if (agent_segment[0] == 0xBA9 && agent_segment[1] == 0x107) return false;
	return true;
}

bool ChatFilter::IsRare(const wchar_t* item_segment) const {
	if (item_segment == nullptr) return false;		// something went wrong, don't ignore
	if (item_segment[0] == 0xA40) return true;		// don't ignore gold items
	if (FullMatch(item_segment, { 0x108, 0x10A, 0x22D9, 0xE7B8, 0xE9DD, 0x2322 })) 
		return true;	// don't ignore ectos
	if (FullMatch(item_segment, { 0x108, 0x10A, 0x22EA, 0xFDA9, 0xDE53, 0x2D16 } ))
		return true; // don't ignore obby shards
	return false;
}

bool ChatFilter::FullMatch(const wchar_t* s, const std::initializer_list<wchar_t>& msg) const {
	int i = 0;
	for (wchar_t b : msg) {
		if (s[i++] != b) return false;
	}
	return true;
}


bool ChatFilter::ShouldIgnore(GW::Packet::StoC::P081* pak) {
		//for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
		//printf("\n");

	if (messagebycontent && ((pak->message[0] == 0x108)
		|| (pak->message[0] == 0x8102 && pak->message[1] == 0xEFE))) {

		wchar_t* start = &pak->message[1];
		bool found_start = false;
		wchar_t* end = &pak->message[122];
		bool found_end = false;
		for (int i = 0; pak->message[i] != 0; ++i) {
			if (pak->message[i] == 0x107) {
				start = &pak->message[i + 1];
				found_start = true;
			}
			end = &pak->message[i + 1];
			if (pak->message[i] == 0x1) {
				end = &pak->message[i];
				found_end = true;
				break;
			}
		}
		if (!found_start && !found_end) return false;
		if (start >= end) return false;
		std::string text(start, end);
		std::transform(text.begin(), text.end(), text.begin(), ::tolower);
		printf("'%s'", text.c_str());
		if (!found_start && suppress_next_p081) {
			suppress_next_p081 = false;
			printf(" blocked (because previous)\n");
			return true;
		}

		for (std::string s : bycontent_words) {
			if (text.find(s) != std::string::npos) {
				printf(" ...blocked ('%s')\n", s.c_str());
				if (!found_end) suppress_next_p081 = true;
				return true;
			}
		}
		printf(" ...ok\n");
		return false;
	}

	switch (pak->message[0]) {
		// ==== Messages not ignored ====
	case 0x108: { // player message
		//for (size_t i = 0; pak->message[i] != 0; ++i) {
		//	if (pak->message[i] == 0x108) {
		//		printf("|");
		//	} else if (pak->message[i] == 0x107) {
		//		printf(">");
		//	} else if (pak->message[i] == 0x1) {
		//		printf("<");
		//	} else {
		//		printf("%lc", pak->message[i]);
		//	}
		//}
		//printf("\n");
		//return false;
		break;
	}

	case 0x777: return false; // I'm level x and x% of the way earning my next skill point	(author is not part of the message)
	case 0x778: return false; // I'm following x			(author is not part of the message)
	case 0x77B: return false; // I'm talking to x			(author is not part of the message)
	case 0x77C: return false; // I'm wielding x			(author is not part of the message)
	case 0x77D: return false; // I'm wielding x and y		(author is not part of the message)
	case 0x781: return false; // I'm targeting x			(author is not part of the message)
	case 0x783: return false; // I'm targeting myself!	(author is not part of the message)
	case 0x7CC:
		if (FullMatch(&pak->message[1], { 0x962D, 0xFEB5, 0x1D08, 0x10A, 0xAC2, 0x101, 0x164, 0x1 })) return lunars; // you receive 100 gold
		break;
	case 0x7E0: return self_common_drops; // party shares gold
	case 0x7ED: return false; // opening the chest reveals x, which your party reserves for y
	case 0x7DF: return self_common_drops; // party shares gold ?
	case 0x7F0: { // monster/player x drops item y (no assignment)
				  // first segment describes the agent who dropped, second segment describes the item dropped
		if (!ShouldIgnoreByAgentThatDropped(Get1stSegment(pak))) return false;
		bool rare = IsRare(Get2ndSegment(pak));
		if (rare) return self_rare_drops;
		else return self_common_drops;
	}
	case 0x7F1: { // monster x drops item y, your party assigns to player z
				  // 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
				  // <monster> is wchar_t id of several wchars
				  // <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
		GW::Agent* me = GW::Agents().GetPlayer();
		bool forplayer = (me && me->PlayerNumber == GetNumericSegment(pak));
		bool rare = IsRare(Get2ndSegment(pak));
		if (forplayer && rare) return self_rare_drops;
		if (forplayer && !rare) return self_common_drops;
		if (!forplayer && rare) return ally_rare_drops;
		if (!forplayer && !rare) return ally_common_drops;
		return false;
	}
	case 0x7F2: return false; // you drop item x
	case 0x7F6: return ally_common_drops; // player x picks up item y (note: item can be unassigned gold)
	case 0x7FC: return false; // you pick up item y (note: item can be unassigned gold)
	case 0x807: return false; // player joined the game
	case 0x816: return skill_points; // you gain a skill point
	case 0x817: return skill_points; // player x gained a skill point
	case 0x87B: return noonehearsyou; // 'no one hears you.' (outpost)
	case 0x87C: return noonehearsyou; // 'no one hears you... ' (explorable)

	case 0x8101:
		switch (pak->message[1]) {
		case 0x1868: // teilah takes 10 festival tickets
		case 0x1867: // stay where you are, nine rings is about to begin
		case 0x1869: // big winner! 55 tickets
		case 0x186A: // you win 40 tickets
		case 0x186B: // you win 25 festival tickets
		case 0x186C: // you win 15 festival tickets
		case 0x186D: // did not win 9rings
			return ninerings;
		}
		if (FullMatch(&pak->message[1], { 0x6649, 0xA2F9, 0xBBFA, 0x3C27 })) return lunars; // you will celebrate a festive new year (rocket or popper)
		if (FullMatch(&pak->message[1], { 0x664B, 0xDBAB, 0x9F4C, 0x6742 })) return lunars; // something special is in your future! (lucky aura)
		if (FullMatch(&pak->message[1], { 0x6648, 0xB765, 0xBC0D, 0x1F73 })) return lunars; // you will have a prosperous new year! (gain 100 gold)
		if (FullMatch(&pak->message[1], { 0x664C, 0xD634, 0x91F8, 0x76EF })) return lunars; // your new year will be a blessed one (lunar blessing)
		if (FullMatch(&pak->message[1], { 0x664A, 0xEFB8, 0xDE25, 0x363 })) return lunars; // You will find bad luck in this new year... or bad luck will find you
		break;

	case 0x8102:
		switch (pak->message[1]) {
		// 0xEFE is a player message
		case 0x4650: return pvp_messages; // skill has been updated for pvp
		case 0x4651: return pvp_messages; // a hero skill has been updated for pvp
		case 0x223B: return hoh; // a party won hall of heroes	
		case 0x23E4: return favor; // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
		case 0x3772: return false; // I'm under the effect of x
		}
		break;

	//default:
	//	for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
	//	printf("\n");
	//	return false;
	}

	return false;
}

void ChatFilter::DrawSettingInternal() {
	ImGui::Text("Hide the following messages:");
	ImGui::Checkbox("Common drops for you", &self_common_drops);
	ImGui::Checkbox("Common drops for allies", &ally_common_drops);
	ImGui::Checkbox("Rare drops for you", &self_rare_drops);
	ImGui::ShowHelp("'Rare' here stands for Gold item, Ecto or Obby shard");
	ImGui::Checkbox("Rare drops for allies", &ally_rare_drops);
	ImGui::ShowHelp("'Rare' here stands for Gold item, Ecto or Obby shard");
	ImGui::Checkbox("Earning skill points", &skill_points);
	ImGui::Checkbox("PvP messages", &pvp_messages);
	ImGui::ShowHelp("Such as 'A skill was updated for pvp!'");
	ImGui::Checkbox("Hall of Heroes winners", &hoh);
	ImGui::Checkbox("Favor of the Gods announcements", &favor);
	ImGui::Checkbox("9 Rings messages", &ninerings);
	ImGui::Checkbox("'No one hears you...'", &noonehearsyou);
	ImGui::Checkbox("Lunar fortunes messages", &lunars);
	ImGui::Checkbox("Player x might not reply...", &playeraway);

	ImGui::Separator();
	ImGui::Checkbox("Hide any messages containing:", &messagebycontent);
	ImGui::Indent();
	ImGui::TextDisabled("(Each in a separate line)");
	ImGui::TextDisabled("(Warning: has bugs, ignores more stuff)");
	if (ImGui::InputTextMultiline("##bycontentfilter", bycontent_buf, FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
		ByContent_ParseBuf();
	}
	ImGui::Unindent();

	ImGui::Separator();
	ImGui::Checkbox("Hide any messages from: ", &messagebyauthor);
	ImGui::Indent();
	ImGui::TextDisabled("(Each in a separate line)");
	ImGui::TextDisabled("(Not implemented)");
	if (ImGui::InputTextMultiline("##byauthorfilter", byauthor_buf, FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
		ByAuthor_ParseBuf();
	}
	ImGui::Unindent();
}

void ChatFilter::ParseBuffer(const char* buf, std::set<std::string>& words) {
	words.clear();
	std::string text(buf);
	char separator = '\n';
	size_t pos = text.find(separator);
	size_t initialpos = 0;

	while (pos != std::string::npos) {
		std::string s = text.substr(initialpos, pos - initialpos);
		if (!s.empty()) {
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			words.insert(s);
		}
		initialpos = pos + 1;
		pos = text.find(separator, initialpos);
	}
	std::string s = text.substr(initialpos, std::min(pos, text.size() - initialpos));
	if (!s.empty()) {
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		words.insert(s);
	}

	printf("filter words are:\n");
	for (std::string s : words) {
		printf("%s\n", s.c_str());
	}
}
