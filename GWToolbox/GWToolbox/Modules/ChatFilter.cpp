#include "stdafx.h"
#include "ChatFilter.h"

#include <imgui.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <ImGuiAddons.h>
#include <logger.h>

#include <Modules/Resources.h>
#include <Defines.h>

//#define PRINT_CHAT_PACKETS

static void printchar(wchar_t c) {
	if (c >= L' ' && c <= L'~') {
		printf("%lc", c);
	} else {
		printf("0x%X ", c);
	}
}

void ChatFilter::Initialize() {
	ToolboxModule::Initialize();

	// server messages
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&MessageServer_Entry,
	[this](GW::HookStatus *status, GW::Packet::StoC::MessageServer *pak) -> void {
#ifdef PRINT_CHAT_PACKETS
		printf("P082: id %d, type %d\n", pak->id, pak->type);
#endif // PRINT_CHAT_PACKETS

		GW::Array<wchar_t> *buff = &GW::GameContext::instance()->world->message_buff;
		if (ShouldIgnore(buff->begin()) || ShouldIgnoreByContent(buff->begin(), buff->size())) {
			buff->clear();
			status->blocked = true;
			return;
		}
	});

#ifdef PRINT_CHAT_PACKETS
	GW::StoC::AddCallback<GW::Packet::StoC::P088>(
	[](GW::Packet::StoC::P088 *pak) -> bool {
		printf("P081: agent_id %d, unk1 %d, unk2 ", pak->agent_id, pak->unk1);
		for (int i = 0; i < 8 && pak->unk2[i]; ++i) printchar(pak->unk2[i]);
		printf("\n");
		return false;
	});
#endif // PRINT_CHAT_PACKETS

	// global messages
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageGlobal>(&MessageGlobal_Entry,
	[this](GW::HookStatus *status, GW::Packet::StoC::MessageGlobal* pak) -> void {
#ifdef PRINT_CHAT_PACKETS
		printf("P081: id %d, name ", pak->id);
		for (int i = 0; i < 32 && pak->sender_name[i]; ++i) printchar(pak->sender_name[i]);
		printf(", guild ");
		for (int i = 0; i < 6 && pak->sender_guild[i]; ++i) printchar(pak->sender_guild[i]);
		printf("\n");
#endif // PRINT_CHAT_PACKETS

		GW::Array<wchar_t> *buff = &GW::GameContext::instance()->world->message_buff;

		if (ShouldIgnore(buff->begin()) ||
			ShouldIgnoreByContent(buff->begin(), buff->size())) {
			buff->clear();
			status->blocked = true;
			return;
		}
	});

	// local messages
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&MessageLocal_Entry,
	[this](GW::HookStatus *status, GW::Packet::StoC::MessageLocal *pak) -> void {
#ifdef PRINT_CHAT_PACKETS
		printf("P085: id %d, type %d\n", pak->id, pak->type);
#endif // PRINT_CHAT_PACKETS

		GW::Array<wchar_t> *buff = &GW::GameContext::instance()->world->message_buff;
		wchar_t *sender = GW::Agents::GetPlayerNameByLoginNumber(pak->id);
		if (!sender) return;

		if (ShouldIgnore(buff->begin()) ||
			ShouldIgnoreBySender(sender, 32) ||
			ShouldIgnoreByContent(buff->begin(), buff->size())) {

			buff->clear();
			status->blocked = true;
			return;
		}
	});

	GW::Chat::RegisterLocalMessageCallback(&LocalMessageCallback_Entry,
	[this](GW::HookStatus *status, int channel, wchar_t *message) -> void {
		if (away && ShouldIgnore(message)) {
			status->blocked = true;
		}
	});
}

