#include "ChatFilter.h"

#include <locale>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\StoCMgr.h>

#include <imgui.h>

ChatFilter::ChatFilter() : 
	ally_common_drops(false, IniSection(), L"hide_ally_common_drops", "common drops for allies"),
	self_common_drops(false, IniSection(), L"hide_self_common_drops", "common drops for you"),
	ally_rare_drops(false, IniSection(), L"hide_ally_rare_drops", "rare drops for allies"),
	self_rare_drops(false, IniSection(), L"hide_self_rare_drops", "rare drops for you"),
	skill_point(false, IniSection(), L"hide_skillpoints", "earning skill points"),
	pvp_messages(false, IniSection(), L"hide_pvp_messages", "pvp messages", "Such as 'A skill was updated for pvp!'"),
	hoh(false, IniSection(), L"hide_hoh", "Hall of Heroes winners"),
	favor(false, IniSection(), L"hide_favor", "Divine favor announcements"),
	shingjeabroadwalk(false, IniSection(), L"hide_shingjeabroadwalk", "Shing Jea Broadwalk messages"),
	noonehearsyou(false, IniSection(), L"hide_noonehearsyou", "'No one hears you'"),
	messagebycontent(false, IniSection(), L"hide_by_content", "Hide any messages containing:") {

	suppress_next_message = false;
	suppress_next_p081 = false;

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P081>([&](GW::Packet::StoC::P081* pak) -> bool {
		//if (pak->message[0] != 0x108) {
		//printf(" === P081: === \n");
		//for (size_t i = 0; pak->message[i] != 0; ++i) {
		//	if (pak->message[i] >= ' ' && pak->message[i] <= '~') {
		//		printf("%c", pak->message[i]);
		//	} else {
		//		printf(" 0x%X ", pak->message[i]);
		//	}	
		//}
		//printf("\n");
		//}
		// if ANY is active
		if ((ally_common_drops.value
			|| self_common_drops.value
			|| ally_rare_drops.value
			|| self_rare_drops.value
			|| skill_point.value
			|| pvp_messages.value
			|| hoh.value
			|| favor.value
			|| shingjeabroadwalk.value
			|| messagebycontent.value) 
			&& ShouldIgnore(pak)) {
			suppress_next_message = true;
			return true;
		}
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P082>([&](GW::Packet::StoC::P082* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
		//printf("Received p082\n");
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P083>([](GW::Packet::StoC::P083* pak) -> bool {
		printf("Received p083\n");
		//printf("P083: ");
		//for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
		//printf("\n");
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P084>([](GW::Packet::StoC::P084* pak) -> bool {
		//printf("Received p084: %ls [%ls]\n", pak->sender_name, pak->sender_guild);
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P085>([&](GW::Packet::StoC::P085* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
		//printf("Received p085\n");
	});
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

bool ChatFilter::ShouldIgnoreItem(const wchar_t* item_segment) const {
	if (item_segment == nullptr) return false;		// something went wrong, don't ignore
	if (item_segment[0] == 0xA40) return false;		// don't ignore gold items
	if (item_segment[0] == 0x108
		&& item_segment[1] == 0x10A
		&& item_segment[2] == 0x22D9
		&& item_segment[3] == 0xE7B8
		&& item_segment[4] == 0xE9DD
		&& item_segment[5] == 0x2322) return false;	// don't ignore ectos
	if (item_segment[0] == 0x108
		&& item_segment[1] == 0x10A
		&& item_segment[2] == 0x22EA
		&& item_segment[3] == 0xFDA9
		&& item_segment[4] == 0xDE53
		&& item_segment[5] == 0x2D16) return false; // don't ignore obby shards
	return true;
}

bool ChatFilter::ShouldIgnore(GW::Packet::StoC::P081* pak) {
	if (messagebycontent.value && ((pak->message[0] == 0x108)
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

		for (std::string s : filter_words) {
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
		// already handled
	}

	case 0x777: // I'm level x and x% of the way earning my next skill point	(author is not part of the message)
	case 0x778: // I'm following x			(author is not part of the message)
	case 0x77B: // I'm talking to x			(author is not part of the message)
	case 0x77C: // I'm wielding x			(author is not part of the message)
	case 0x77D: // I'm wielding x and y		(author is not part of the message)
	case 0x781: // I'm targeting x			(author is not part of the message)
	case 0x783: // I'm targeting myself!	(author is not part of the message)
	case 0x7ED: // opening the chest reveals x, which your party reserves for y
	case 0x7F2: // you drop item x
	case 0x7FC: // you pick up item y (note: item can be unassigned gold)
	case 0x807: // player joined the game
		return false;

		// ==== Ignored Messages ====
	case 0x7E0: // party shares gold
	case 0x7DF: // party shares gold ?
		return self_common_drops.value;
	case 0x7F6: // player x picks up item y (note: item can be unassigned gold)
		return ally_common_drops.value;
	case 0x816: // you gain a skill point
		return skill_point.value;
	case 0x817: // player x gained a skill point
		return skill_point.value;
	case 0x87C: // 'no one hears you... '
		return noonehearsyou.value;

		// ==== Monster drops: ignored unless gold/ecto for player ====
	case 0x7F0: { // monster/player x drops item y (no assignment)
				  // first segment describes the agent who dropped
		if (!ShouldIgnoreByAgentThatDropped(Get1stSegment(pak))) return false;
		bool rare = ShouldIgnoreItem(Get2ndSegment(pak));
		if (rare) return self_rare_drops.value;
		else return self_common_drops.value;
		//return ShouldIgnoreByAgentThatDropped(Get1stSegment(pak)) && ShouldIgnoreItem(Get2ndSegment(pak));
	}
	case 0x7F1: { // monster x drops item y, your party assigns to player z
				  // 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
				  // <monster> is wchar_t id of several wchars
				  // <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
		GW::Agent* me = GW::Agents().GetPlayer();
		bool forplayer = (me && me->PlayerNumber == GetNumericSegment(pak));
		bool rare = ShouldIgnoreItem(Get2ndSegment(pak));
		if (forplayer && rare) return self_rare_drops.value;
		if (forplayer && !rare) return self_common_drops.value;
		if (!forplayer && rare) return ally_rare_drops.value;
		if (!forplayer && !rare) return ally_common_drops.value;
		//if (me && me->PlayerNumber != GetNumericSegment(pak)) return true;	// ignore drops not for the player
		//return ShouldIgnoreItem(Get2ndSegment(pak));
		return false;
	}

	case 0x8101:
		switch (pak->message[1]) {
		case 0x1868: // teilah takes 10 festival tickets
		case 0x1867: // stay where you are, nine rings is about to begin
		case 0x1869: // big winner! 55 tickets
		case 0x186A: // you win 40 tickets
		case 0x186B: // you win 25 festival tickets
		case 0x186C: // you win 15 festival tickets
		case 0x186D: // did not win 9rings
			return shingjeabroadwalk.value;
		default: return false;
		}

	case 0x8102:
		switch (pak->message[1]) {
		// 0xEFE is a player message
		case 0x4650: // skill has been updated for pvp
		case 0x4651: // a hero skill has been updated for pvp
			return pvp_messages.value;
		case 0x223B: // a party won hall of heroes
			return hoh.value;
		case 0x23E4: // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
			return favor.value;
		case 0x3772: // I'm under the effect of x
		default:
			return false;
		}

	default:
		//for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
		//printf("\n");
		return false;
	}
}

void ChatFilter::DrawSettings() {
	ImGui::Text("Hide the following messages:");
	ally_common_drops.Draw();
	self_common_drops.Draw();
	ally_rare_drops.Draw();
	self_rare_drops.Draw();
	skill_point.Draw();
	pvp_messages.Draw();
	hoh.Draw();
	favor.Draw();
	shingjeabroadwalk.Draw();
	noonehearsyou.Draw();

	ImGui::Separator();
	messagebycontent.Draw();
	ImGui::Indent();
	ImGui::TextDisabled("(Separated by comma)");
	const int buf_size = 1024 * 16;
	static char buf[buf_size];
	if (ImGui::InputTextMultiline("##filter", buf, buf_size, ImVec2(-1.0f, 0.0f))) {
		filter_words.clear();
		std::string text(buf);
		char separator = ',';
		size_t pos = text.find(separator);
		size_t initialpos = 0;

		while (pos != std::string::npos) {
			std::string s = text.substr(initialpos, pos - initialpos);
			if (!s.empty()) {
				std::transform(s.begin(), s.end(), s.begin(), ::tolower);
				filter_words.insert(s);
			}
			initialpos = pos + 1;
			pos = text.find(separator, initialpos);
		}
		std::string s = text.substr(initialpos, std::min(pos, text.size() - initialpos));
		if (!s.empty()) {
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			filter_words.insert(s);
		}

		printf("filter words are:\n");
		for (std::string s : filter_words) {
			printf("%s\n", s.c_str());
		}
	}
	ImGui::Unindent();
}
