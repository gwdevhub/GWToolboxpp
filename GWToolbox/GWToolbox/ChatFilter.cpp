#include "ChatFilter.h"

#include <GWCA\GWCA.h>
#include <GWCA\StoCMgr.h>

ChatFilter::ChatFilter() {
	suppress_next_message = false;
	suppress_messages_active = false;

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P081>([&](GWCA::StoC_Pak::P081* pak) -> bool {
		//if (pak->message[0] != 0x108) {
		//	for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
		//	printf("\n");
		//}
		if (suppress_messages_active && ShouldIgnore(pak)) {
			suppress_next_message = true;
			return true;
		}
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P082>([&](GWCA::StoC_Pak::P082* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
		//printf("Received p082\n");
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P083>([](GWCA::StoC_Pak::P083* pak) -> bool {
		//printf("Received p083\n");
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P084>([](GWCA::StoC_Pak::P084* pak) -> bool {
		//printf("Received p084: %ls [%ls]\n", pak->sender_name, pak->sender_guild);
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P085>([&](GWCA::StoC_Pak::P085* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
		//printf("Received p085\n");
	});
}


const wchar_t* ChatFilter::Get1stSegment(GWCA::StoC_Pak::P081* pak) const {
	for (size_t i = 0; pak->message[i] != 0; ++i) {
		if (pak->message[i] == 0x10A) return pak->message + i + 1;
	}
	return nullptr;
};

const wchar_t* ChatFilter::Get2ndSegment(GWCA::StoC_Pak::P081* pak) const {
	for (size_t i = 0; pak->message[i] != 0; ++i) {
		if (pak->message[i] == 0x10B) return pak->message + i + 1;
	}
	return nullptr;
}

DWORD ChatFilter::GetNumericSegment(GWCA::StoC_Pak::P081* pak) const {
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

bool ChatFilter::ShouldIgnore(GWCA::StoC_Pak::P081* pak) const {
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
	case 0x7F6: // player x picks up item y (note: item can be unassigned gold)
	case 0x816: // you gain a skill point
	case 0x817: // player x gained a skill point
		return true;

		// ==== Monster drops: ignored unless gold/ecto for player ====
	case 0x7F0: { // monster/player x drops item y (no assignment)
				  // first segment describes the agent who dropped
		return ShouldIgnoreByAgentThatDropped(Get1stSegment(pak)) && ShouldIgnoreItem(Get2ndSegment(pak));
	}
	case 0x7F1: { // monster x drops item y, your party assigns to player z
				  // 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
				  // <monster> is wchar_t id of several wchars
				  // <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
		GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
		if (me && me->PlayerNumber != GetNumericSegment(pak)) return true;	// ignore drops not for the player
		return ShouldIgnoreItem(Get2ndSegment(pak));
	}

	case 0x8101:
		switch (pak->message[1]) {
		case 0x1868: return true; // teilah takes 10 festival tickets
		case 0x1867: return true; // stay where you are, nine rings is about to begin
		case 0x1869: return true; // big winner! 55 tickets
		case 0x186A: return true; // you win 40 tickets
		case 0x186B: return true; // you win 25 festival tickets
		case 0x186C: return true; // you win 15 festival tickets
		case 0x186D: return true; // did not win 9rings
		default: return false;
		}

	case 0x8102:
		switch (pak->message[1]) {
		case 0x4650: // skill has been updated for pvp
		case 0x4651: // a hero skill has been updated for pvp
			return true;

		case 0x223B: // a party won hall of heroes
		case 0x23E4: // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
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