void ChatFilter::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	self_drop_rare = ini->GetBoolValue(Name(), VAR_NAME(self_drop_rare), false);
	self_drop_common = ini->GetBoolValue(Name(), VAR_NAME(self_drop_common), false);
	ally_drop_rare = ini->GetBoolValue(Name(), VAR_NAME(ally_drop_rare), false);
	ally_drop_common = ini->GetBoolValue(Name(), VAR_NAME(ally_drop_common), false);
	ally_pickup_rare = ini->GetBoolValue(Name(), VAR_NAME(ally_pickup_rare), false);
	ally_pickup_common = ini->GetBoolValue(Name(), VAR_NAME(ally_pickup_common), false);
	skill_points = ini->GetBoolValue(Name(), VAR_NAME(skill_points), false);
	pvp_messages = ini->GetBoolValue(Name(), VAR_NAME(pvp_messages), true);
	hoh = ini->GetBoolValue(Name(), VAR_NAME(hoh_messages), false);
	favor = ini->GetBoolValue(Name(), VAR_NAME(favor), false);
	ninerings = ini->GetBoolValue(Name(), VAR_NAME(ninerings), true);
	noonehearsyou = ini->GetBoolValue(Name(), VAR_NAME(noonehearsyou), true);
	lunars = ini->GetBoolValue(Name(), VAR_NAME(lunars), true);
	messagebycontent = ini->GetBoolValue(Name(), VAR_NAME(messagebycontent), false);
	away = ini->GetBoolValue(Name(), VAR_NAME(away), false);
	you_have_been_playing_for = ini->GetBoolValue(Name(), VAR_NAME(you_have_been_playing_for), false);
	player_has_achieved_title = ini->GetBoolValue(Name(), VAR_NAME(player_has_achieved_title), false);

	{
		std::ifstream file;
		file.open(Resources::GetPath(L"FilterByContent.txt"));
		if (file.is_open()) {
			file.get(bycontent_word_buf, FILTER_BUF_SIZE, '\0');
			file.close();
			ParseBuffer(bycontent_word_buf, bycontent_words);
		}
	}
	{
		std::ifstream file;
		file.open(Resources::GetPath(L"FilterByContent_regex.txt"));
		if (file.is_open()) {
			file.get(bycontent_regex_buf, FILTER_BUF_SIZE, '\0');
			file.close();
			ParseBuffer(bycontent_regex_buf, bycontent_regex);
		}
	}

#ifdef EXTENDED_IGNORE_LIST
	std::ifstream byauthor_file;
	byauthor_file.open(Resources::GetPath(L"FilterByAuthor.txt"));
	if (byauthor_file.is_open()) {
		byauthor_file.get(byauthor_buf, FILTER_BUF_SIZE, '\0');
		byauthor_file.close();
		ByAuthor_ParseBuf();
	}
#endif
}

void ChatFilter::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(self_drop_rare), self_drop_rare);
	ini->SetBoolValue(Name(), VAR_NAME(self_drop_common), self_drop_common);
	ini->SetBoolValue(Name(), VAR_NAME(ally_drop_rare), ally_drop_rare);
	ini->SetBoolValue(Name(), VAR_NAME(ally_drop_common), ally_drop_common);
	ini->SetBoolValue(Name(), VAR_NAME(ally_pickup_rare), ally_pickup_rare);
	ini->SetBoolValue(Name(), VAR_NAME(ally_pickup_common), ally_pickup_common);
	ini->SetBoolValue(Name(), VAR_NAME(skill_points), skill_points);
	ini->SetBoolValue(Name(), VAR_NAME(pvp_messages), pvp_messages);
	ini->SetBoolValue(Name(), VAR_NAME(hoh_messages), hoh);
	ini->SetBoolValue(Name(), VAR_NAME(favor), favor);
	ini->SetBoolValue(Name(), VAR_NAME(ninerings), ninerings);
	ini->SetBoolValue(Name(), VAR_NAME(noonehearsyou), noonehearsyou);
	ini->SetBoolValue(Name(), VAR_NAME(lunars), lunars);
	ini->SetBoolValue(Name(), VAR_NAME(messagebycontent), messagebycontent);
	ini->SetBoolValue(Name(), VAR_NAME(away), away);
	ini->SetBoolValue(Name(), VAR_NAME(you_have_been_playing_for), you_have_been_playing_for);
	ini->SetBoolValue(Name(), VAR_NAME(player_has_achieved_title), player_has_achieved_title);

	if (timer_parse_filters) {
		timer_parse_filters = 0;
		ParseBuffer(bycontent_word_buf, bycontent_words);
		bycontent_filedirty = true;
	}

	if (timer_parse_regexes) {
		timer_parse_regexes = 0;
		ParseBuffer(bycontent_regex_buf, bycontent_regex);
		bycontent_filedirty = true;
	}

	if (bycontent_filedirty) {
		std::ofstream file1;
		file1.open(Resources::GetPath(L"FilterByContent.txt"));
		if (file1.is_open()) {
			file1.write(bycontent_word_buf, strlen(bycontent_word_buf));
			file1.close();
		}
		std::ofstream file2;
		file2.open(Resources::GetPath(L"FilterByContent_regex.txt"));
		if (file2.is_open()) {
			file2.write(bycontent_regex_buf, strlen(bycontent_regex_buf));
			file2.close();
		}
		bycontent_filedirty = false;
	}

#ifdef EXTENDED_IGNORE_LIST
	if (byauthor_filedirty) {
		std::ofstream byauthor_file;
		byauthor_file.open(Resources::GetPath(L"FilterByAuthor.txt"));
		if (byauthor_file.is_open()) {
			byauthor_file.write(byauthor_buf, strlen(byauthor_buf));
			byauthor_file.close();
			byauthor_filedirty = false;
		}
	}
#endif
}


const wchar_t* ChatFilter::Get1stSegment(const wchar_t *message) const {
	for (size_t i = 0; message[i] != 0; ++i) {
		if (message[i] == 0x10A) return message + i + 1;
	}
	return nullptr;
};

const wchar_t* ChatFilter::Get2ndSegment(const wchar_t *message) const {
	for (size_t i = 0; message[i] != 0; ++i) {
		if (message[i] == 0x10B) return message + i + 1;
	}
	return nullptr;
}

DWORD ChatFilter::GetNumericSegment(const wchar_t *message) const {
	for (size_t i = 0; message[i] != 0; ++i) {
		if ((0x100 < message[i] && message[i] < 0x107) || (0x10D < message[i] && message[i] < 0x110))
			return (message[i + 1] - 0x100);
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

bool ChatFilter::ShouldIgnore(const wchar_t *message) {
#ifdef PRINT_CHAT_PACKETS
	for (size_t i = 0; message[i] != 0; ++i) printchar(message[i]);
	printf("\n");
#endif // PRINT_CHAT_PACKETS

	switch (message[0]) {
		// ==== Messages not ignored ====
	case 0x108: return false; // player message
	case 0x76D: return false; // whisper received.
	case 0x76E: return false; // whisper sended.
	case 0x777: return false; // I'm level x and x% of the way earning my next skill point	(author is not part of the message)
	case 0x778: return false; // I'm following x			(author is not part of the message)
	case 0x77B: return false; // I'm talking to x			(author is not part of the message)
	case 0x77C: return false; // I'm wielding x			(author is not part of the message)
	case 0x77D: return false; // I'm wielding x and y		(author is not part of the message)
	case 0x781: return false; // I'm targeting x			(author is not part of the message)
	case 0x783: return false; // I'm targeting myself!	(author is not part of the message)
	case 0x791: return false; // emote agree
	case 0x792: return false; // emote attention
	case 0x793: return false; // emote beckon
	case 0x794: return false; // emote beg
	case 0x795: return false; // emote boo
		// all other emotes, in alphabetical order
	case 0x7BE: return false; // emote yawn
	case 0x7BF: return false; // emote yes
	case 0x7CC:
		if (FullMatch(&message[1], { 0x962D, 0xFEB5, 0x1D08, 0x10A, 0xAC2, 0x101, 0x164, 0x1 })) return lunars; // you receive 100 gold
		break;
	case 0x7E0: return ally_pickup_common; // party shares gold
	case 0x7ED: return false; // opening the chest reveals x, which your party reserves for y
	case 0x7DF: return ally_pickup_common; // party shares gold ?
	case 0x7F0: { // monster/player x drops item y (no assignment)
				  // first segment describes the agent who dropped, second segment describes the item dropped
		if (!ShouldIgnoreByAgentThatDropped(Get1stSegment(message))) return false;
		bool rare = IsRare(Get2ndSegment(message));
		if (rare) return self_drop_rare;
		else return self_drop_common;
	}
	case 0x7F1: { // monster x drops item y, your party assigns to player z
				  // 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
				  // <monster> is wchar_t id of several wchars
				  // <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
		GW::Agent* me = GW::Agents::GetPlayer();
		bool forplayer = (me && me->player_number == GetNumericSegment(message));
		bool rare = IsRare(Get2ndSegment(message));
		if (forplayer && rare) return self_drop_rare;
		if (forplayer && !rare) return self_drop_common;
		if (!forplayer && rare) return ally_drop_rare;
		if (!forplayer && !rare) return ally_drop_common;
		return false;
	}
	case 0x7F2: return false; // you drop item x
	case 0x7F6: { // player x picks up item y (note: item can be unassigned gold)
		bool rare = IsRare(Get2ndSegment(message));
		if (rare) return ally_pickup_rare;
		else	  return ally_pickup_common;
	}
	case 0x7FC: return false; // you pick up item y (note: item can be unassigned gold)
	case 0x807: return false; // player joined the game
	case 0x816: return skill_points; // you gain a skill point
	case 0x817: return skill_points; // player x gained a skill point
	case 0x846: return false; // 'Screenshot saved as <path>'.
	case 0x87B: return noonehearsyou; // 'no one hears you.' (outpost)
	case 0x87C: return noonehearsyou; // 'no one hears you... ' (explorable)
	case 0x87D: return away; // 'Player <name> might not reply...' (Away)
	case 0x87F: return false; // 'Failed to send whisper to player <name>...' (Do not disturb)
	case 0x880: return false; // 'Player name <name> is invalid.'. (Anyone actually saw it ig ?)
	case 0x881: return false; // 'Player <name> is not online.' (Offline)

	case 0x7BF4: return you_have_been_playing_for; // You have been playing for x time.
	case 0x7BF5: return you_have_been_playing_for; // You have been playinf for x time. Please take a break.

	case 0x8101:
		switch (message[1]) {
			// nine rings
		case 0x1867: // stay where you are, nine rings is about to begin
		case 0x1868: // teilah takes 10 festival tickets
		case 0x1869: // big winner! 55 tickets
		case 0x186A: // you win 40 tickets
		case 0x186B: // you win 25 festival tickets
		case 0x186C: // you win 15 festival tickets
		case 0x186D: // did not win 9rings
			// rings of fortune
		case 0x1526: // The rings of fortune did not favor you this time. Stay in the area to try again. 
		case 0x1529: // Pan takes 2 festival tickets
		case 0x152A: // stay right were you are! rings of fortune is about to begin!
		case 0x152B: // you win 12 festival tickets
		case 0x152C: // You win 3 festival tickets
			return ninerings;
        case 0x39CD: // you have a special item available: <special item reward>
            return ninerings;
		case 0x7B91:	// x minutes of favor of the gods remaining. Note: full message is 0x8101 0x7B91 0xC686 0xE490 0x6922 0x101 0x100+value
		case 0x7B92:	// x more achievements must be performed to earn the favor of the gods. // 0x8101 0x7B92 0x8B0A 0x8DB5 0x5135 0x101 0x100+value
			return favor;
		}
		if (FullMatch(&message[1], { 0x6649, 0xA2F9, 0xBBFA, 0x3C27 })) return lunars; // you will celebrate a festive new year (rocket or popper)
		if (FullMatch(&message[1], { 0x664B, 0xDBAB, 0x9F4C, 0x6742 })) return lunars; // something special is in your future! (lucky aura)
		if (FullMatch(&message[1], { 0x6648, 0xB765, 0xBC0D, 0x1F73 })) return lunars; // you will have a prosperous new year! (gain 100 gold)
		if (FullMatch(&message[1], { 0x664C, 0xD634, 0x91F8, 0x76EF })) return lunars; // your new year will be a blessed one (lunar blessing)
		if (FullMatch(&message[1], { 0x664A, 0xEFB8, 0xDE25, 0x363 })) return lunars; // You will find bad luck in this new year... or bad luck will find you
		break;

	case 0x8102:
		switch (message[1]) {
		// 0xEFE is a player message
		case 0x1443: return player_has_achieved_title; // Player has achieved the title...
		case 0x4650: return pvp_messages; // skill has been updated for pvp
		case 0x4651: return pvp_messages; // a hero skill has been updated for pvp
		case 0x223F: return false; // "x minutes of favor of the gods remaining" as a result of /favor command
		case 0x223B: return hoh; // a party won hall of heroes	
		case 0x23E4: return favor; // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
		case 0x23E5: return player_has_achieved_title;
		case 0x23E6: return player_has_achieved_title;
		case 0x2E35: return player_has_achieved_title; // Player has achieved the title...
		case 0x2E36: return player_has_achieved_title; // Player has achieved the title...
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

bool ChatFilter::ShouldIgnoreByContent(const wchar_t *message, size_t size) {
	if (!messagebycontent) return false;
	if (!(message[0] == 0x108 && message[1] == 0x107)
		&& !(message[0] == 0x8102 && message[1] == 0xEFE && message[2] == 0x107)) return false;
	const wchar_t* start = nullptr;
	const wchar_t* end = &message[size];
	for (int i = 0; i < (int)size && message[i] != 0; ++i) {
		if (message[i] == 0x107) {
			start = &message[i + 1];
		} else if (message[i] == 0x1) {
			end = &message[i];
		}
	}
	if (start == nullptr) return false; // no string segment in this packet

	// std::string temp(start, end);
	char buffer[1024];
	utf8::string temp = Unicode16ToUtf8(buffer, 1024, start, end);
	if (!temp.count)
		return false;

	utf8::string text = Utf8Normalize(temp.bytes);
	if (!text.count) {
		return false;
	}

	for (const auto& s : bycontent_words) {
		if (strstr(text.bytes, s.c_str())) {
			free(text);
			return true;
		}
	}
	for (const std::regex& r : bycontent_regex) {
		if (std::regex_match(text.bytes, r)) {
			return true;
		}
	}
	free(text);
	return false;
}

bool ChatFilter::ShouldIgnoreBySender(const wchar_t *sender, size_t size) {
#ifdef EXTENDED_IGNORE_LIST
	if (sender == nullptr) return false;
	char s[32];
	for (size_t i = 0; i < size; i++) {
		if (sender[i] & ~0xff) return false; // We currently don't support non-ascii names
		s[i] = tolower(sender[i]);
		if (sender[i] == 0)
			break;
	}
	if (byauthor_words.find(s) != byauthor_words.end())
		return true;
#endif
	return false;
}

void ChatFilter::DrawSettingInternal() {
	ImGui::Text("Hide the following messages:");
	ImGui::Separator();
	ImGui::Text("Drops");
	ImGui::SameLine();
	ImGui::TextDisabled("('Rare' stands for Gold item, Ecto or Obby shard)");
	ImGui::Checkbox("A rare item drops for you", &self_drop_rare);
	ImGui::Checkbox("A common item drops for you", &self_drop_common);
	ImGui::Checkbox("A rare item drops for an ally", &ally_drop_rare);
	ImGui::Checkbox("A common item drops for an ally", &ally_drop_common);
	ImGui::Checkbox("An ally picks up a rare item", &ally_pickup_rare);
	ImGui::Checkbox("An ally picks up a common item", &ally_pickup_common);

	ImGui::Separator();
	ImGui::Text("Announcements");
	ImGui::Checkbox("Hall of Heroes winners", &hoh);
	ImGui::Checkbox("Favor of the Gods announcements", &favor);
	ImGui::Checkbox("'You have been playing for...'", &you_have_been_playing_for);
	ImGui::Checkbox("'Player x has achieved title...'", &player_has_achieved_title);

	ImGui::Separator();
	ImGui::Text("Others");
	ImGui::Checkbox("Earning skill points", &skill_points);
	ImGui::Checkbox("PvP messages", &pvp_messages);
	ImGui::ShowHelp("Such as 'A skill was updated for pvp!'");
	ImGui::Checkbox("9 Rings messages", &ninerings);
	ImGui::Checkbox("Lunar fortunes messages", &lunars);
	ImGui::Checkbox("'No one hears you...'", &noonehearsyou);
	ImGui::Checkbox("'Player x might not reply because his/her status is set to away'", &away);

	ImGui::Separator();
	ImGui::Checkbox("Hide any messages containing:", &messagebycontent);
	ImGui::Indent();
	ImGui::TextDisabled("(Each in a separate line. Not case sensitive)");
	if (ImGui::InputTextMultiline("##bycontentfilter", bycontent_word_buf, 
		FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
		timer_parse_filters = GetTickCount() + NOISE_REDUCTION_DELAY_MS;
	}
	ImGui::Text("And messages matching regular expressions:");
	ImGui::ShowHelp("Regular expressions allow you to specify wildcards and express more.\nThe syntax is described at www.cplusplus.com/reference/regex/ECMAScript\nNote that the whole message needs to be matched, so for example you might want .* at the end.");
	if (ImGui::InputTextMultiline("##bycontentfilter_regex", bycontent_regex_buf,
		FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0))) {
		timer_parse_regexes = GetTickCount() + NOISE_REDUCTION_DELAY_MS;
	}
	ImGui::Unindent();

#ifdef EXTENDED_IGNORE_LIST
	ImGui::Separator();
	ImGui::Checkbox("Hide any messages from: ", &messagebyauthor);
	ImGui::Indent();
	ImGui::TextDisabled("(Each in a separate line)");
	ImGui::TextDisabled("(Not implemented)");
	if (ImGui::InputTextMultiline("##byauthorfilter", byauthor_buf, FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
		ByAuthor_ParseBuf();
		byauthor_filedirty = true;
	}
	ImGui::Unindent();
#endif // EXTENDED_IGNORE_LIST
}

void ChatFilter::Update(float delta) {
	uint32_t timestamp = GetTickCount();
	
	if (timer_parse_filters && timer_parse_filters < timestamp) {
		timer_parse_filters = 0;
		ParseBuffer(bycontent_word_buf, bycontent_words);
		bycontent_filedirty = true;
	}

	if (timer_parse_regexes && timer_parse_regexes < timestamp) {
		timer_parse_regexes = 0;
		ParseBuffer(bycontent_regex_buf, bycontent_regex);
		bycontent_filedirty = true;
	}
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::string> &words) const {
	words.clear();
	utf8::string normalized_text = Utf8Normalize(text);
	std::istringstream stream(normalized_text.bytes);
	std::string word;
	while (std::getline(stream, word)) {
		if (!word.empty()) {
			words.push_back(word);
		}
	}
	free(normalized_text);
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::wstring> &words) const {
	words.clear();
	wchar_t buffer[1024];
	Utf8ToUnicode(text, buffer, 1024);
	std::wstringstream stream(buffer);
	std::wstring word;
	while (std::getline(stream, word)) {
		if (!word.empty()) {
			words.push_back(word);
		}
	}
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::regex> &regex) const {
	regex.clear();
	std::istringstream stream(text);
	std::string word;
	while (std::getline(stream, word)) {
		if (!word.empty()) {
			// std::transform(word.begin(), word.end(), word.begin(), ::tolower);
			try {
				regex.push_back(std::regex(word));
			} catch (...) {
				Log::Warning("Cannot parse regular expression '%s'", word.c_str());
			}
		}
	}
}
