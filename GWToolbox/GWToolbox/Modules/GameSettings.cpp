#include "stdafx.h"
#include "GameSettings.h"

#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/ItemContext.h>

#include <GWCA/Constants/AgentIDs.h>

#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Packets/CtoSHeaders.h>

#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>


#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <logger.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Timer.h>
#include <Color.h>
#include <Windows\StringDecoderWindow.h>

namespace {



	void SendChatCallback(GW::Chat::Channel chan, wchar_t msg[120]) {
		if (!GameSettings::Instance().auto_url || !msg) return;
		size_t len = wcslen(msg);
		size_t max_len = 120;

		if (chan == GW::Chat::CHANNEL_WHISPER) {
			// msg == "Whisper Target Name,msg"
			size_t i;
			for (i = 0; i < len; i++)
				if (msg[i] == ',')
					break;

			if (i < len) {
				msg += i + 1;
				len -= i + 1;
				max_len -= i + 1;
			}
		}

		if (wcsncmp(msg, L"http://", 7) && wcsncmp(msg, L"https://", 8)) return;

		if (len + 5 < max_len) {
			for (int i = len; i > 0; i--)
				msg[i] = msg[i - 1];
			msg[0] = '[';
			msg[len + 1] = ';';
			msg[len + 2] = 'x';
			msg[len + 3] = 'x';
			msg[len + 4] = ']';
			msg[len + 5] = 0;
		}
	}

	void FlashWindow() {
		FLASHWINFO flashInfo = { 0 };
		flashInfo.cbSize = sizeof(FLASHWINFO);
		flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
		flashInfo.dwFlags = FLASHW_TIMER | FLASHW_TRAY | FLASHW_TIMERNOFG;
		flashInfo.uCount = 0;
		flashInfo.dwTimeout = 0;
		FlashWindowEx(&flashInfo);
	}

	void PrintTime(wchar_t *buffer, size_t n, DWORD time_sec) {
		DWORD secs = time_sec % 60;
		DWORD minutes = (time_sec / 60) % 60;
		DWORD hours = time_sec / 3600;
		DWORD time = 0;
		const wchar_t *time_unit = L"";
		if (hours != 0) {
			time_unit = L"hour";
			time = hours;
		} else if (minutes != 0) {
			time_unit = L"minute";
			time = minutes;
		} else {
			time_unit = L"second";
			time = secs;
		}
		if (time > 1) {
			swprintf(buffer, n, L"%lu %ss", time, time_unit);
		} else {
			swprintf(buffer, n, L"%lu %s", time, time_unit);
		}
	}

	const wchar_t *GetPlayerName(void) {
		GW::Agent *player = GW::Agents::GetPlayer();
		if (!player) return L"";
		DWORD playerNumber = player->player_number;
		return GW::Agents::GetPlayerNameByLoginNumber(playerNumber);
	}

	void WhisperCallback(const wchar_t from[20], const wchar_t msg[140]) {
		GameSettings&  game_setting = GameSettings::Instance();
		if (game_setting.flash_window_on_pm) FlashWindow();
		DWORD status = GW::FriendListMgr::GetMyStatus();
		if (status == GW::FriendStatus_Away && !game_setting.afk_message.empty()) {
			wchar_t buffer[120];
			DWORD diff_time = (clock() - game_setting.afk_message_time) / CLOCKS_PER_SEC;
			wchar_t time_buffer[128];
			PrintTime(time_buffer, 128, diff_time);
			swprintf(buffer, 120, L"Automatic message: \"%s\" (%s ago)", game_setting.afk_message.c_str(), time_buffer);
			// Avoid infinite recursion
			if (wcsncmp(from, ::GetPlayerName(), 20))
				GW::Chat::SendChat(from, buffer);
		}
	}

	int move_materials_to_storage(GW::Item *item) {
		assert(item && item->quantity);
		assert(item->GetIsMaterial());

		int slot = GW::Items::GetMaterialSlot(item);
		if (slot < 0 || (int)GW::Constants::N_MATS <= slot) return 0;
		int availaible = 250;
		GW::Item *b_item = GW::Items::GetItemBySlot(GW::Constants::Bag::Material_Storage, slot + 1);
		if (b_item) availaible = 250 - b_item->quantity;
		int will_move = std::min((int)item->quantity, availaible);
		if (will_move) GW::Items::MoveItem(item, GW::Constants::Bag::Material_Storage, slot, will_move);
		return will_move;
	}

	// From bag_first to bag_last (included) i.e. [bag_first, bag_last]
	// Returns the amount moved
	int complete_existing_stack(GW::Item *item, int bag_first, int bag_last, int remaining) {
		if (!item->GetIsStackable() || remaining == 0) return 0;
		int remaining_start = remaining;
		for (int bag_i = bag_first; bag_i <= bag_last; bag_i++) {
			GW::Bag *bag = GW::Items::GetBag(bag_i);
			if (!bag) continue;
			size_t slot = bag->find2(item);
			while (slot != GW::Bag::npos) {
				GW::Item *b_item = bag->items[slot];
				// b_item can be null in the case of birthday present for instance.
				if (b_item != nullptr) {
					int availaible = 250 - b_item->quantity;
					int will_move = std::min(availaible, remaining);
					if (will_move > 0) {
						GW::Items::MoveItem(item, b_item, will_move);
						remaining -= will_move;
					}
					if (remaining == 0)
						return remaining_start;
				}
				slot = bag->find2(item, slot + 1);
			}
		}
		return remaining_start - remaining;
	}

	void move_to_first_empty_slot(GW::Item *item, int bag_first, int bag_last) {
		for (int bag_i = bag_first; bag_i <= bag_last; bag_i++) {
			GW::Bag *bag = GW::Items::GetBag(bag_i);
			if (!bag) continue;
			size_t slot = bag->find1(0);
			// The reason why we test if the slot has no item is because birthday present have ModelId == 0
			while (slot != GW::Bag::npos) {
				if (bag->items[slot] == nullptr) {
					GW::Items::MoveItem(item, bag, slot);
					return;
				}
				slot = bag->find1(0, slot + 1);
			}
		}
	}

	void move_item_to_storage_page(GW::Item *item, int page) {
		assert(item && item->quantity);
		if (page == static_cast<int>(GW::Constants::StoragePane::Material_Storage)) {
			if (!item->GetIsMaterial()) return;
			move_materials_to_storage(item);
			return;
		}

		if (page < static_cast<int>(GW::Constants::StoragePane::Storage_1) ||
			static_cast<int>(GW::Constants::StoragePane::Storage_14) < page) {

			return;
		}

		const int storage1 = (int)GW::Constants::Bag::Storage_1;
		const int bag_index = storage1 + page;
		assert(GW::Items::GetBag(bag_index));

		int remaining = item->quantity;

		// For materials, we always try to move what we can into the material page
		if (item->GetIsMaterial()) {
			int moved = move_materials_to_storage(item);
			remaining -= moved;
		}

		// if the item is stackable we try to complete stack that already exist in the current storage page
		int moved = complete_existing_stack(item, bag_index, bag_index, remaining);
		remaining -= moved;

		// if there is still item, we find the first empty slot and move everything there
		if (remaining) {
			move_to_first_empty_slot(item, bag_index, bag_index);
		}
	}

	void move_item_to_storage(GW::Item *item) {
		assert(item && item->quantity);

		GW::Bag **bags = GW::Items::GetBagArray();
		if (!bags) return;
		int remaining = item->quantity;

		// We try to move to the material storage
		if (item->GetIsMaterial()) {
			int moved = move_materials_to_storage(item);
			remaining -= moved;
		}

		const int storage1 = (int)GW::Constants::Bag::Storage_1;
		const int storage14 = (int)GW::Constants::Bag::Storage_14;

		// If item is stackable, try to complete similar stack
		if (remaining == 0) return;
		int moved = complete_existing_stack(item, storage1, storage14, remaining);
		remaining -= moved;

		// We find the first empty slot and put the remaining there
		if (remaining) {
			move_to_first_empty_slot(item, storage1, storage14);
		}
	}

	void move_item_to_inventory(GW::Item *item) {
		assert(item && item->quantity);

		const int backpack = (int)GW::Constants::Bag::Backpack;
		const int bag2 = (int)GW::Constants::Bag::Bag_2;

		int total = item->quantity;
		int remaining = total;

		// If item is stackable, try to complete similar stack
		int moved = complete_existing_stack(item, backpack, bag2, remaining);
		remaining -= moved;

		// If we didn't move any item (i.e. there was no stack to complete), move the stack to first empty slot
		if (remaining == total) {
			move_to_first_empty_slot(item, backpack, bag2);
		}
	}

	// This whole section is commented because packets are not up to date after the update. 
	// Should still work if you match the right packets.

#ifdef APRIL_FOOLS
	namespace AF {
		using namespace GW::Packet::StoC;
		void CreateXunlaiAgentFromGameThread(void) {
			{
				NpcGeneralStats packet;
				packet.header = NpcGeneralStats::STATIC_HEADER;
				packet.npc_id = 221;
				packet.file_id = 0x0001c601;
				packet.data1 = 0;
				packet.scale = 0x64000000;
				packet.data2 = 0;
				packet.flags = 0x0000020c;
				packet.profession = 3;
				packet.level = 15;
				wcsncpy(packet.name, L"\x8101\x1f4e\xd020\x87c8\x35a8\x0000", 8);

				GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
			}

			{
				NPCModelFile packet;
				packet.header = NPCModelFile::STATIC_HEADER;
				packet.npc_id = 221;
				packet.count = 1;
				packet.data[0] = 0x0001fc56;

				GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
			}
		}

		void ApplySkinSafe(GW::Agent *agent, DWORD npc_id) {
			if (!agent) return;

			GW::GameContext *game_ctx = GW::GameContext::instance();
			if (game_ctx && game_ctx->character && game_ctx->character->is_explorable)
				return;

			GW::NPCArray &npcs = GW::GameContext::instance()->world->npcs;
			if (!npcs.valid() || npc_id >= npcs.size() || npcs[npc_id].ModelFileID == 0) {
				GW::GameThread::Enqueue([]() {
					CreateXunlaiAgentFromGameThread();
				});
			}

			GW::GameThread::Enqueue([npc_id, agent]() {
				if (!agent->IsPlayer()) return;

				GW::Array<GW::AgentMovement *> &movements = GW::GameContext::instance()->agent->agentmovement;
				if (agent->Id >= movements.size()) return;
				auto movement = movements[agent->Id];
				if (!movement) return;
				if (movement->h001C != 1) return;

				AgentModel packet;
				packet.header = AgentModel::STATIC_HEADER;
				packet.agent_id = agent->Id;
				packet.model_id = npc_id;
				GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
			});
		}
		bool IsItTime() {
			time_t t = time(nullptr);
			tm* utc_tm = gmtime(&t);
			int hour = utc_tm->tm_hour; // 0-23 range
			int day = utc_tm->tm_mday; // 1-31 range
			int month = utc_tm->tm_mon; // 0-11 range
			const int target_month = 3;
			const int target_day = 1;
			if (month != target_month) return false;
			return (day == target_day && hour >= 7) || (day == target_day + 1 && hour < 7);
		}
		void ApplyPatches() {
			// apply skin on agent spawn
			GW::StoC::AddCallback<DisplayCape>(
				[](DisplayCape *packet) -> bool {
				DWORD agent_id = packet->agent_id;
				GW::Agent *agent = GW::Agents::GetAgentByID(agent_id);
				ApplySkinSafe(agent, 221);
				return false;
			});

			// override tonic usage
			GW::StoC::AddCallback<AgentModel>(
				[](AgentModel *packet) -> bool {
				GW::Agent *agent = GW::Agents::GetAgentByID(packet->agent_id);
				if (!(agent && agent->IsPlayer())) return false;
				GW::GameContext *game_ctx = GW::GameContext::instance();
				if (game_ctx && game_ctx->character && game_ctx->character->is_explorable) return false;
				return true; // do not process
			});

			// This apply when you start to everyone in the map
			GW::GameContext *game_ctx = GW::GameContext::instance();
			if (game_ctx && game_ctx->character && !game_ctx->character->is_explorable) {
				GW::AgentArray &agents = GW::Agents::GetAgentArray();
				for (auto agent : agents) {
					ApplySkinSafe(agent, 221);
				}
			}
		}
		void ApplyPatchesIfItsTime() {
			static bool appliedpatches = false;
			if (!appliedpatches && IsItTime()) {
				ApplyPatches();
				appliedpatches = true;
			}
		}
	} 
#endif // APRIL_FOOLS

    // used by chat colors grid
    float chat_colors_grid_x[] = { 0, 100, 160, 240 };

	void SaveChannelColor(CSimpleIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
		char key[128];
		GW::Chat::Color sender, message;
		GW::Chat::GetChannelColors(chan, &sender, &message);
		// @Cleanup: We relie on the fact the Color and GW::Chat::Color are the same format
		snprintf(key, 128, "%s_color_sender", chanstr);
		Colors::Save(ini, section, key, (Color)sender);
		snprintf(key, 128, "%s_color_message", chanstr);
		Colors::Save(ini, section, key, (Color)message);
	}

	void LoadChannelColor(CSimpleIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
		char key[128];
		GW::Chat::Color sender, message;
		GW::Chat::GetDefaultColors(chan, &sender, &message);
		snprintf(key, 128, "%s_color_sender", chanstr);
		sender = (GW::Chat::Color)Colors::Load(ini, section, key, (Color)sender);
		GW::Chat::SetSenderColor(chan, sender);
		snprintf(key, 128, "%s_color_message", chanstr);
		message = (GW::Chat::Color)Colors::Load(ini, section, key, (Color)message);
		GW::Chat::SetMessageColor(chan, message);
	}

    struct PendingAddToParty {
        PendingAddToParty(uint32_t _agent_id, uint32_t _agent_type, uint32_t _allegiance_bits) : agent_id(_agent_id), agent_type(_agent_type), allegiance_bits(_allegiance_bits) {};
        uint32_t agent_id;
        uint32_t agent_type;
        uint32_t allegiance_bits;
    };
    std::vector<PendingAddToParty> allies_to_add;
    std::vector<uint32_t> allies_to_remove;
    std::vector<uint32_t> allies_added_to_party;

    struct PendingSendChatMessage {};
}



typedef void(__fastcall* OnPingEqippedItem_pt)(uint32_t unk1, uint32_t item_id1, uint32_t item_id2);
OnPingEqippedItem_pt OnPingEquippedItem_Func;
OnPingEqippedItem_pt OnPingEquippedItemRet;

std::wstring ShorthandItemDescription(GW::Item* item) {
	std::wstring original(item->info_string);
	std::wsmatch m;

	// Replace "Requires 9 Divine Favor" > "q9 Divine Favor"
	std::wregex regexp_req(L".\x010A\x0AA8\x010A\x0AA9\x010A.\x0001\x0101.\x0001\x0001");
	while (std::regex_search(original, m, regexp_req)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[128];
			wsprintfW(buffer, L"\x0108\x0107, q%d \x0001\x0002%c", found.at(9) - 0x100, found.at(6));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}
	// Replace "Requires 9 Scythe Mastery" > "q9 Scythe Mastery"
	std::wregex regexp_req2(L".\x10A\xAA8\x10A\xAA9\x010A\x8101.\x1\x0101.\x1\x1");
	while (std::regex_search(original, m, regexp_req2)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[128];
			wsprintfW(buffer, L"\x108\x107, q%d \x1\x2\x8101%c", found.at(10) - 0x100, found.at(7));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// "vs. Earth damage" > "Earth"
	// "vs. Demons" > "Demons"
	std::wregex vs_damage(L"[\xAAC\xAAF]\x10A.\x1");
	while (std::regex_search(original, m, vs_damage)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[4];
			wsprintfW(buffer, L"%c", found.at(2));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Replace "Lengthens ??? duration on foes by 33%" > "??? duration +33%"
	std::wregex regexp_lengthens_duration(L"\x0AA4\x010A.\x0001");
	while (std::regex_search(original, m, regexp_lengthens_duration)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"%c\x0002\x0108\x0107 +33%%\x0001", found.at(2));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Replace "Reduces ??? duration on you by 20%" > "??? duration -20%"
	std::wregex regexp_reduces_duration(L"\xAA7\x010A.\x0001");
	while (std::regex_search(original, m, regexp_reduces_duration)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"%c\x0002\x0108\x0107 -20%%\x0001", found.at(2));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Change "Damage 15% (while Health is above 50%)" to "Damage +15^50"
	std::wregex damage_15_over_50(L".\x010A\xA85\x010A\xA4C\x1\x101.\x1\x2" L".\x010A\xAA8\x010A\xABC\x10A\xA52\x1\x101.\x1\x1");
	while (std::regex_search(original, m, damage_15_over_50)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"\xA4C\x0002\x0108\x0107 +%d%%^%d\x0001", found.at(7) - 0x100, found.at(19) - 0x100);
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Change "Enchantments last 20% longer" to "Ench +20%"
	std::wregex enchantments(L"\xAA2\x101.");
	while (std::regex_search(original, m, enchantments)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"\x0108\x0107" L"Enchantments +%d%%\x0001", found.at(2) - 0x100);
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// "(Chance: 18%)" > "(18%)"
	std::wregex chance_regex(L"\xA87\x10A\xA48\x1\x101.");
	while (std::regex_search(original, m, chance_regex)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"\x0108\x0107%d%%\x0001", found.at(5) - 0x100);
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}
	// Change "Halves skill recharge of <attribute> spells" > "HSR <attribute>"
	std::wregex hsr_attribute(L"\xA81\x10A\xA58\x1\x10B.\x1");
	while (std::regex_search(original, m, hsr_attribute)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"\x0108\x0107" L"HSR \x1\x2%c", found.at(5));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}
	// Change "Inscription: "Blah Blah"" to just "Blah Blah"
	std::wregex inscription(L"\x8101\x5DC5\x10A..\x1");
	while (std::regex_search(original, m, inscription)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"%c%c", found.at(3), found.at(4));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Change "Halves casting time of <attribute> spells" > "HCT <attribute>"
	std::wregex hct_attribute(L"\xA81\x10A\xA47\x1\x10B.\x1");
	while (std::regex_search(original, m, hct_attribute)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"\x0108\x0107" L"HCT \x1\x2%c", found.at(5));
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Change "Piercing Dmg: 11-22" > "Piercing: 11-22"
	std::wregex weapon_dmg(L"\xA89\x10A\xA4E\x1\x10B.\x1\x101.\x102.");
	while (std::regex_search(original, m, weapon_dmg)) {
		for (auto match : m) {
			std::wstring found = match.str();
			wchar_t buffer[64];
			wsprintfW(buffer, L"%c\x2\x108\x107: %d-%d\x1", found.at(5),found.at(8) - 0x100, found.at(10) - 0x100);
			original = std::regex_replace(original, std::wregex(found), buffer);
		}
	}

	// Change "Life draining -3, Health regeneration -1" > "Vampiric" (add at end of description)
	std::wregex vampiric(L"\x2\x102\x2.\x10A\xA86\x10A\xA54\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA53\x1\x101.\x1");
	if (std::regex_search(original, vampiric)) {
		original = std::regex_replace(original, vampiric, L"");
		original += L"\x2\x102\x2\x108\x107" L"Vampiric\x1";
	}
	// Change "Energy gain on hit 1, Energy regeneration -1" > "Zealous" (add at end of description)
	std::wregex zealous(L"\x2\x102\x2.\x10A\xA86\x10A\xA50\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA51\x1\x101.\x1");
	if (std::regex_search(original, zealous)) {
		original = std::regex_replace(original, zealous, L"");
		original += L"\x2\x102\x2\x108\x107" L"Zealous\x1";
	}

	// Change "Damage" > "Dmg"
	original = std::regex_replace(original, std::wregex(L"\xA4C"), L"\xA4E");

	// Change Bow "Two-Handed" > ""
	original = std::regex_replace(original, std::wregex(L"\x8102\x1227"), L"\xA3E");

	// Change "Halves casting time of spells" > "HCT"
	original = std::regex_replace(original, std::wregex(L"\xA80\x010A\xA47\x1"), L"\x0108\x0107" L"HCT\x1");

	// Change "Halves skill recharge of spells" > "HSR"
	std::wregex half_skill_recharge(L"\xA80\x010A\xA58\x1");
	original = std::regex_replace(original, half_skill_recharge, L"\x0108\x0107" L"HSR\x1");

	// Remove (Stacking) and (Non-stacking) rubbish
	std::wregex stacking_non_stacking(L"\x0002.\x010A\x0AA8\x010A(\x0AB1|\x0AB2)\x0001\x0001");
	original = std::regex_replace(original, stacking_non_stacking, L"");

	// Replace (while xxx) to just (xxx)
	original = std::regex_replace(original, std::wregex(L"\x0AB4"), L"\x0108\x0107" L"Attacking\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0AB5"), L"\x0108\x0107" L"Casting\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0AB6"), L"\x0108\x0107" L"Condition\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0AB7"), L"\x0108\x0107" L"Enchanted\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0AB8"), L"\x0108\x0107" L"Hexed\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0AB9"), L"\x0108\x0107" L"Stance\x0001");
	original = std::regex_replace(original, std::wregex(L"\x0ABA"), L"\x0108\x0107" L"Stance\x0001");

	return original;
}

std::wstring ParseItemDescription(GW::Item* item) {
	std::wstring original = ShorthandItemDescription(item);

    // Remove "Value: 122 gold"
    original = std::regex_replace(original, std::wregex(L"\x0002\x0102\x0002\x0A3E\x010A\x0A8A\x010A\x0A59\x0001\x010B.\x0101.\x0001\x0001"), L"");

    // Remove other "greyed" generic terms e.g. "Two-Handed", "Unidentified"	
	original = std::regex_replace(original, std::wregex(L"\x0002\x0102\x0002\x0A3E\x010A.\x0001"), L"");

	// Remove "Necromancer Munne sometimes gives these to me in trade" etc
	original = std::regex_replace(original, std::wregex(L"\x0002\x0102\x0002.\x010A\x8102.\x0001"), L"");

	// Remove "Inscription: None"
	original = std::regex_replace(original, std::wregex(L"\x0002\x0102\x0002.\x010A\x8101\x5A1F\x0001"), L"");

	// Remove "Crafted in tribute to an enduring legend."
	original = std::regex_replace(original, std::wregex(L"\x2\x0102\x2.\x010A\x8103\xB5A\x1"), L"");

	// Remove "20% Additional damage during festival events" > "Dmg +20% (Festival)"
	original = std::regex_replace(original, std::wregex(L".\x010A\x108\x10A\x8103\xB71\x101\x100\x1\x1"), L"\xA85\x10A\xA4E\x1\x0101\x114\x2\xAA8\x10A\x108\x107" L"Festival\x1\x1");

	if (item->customized) {
		// Remove "\nDamage +20%" > "\n"
		original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\xA85\x10A(\xA4C|\xA4E)\x1\x0101\x114\x1"), L"");
		// Append "Customized"
		original += L"\x0002\x0102\x0002\x0108\x0107" L"Customized\x0001";
	}

    return original;
}

void __fastcall OnPingEquippedItem(uint32_t oneC, uint32_t item_id1, uint32_t item_id2) {
    GW::HookBase::EnterHook();
    OnPingEquippedItemRet(oneC, item_id1, item_id2);
	if (!GameSettings::Instance().shorthand_item_ping) {
		GW::HookBase::LeaveHook();
		return;
	}
    GW::Item* item;
    GW::Player* p = GW::PlayerMgr::GetPlayerByID(GW::Agents::GetPlayer()->login_number);
    if (item_id1) {
        item = GW::GameContext::instance()->items->item_array[item_id1];
		#ifdef _DEBUG
			printf("Item 1 Name:\n");
			StringDecoderWindow::PrintEncStr(item->name_enc);
			printf("Item 1 Desc:\n");
			StringDecoderWindow::PrintEncStr(item->info_string);
		#endif
        std::wstring item_description = ParseItemDescription(item);
        PendingChatMessage* m = new PendingChatMessage(GW::Chat::Channel::CHANNEL_GROUP, item_description.c_str(), p->name_enc);
        if (!m->invalid) {
            m->SendIt();
            GameSettings::Instance().pending_messages.push_back(m);
        }
    }
    if (item_id2) {
        item = GW::GameContext::instance()->items->item_array[item_id2];
		#ifdef _DEBUG
			printf("Item 2 Name:\n");
			StringDecoderWindow::PrintEncStr(item->name_enc);
			printf("Item 2 Desc:\n");
			StringDecoderWindow::PrintEncStr(item->info_string);
		#endif
        std::wstring item_description2 = ParseItemDescription(item);
        PendingChatMessage* m2 = new PendingChatMessage(GW::Chat::Channel::CHANNEL_GROUP, item_description2.c_str(), p->name_enc);
        if (!m2->invalid) {
            m2->SendIt();
            GameSettings::Instance().pending_messages.push_back(m2);
        }
    }
    GW::HookBase::LeaveHook();
}

const bool PendingChatMessage::SendMessage() {
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    std::vector<std::wstring> sanitised_lines = SanitiseForSend();
    wchar_t buf[120];
    size_t len = 0;
    for (size_t i = 0; i < sanitised_lines.size(); i++) {
        size_t san_len = sanitised_lines[i].length();
        const wchar_t* str = sanitised_lines[i].c_str();
        if (len + san_len + 3 > 120) {
            GW::Chat::SendChat('#', buf);
            buf[0] = '\0';
            len = 0;
        }
        if (len) {
            buf[len] = ',';
            buf[len + 1] = ' ';
            len += 2;
        }
        for (size_t i = 0; i < san_len; i++) {
            buf[len] = str[i];
            len++;
        }
        buf[len] = '\0';
    }
    if (len) {
        GW::Chat::SendChat('#', buf);
    }
    printed = true;
    return printed;
}
const bool PendingChatMessage::PrintMessage() {
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    if (this->printed) return true; // Already printed.
    GW::Chat::Color senderCol;
    GW::Chat::Color messageCol;

    wchar_t buffer[512];
    switch (channel) {
    case GW::Chat::Channel::CHANNEL_GROUP:
        GW::Chat::GetChannelColors(GW::Chat::CHANNEL_GROUP, &senderCol, &messageCol);   // Sender should be same color as emote sender

        swprintf(buffer, 256, L"<c=#%06X><a=2>%ls</a></c>: <c=#%06X>%ls</c>", senderCol & 0x00FFFFFF, output_sender.c_str(), messageCol & 0x00FFFFFF, output_message.c_str());
        GW::Chat::WriteChat(GW::Chat::CHANNEL_GROUP, buffer);
        break;
    case GW::Chat::Channel::CHANNEL_EMOTE:
        GW::Chat::Color dummy; // Needed for GW::Chat::GetChannelColors
        GW::Chat::GetChannelColors(GW::Chat::CHANNEL_EMOTE, &senderCol, &dummy);   // Sender should be same color as emote sender
        GW::Chat::GetChannelColors(GW::Chat::CHANNEL_ALLIES, &dummy, &messageCol); // ...but set the message to be same color as ally chat

        swprintf(buffer, 512, L"<c=#%06X>%ls</c>: <c=#%06X>%ls</c>", senderCol & 0x00FFFFFF, output_sender.c_str(), messageCol & 0x00FFFFFF, output_message.c_str());
        GW::Chat::WriteChat(GW::Chat::CHANNEL_EMOTE, buffer);
        break;
    }
    output_message.clear();
    output_sender.clear();
    printed = true;
    return printed;
};



void GameSettings::Initialize() {
	ToolboxModule::Initialize();
	// Open links on player name click
	// Ctrl click name to target (and add to party)
	// Ctrl+shift to invite to party
	GW::Chat::AddStartWhisperCallback([&](wchar_t* name) -> bool {
		if (openlinks && name && (!wcsncmp(name, L"http://", 7) || !wcsncmp(name, L"https://", 8))) {
			ShellExecuteW(NULL, L"open", name, NULL, NULL, SW_SHOWNORMAL);
			return true;
		}
		if (!name || !ImGui::GetIO().KeysDown[VK_CONTROL])
			return false;
		if (ImGui::GetIO().KeysDown[VK_SHIFT] && GW::PartyMgr::GetPlayerIsLeader()) {
			wchar_t buf[64];
			swprintf(buf, 64, L"invite %ls", name);
			GW::Chat::SendChat('/', buf);
		}
		GW::Player* player = GW::PlayerMgr::GetPlayerByName(name);
		if (player && GW::Agents::GetAgentByID(player->agent_id)) {
			GW::Agents::ChangeTarget(player->agent_id);
		}
		return true;
	});
	{
		// Patch that allow storage page (and Anniversary page) to work... (ask Ziox for more info)
		uintptr_t found = GW::Scanner::Find("\xEB\x00\x33\xC0\xBE\x06", "x?xxxx", -4);
		printf("[SCAN] StoragePatch = %p\n", (void *)found);

		// Xunlai Chest has a behavior where if you
		// 1. Open chest on page 1 to 14
		// 2. Close chest & open it again
		// -> You should still be on the same page
		// But, if you try with the material page (or anniversary page in the case when you bought all other storage page)
		// you will get back the the page 1. I think it was a intended use for material page & forgot to fix it
		// when they added anniversary page so we do it ourself.
		DWORD page_max = 14;
		ctrl_click_patch = new GW::MemoryPatcher(found, &page_max, 1);
		ctrl_click_patch->TooglePatch(true);
	}

	{
		uintptr_t found = GW::Scanner::Find("\xEC\x6A\x00\x51\x8B\x4D\xF8\xBA\x47", "xxxxxxxxx", -9);
		printf("[SCAN] TomePatch = %p\n", (void *)found);
		if (found) {
			tome_patch = new GW::MemoryPatcher(found, "\x75\x1E\x90\x90\x90\x90\x90", 7);
		}
	}

	{
		uintptr_t found = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxxxx", +7);
		printf("[SCAN] GoldConfirmationPatch = %p\n", (void *)found);
		if (found) {
			gold_confirm_patch = new GW::MemoryPatcher(found, "\x90\x90", 2);
		}
	}
    // Automatically return to outpost on defeat
    GW::StoC::AddCallback<GW::Packet::StoC::PartyDefeated>([&](GW::Packet::StoC::PartyDefeated*) -> bool {
        if (!auto_return_on_defeat || !GetPlayerIsLeader())
            return false;
        struct ReturnToOutpostPacket { const uint32_t header = CtoGS_MSGReturnToOutpost; };
        static ReturnToOutpostPacket pak;
        GW::CtoS::SendPacket(&pak);
        return false;
    });
    // Remove certain NPCs from party window when dead
    GW::StoC::AddCallback<GW::Packet::StoC::AgentState>([&](GW::Packet::StoC::AgentState* pak) -> bool {
        if (!add_special_npcs_to_party_window || pak->state != 16)
            return false; // Not dead.
        if (ShouldRemoveAgentFromPartyWindow(pak->agent_id))
            allies_to_remove.push_back(pak->agent_id);
        return false;
    });
    // Remove certain NPCs from party window when despawned
    GW::StoC::AddCallback<GW::Packet::StoC::AgentRemove>([&](GW::Packet::StoC::AgentRemove* pak) -> bool {
        if (ShouldRemoveAgentFromPartyWindow(pak->agent_id))
            allies_to_remove.push_back(pak->agent_id);
        return false;
    });
    // Add certain NPCs to party window when spawned
    GW::StoC::AddCallback<GW::Packet::StoC::AgentAdd>([&](GW::Packet::StoC::AgentAdd* pak) -> bool {
            if (!add_special_npcs_to_party_window)
                return false;
            if (!ShouldAddAgentToPartyWindow(pak))
                return false;
            allies_to_add.push_back({ pak->agent_id,pak->agent_type,pak->allegiance_bits });
            return false;
        });
    // Flash/focus window on trade
    GW::StoC::AddCallback<GW::Packet::StoC::TradeStart>([&](GW::Packet::StoC::TradeStart*) -> bool {
        if(flash_window_on_trade) 
            FlashWindow();
        if (focus_window_on_trade) {
            HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
            SetForegroundWindow(hwnd);
            ShowWindow(hwnd, SW_RESTORE);
        }
        return false;
    });
    // Flash window on party member added
	GW::StoC::AddCallback<GW::Packet::StoC::PartyPlayerAdd>([&](GW::Packet::StoC::PartyPlayerAdd*) -> bool {
        if (!GW::Agents::GetPlayerId())
            return false;
		if (flash_window_on_party_invite && GetPlayerIsLeader())
            FlashWindow();
        check_message_on_party_change = true;
		return false;
	});
    // Trigger for message on party change
	GW::StoC::AddCallback<GW::Packet::StoC::PartyPlayerRemove>([&](GW::Packet::StoC::PartyPlayerRemove*) -> bool {
		check_message_on_party_change = true;
		return false;
	});
    // Flash/focus window on zoning (and a bit of housekeeping)
	GW::StoC::AddCallback<GW::Packet::StoC::GameSrvTransfer>([&](GW::Packet::StoC::GameSrvTransfer *pak) -> bool {
        allies_added_to_party.clear();
        allies_to_remove.clear();
		if (flash_window_on_zoning) FlashWindow();
		if (focus_window_on_zoning && pak->is_explorable) {
			HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
			SetForegroundWindow(hwnd);
			ShowWindow(hwnd, SW_RESTORE);
		}
		return false;
	});
    // Automatically skip cinematics
	GW::StoC::AddCallback<GW::Packet::StoC::CinematicPlay>([&](GW::Packet::StoC::CinematicPlay *packet) -> bool {
        if (packet->play && auto_skip_cinematic) {
            GW::Map::SkipCinematic();
            return false;
        }
        if (flash_window_on_cinematic)
            FlashWindow();
		return false;
	});
	// - Print NPC speech bubbles to emote chat.
	GW::StoC::AddCallback<GW::Packet::StoC::SpeechBubble>([&](GW::Packet::StoC::SpeechBubble *pak) -> bool {
		if (!npc_speech_bubbles_as_chat || !pak->message || !pak->agent_id)
            return false; // Disabled, invalid, or pending another speech bubble
        if (emulated_speech_bubble) {
            emulated_speech_bubble = false;
            return false; // Toolbox generated, skip
        }
        size_t len = 0;
        for (size_t i = 0; pak->message[i] != 0; i++)
            len = i + 1;
		if (len < 3)
            return false; // Shout skill etc
		GW::Agent* agent = GW::Agents::GetAgentByID(pak->agent_id);
		if (!agent || agent->login_number) return false; // Agent not found or Speech bubble from player e.g. drunk message.
        PendingChatMessage* m = new PendingChatMessage(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, PendingChatMessage::GetAgentNameEncoded(agent));
        if (!m->invalid) {
            //Log::Log("Push back now\n");
            pending_messages.push_back(m);
        }
        //Log::Log("SpeechBubble processed, pending message cnt = %d\n", pending_messages.size());
		return false; // Consume.
	});
    // - NPC dialog messages to emote chat
    GW::StoC::AddCallback<GW::Packet::StoC::DisplayDialogue>([&](GW::Packet::StoC::DisplayDialogue* pak) -> bool {
            if (!redirect_npc_messages_to_emote_chat)
                return false; // Disabled or message pending
            PendingChatMessage* m = new PendingChatMessage(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, pak->name);
        if(!m->invalid) pending_messages.push_back(m);
        return true;
    });
    // - NPC teamchat messages to emote chat (emulate speech bubble instead)
    GW::StoC::AddCallback<GW::Packet::StoC::MessageNPC>([&](GW::Packet::StoC::MessageNPC* pak) -> bool {
            if (!redirect_npc_messages_to_emote_chat || !pak->sender_name)
                return false; // Disabled or message pending
            GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size())
                return true; // No buffer, may have already been cleared.
            PendingChatMessage* m = new PendingChatMessage(GW::Chat::Channel::CHANNEL_EMOTE, buff->begin(), pak->sender_name);
            if (!m->invalid) pending_messages.push_back(m);
            if (pak->agent_id) {
                wchar_t msg[122];
                wcscpy(msg, buff->begin()); // Copy from the message buffer, then clear it.
                // Then forward the message on to speech bubble
                uint32_t agent_id = pak->agent_id;
                {
                    GW::GameThread::Enqueue([&, msg, agent_id]() {
                        GW::Packet::StoC::SpeechBubble packet;
                        packet.header = GW::Packet::StoC::SpeechBubble::STATIC_HEADER;
                        packet.agent_id = agent_id;
                        wcscpy(packet.message, msg);
                        emulated_speech_bubble = true;
                        GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase*) &packet);
                    });
                }
            }
            buff->clear();
            return true;
        });
    // - Allow clickable name when a player pings "I'm following X" or "I'm targeting X"
    GW::StoC::AddCallback<GW::Packet::StoC::MessageLocal>([&](GW::Packet::StoC::MessageLocal* pak) -> bool {
        if (pak->channel != static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GROUP) || !pak->player_number)
            return false; // Not team chat or no sender
        GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
        if (!buff || !buff->valid() || !buff->size())
            return true; // No buffer, may have already been cleared.
        wchar_t* message = buff->begin();
        if ((message[0] != 0x778 && message[0] != 0x781) || message[1] != 0x10A || message[2] != 0xBA9 || message[3] != 0x107)
            return false; // Not "I'm Following X" or "I'm Targeting X" message.
        GW::Player* sender = GW::PlayerMgr::GetPlayerByID(pak->player_number);
        if (!sender)
            return false;

        std::wstring output(message);
        if (flash_window_on_name_ping) {
            std::wstring player_pinged = output.substr(4, output.size() - 6);
            if (!wcsncmp(GW::GameContext::instance()->character->player_name, player_pinged.c_str(), 20))
                FlashWindow(); // Flash window - we've been followed!
        }
        output.insert(4, L"<a=1>");
        output.insert(output.size() - 2, L"</a>");
        PendingChatMessage* m = new PendingChatMessage(GW::Chat::Channel::CHANNEL_GROUP, output.c_str(), sender->name_enc);
        if (!m->invalid) pending_messages.push_back(m);
        buff->clear();
        return true; // consume original packet.
    });
    // - Show a message when player joins the outpost
    GW::StoC::AddCallback<GW::Packet::StoC::PlayerJoinInstance>([&](GW::Packet::StoC::PlayerJoinInstance* pak) -> bool {
        if (!notify_when_players_join_outpost && !notify_when_friends_join_outpost)
            return false; // Dont notify about player joining
        if (!pak->player_name || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
            return false; // Only message in an outpost.
        if (!GW::Agents::GetPlayerId())
            return false; // Current player not loaded in yet
        GW::Agent* agent = GW::Agents::GetAgentByID(pak->agent_id);
        if (agent)
            return false; // Player already joined
        GW::Friend* f = GetOnlineFriend(nullptr, pak->player_name);
        if (!f) {
            if (!notify_when_players_join_outpost)
                return false; // Notify on friends only, but this player aint yer pal, buddy.
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> entered the outpost.", pak->player_name);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
        else {
            // Friend
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> (%ls) entered the outpost.", f->charname, f->account);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
        return false;
    });
    // - Show a message when player leaves the outpost
    GW::StoC::AddCallback<GW::Packet::StoC::PlayerLeaveInstance>([&](GW::Packet::StoC::PlayerLeaveInstance* pak) -> bool {
        if (!notify_when_players_leave_outpost && !notify_when_friends_leave_outpost)
            return false; // Dont notify about player leaving
        if (!pak->player_number || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
            return false; // Only message in an outpost.
        if (pak->player_number >= GW::PlayerMgr::GetPlayerArray().size())
            return false; // Not a valid player.
        wchar_t* player_name = GW::PlayerMgr::GetPlayerName(pak->player_number);
        if (!player_name) 
            return false; // Failed to get name
        GW::Friend* f = GetOnlineFriend(nullptr, player_name);
        if (!f) {
            if(!notify_when_players_leave_outpost)
                return false; // Notify on friends only, but this player aint yer pal, buddy.
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> left the outpost.", player_name);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        } else {
            // Friend
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> (%ls) left the outpost.", f->charname, f->account);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
        return false;
    });

	GW::FriendListMgr::SetOnFriendStatusCallback(GameSettings::FriendStatusCallback);



    OnPingEquippedItem_Func = (OnPingEqippedItem_pt)GW::Scanner::Find("\x8D\x4D\xF0\xC7\x45\xF0\x2B", "xxxxxxx", -0xC); // NOTE: 0x2B is CtoS header
    printf("[SCAN] OnPingEquippedItem = %p\n", OnPingEquippedItem_Func);
    if (OnPingEquippedItem_Func) {
        GW::HookBase::CreateHook(OnPingEquippedItem_Func, OnPingEquippedItem, (void**)& OnPingEquippedItemRet);
        GW::HookBase::EnableHooks(OnPingEquippedItem_Func);
    }

#ifdef APRIL_FOOLS
	AF::ApplyPatchesIfItsTime();
#endif

}
// Same as GW::PartyMgr::GetPlayerIsLeader() but has an extra check to ignore disconnected people.
bool GameSettings::GetPlayerIsLeader() {
    GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
    if (!party) return false;
    GW::Agent* player_agent = GW::Agents::GetPlayer();
    if (!player_agent) return false;
    if (!party->players.size()) return false;
    for (size_t i = 0; i < party->players.size(); i++) {
        if (!party->players[i].connected())
            continue;
        return (party->players[i].login_number == player_agent->login_number);
    }
    return false;
}

bool GameSettings::ShouldRemoveAgentFromPartyWindow(uint32_t agent_id) {
    GW::Agent* a = GW::Agents::GetAgentByID(agent_id);
    if (!a || !(a->type_map & 0x20000) || a->allegiance != 6)
        return false; // Not in party window or not ally.
    for (size_t i = 0; i < allies_added_to_party.size(); i++) {
        if (a->agent_id != allies_added_to_party[i])
            continue;
        return true;
    }
    return false;
}
bool GameSettings::ShouldAddAgentToPartyWindow(GW::Packet::StoC::AgentAdd* pak) {
    if ((pak->agent_type & 0x20000000) == 0)
        return false; // Not an NPC
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
        return false; // Not in an explorable area
    GW::Constants::MapID map_id = GW::Map::GetMapID();
	uint32_t player_number = (pak->agent_type ^ 0x20000000);
    switch (player_number) {
		// PVP
	case 3087: // Luxon Longbow
	case 3588: // Luxon monk
	case 3024: // Luxon priest
	case 3032: // Luxon warrior
		return false; // wip
	case 3369: // Kurz warrior
	case 3370: // Kurzick ranger
	case 3367: // Kurz ele
	case 3030: // Kurz necro
		return false; // wip
    case 2349: // Vale friendly spirit 1
    case 2350: // Vale friendly spirit 2
    case 2358: // Pits friendly spirit 1
    case 2359: // Pits friendly spirit 2
    case 2360: // Pits friendly spirit 3
    case 2351: // Pits friendly spirit 4
        return map_id == GW::Constants::MapID::The_Underworld;
    case 3582: // Siege turtle, Gyala hatchery
        return map_id == GW::Constants::MapID::Gyala_Hatchery_outpost_mission;
    case 4998: // Captain Sulahresh (maybe filter this one by encoded name as it shares id with other snek bois)
        return map_id == GW::Constants::MapID::Domain_of_Anguish;
    case GW::Constants::ModelID::FoW::Griffons:
    case GW::Constants::ModelID::FoW::Forgemaster:
        return map_id == GW::Constants::MapID::The_Fissure_of_Woe;
    case 5843: // Mursaat Elementalist (Polymock summon)
    case 5844: // Flame Djinn (Polymock summon)
    case 5845: // Ice Imp (Polymock summon)
    case 5846: // Naga Shaman (Polymock summon)
    case 5848: // Ebon Vanguard Assassin
        return true;
    }
    return false;
}
// Helper function; avoids doing string checks on offline friends.
GW::Friend* GameSettings::GetOnlineFriend(wchar_t* account, wchar_t* playing) {
    if (!(account || playing)) return NULL;
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    if (!fl) return NULL;
    uint32_t n_friends = fl->number_of_friend;
    GW::FriendsListArray& friends = fl->friends;
    for (GW::Friend* it : friends) {
        if (!it) continue;
        if (it->type != GW::FriendType_Friend) continue;
        if (it->status != GW::FriendStatus::FriendStatus_Online) continue;
        if (account && !wcsncmp(it->account, account, 20))
            return it;
        if (playing && !wcsncmp(it->charname, playing, 20))
            return it;
    }
    return NULL;
}
void GameSettings::MessageOnPartyChange() {
    if (!check_message_on_party_change || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return; // Don't need to check, or not an outpost.
	GW::PartyInfo* current_party = GW::PartyMgr::GetPartyInfo();
    GW::Agent* me = GW::Agents::GetPlayer();
	if (!me || !current_party || !current_party->players.valid())
		return; // Party not ready yet.
    bool is_leading = false;
	std::vector<std::wstring> current_party_names;
    GW::PlayerPartyMemberArray current_party_players = current_party->players; // Copy the player array here to avoid ptr issues.
	for (size_t i = 0; i < current_party_players.size(); i++) {
        if (!current_party_players[i].login_number)
            continue;
        if (i == 0)
            is_leading = current_party_players[i].login_number == me->login_number;
        wchar_t* player_name = GW::Agents::GetPlayerNameByLoginNumber(current_party_players[i].login_number);
        if (!player_name)
            continue;
		current_party_names.push_back(std::wstring(player_name));
	}
	// If previous party list is empty (i.e. map change), then initialise
	if (!previous_party_names.size()) {
		previous_party_names = current_party_names;
		was_leading = is_leading;
	}
	if (current_party_names.size() > previous_party_names.size()) {
		// Someone joined my party
		for (size_t i = 0; i < current_party_names.size() && notify_when_party_member_joins; i++) {
			bool found = false;
			for (size_t j = 0; j < previous_party_names.size() && !found; j++) {
				found = previous_party_names[j] == current_party_names[i];
			}
			if (!found) {
				wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> joined the party.", current_party_names[i].c_str());
				GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
			}
		}
	} else if (!was_leading && is_leading && current_party_names.size() == 1 && previous_party_names.size() > 2) {
		// We left a party of at least 2 other people
	} else if (current_party_names.size() < previous_party_names.size()) {
		// Someone left my party
		for (size_t i = 0; i < previous_party_names.size() && notify_when_party_member_leaves; i++) {
			bool found = false;
			for (size_t j = 0; j < current_party_names.size() && !found; j++) {
				found = previous_party_names[i] == current_party_names[j];
			}
			if (!found) {
				wchar_t buffer[128];
				swprintf(buffer, 128, L"<a=1>%ls</a> left the party.", previous_party_names[i].c_str());
				GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
			}
		}
	} 
	was_leading = is_leading;
	previous_party_names = current_party_names;
	check_message_on_party_change = false;
}
void GameSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
#ifdef ENABLE_BORDERLESS
	borderlesswindow = ini->GetBoolValue(Name(), VAR_NAME(borderlesswindow), false);
#endif
	maintain_fov = ini->GetBoolValue(Name(), VAR_NAME(maintain_fov), false);
	fov = (float)ini->GetDoubleValue(Name(), VAR_NAME(fov), 1.308997f);
	tick_is_toggle = ini->GetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);
    show_timestamps = ini->GetBoolValue(Name(), VAR_NAME(show_timestamps), show_timestamps);
    show_timestamp_24h = ini->GetBoolValue(Name(), VAR_NAME(show_timestamp_24h), show_timestamp_24h);
    show_timestamp_seconds = ini->GetBoolValue(Name(), VAR_NAME(show_timestamp_seconds), show_timestamp_seconds);
	timestamps_color = Colors::Load(ini, Name(), VAR_NAME(timestamps_color), Colors::RGB(0xc0, 0xc0, 0xbf));

	shorthand_item_ping = ini->GetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);
	openlinks = ini->GetBoolValue(Name(), VAR_NAME(openlinks), true);
	auto_url = ini->GetBoolValue(Name(), VAR_NAME(auto_url), true);
	move_item_on_ctrl_click = ini->GetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
	move_item_to_current_storage_pane = ini->GetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);

	flash_window_on_pm = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
	flash_window_on_party_invite = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
	flash_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
    flash_window_on_cinematic = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_cinematic), flash_window_on_cinematic);
	focus_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    flash_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    focus_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    flash_window_on_name_ping = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);

	auto_set_away = ini->GetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
	auto_set_away_delay = ini->GetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
	auto_set_online = ini->GetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);
    auto_return_on_defeat = ini->GetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);

	show_unlearned_skill = ini->GetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
	auto_skip_cinematic = ini->GetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);

	npc_speech_bubbles_as_chat = ini->GetBoolValue(Name(), VAR_NAME(npc_speech_bubbles_as_chat), npc_speech_bubbles_as_chat);
    redirect_npc_messages_to_emote_chat = ini->GetBoolValue(Name(), VAR_NAME(redirect_npc_messages_to_emote_chat), redirect_npc_messages_to_emote_chat);

	faction_warn_percent = ini->GetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
	faction_warn_percent_amount = ini->GetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);


	disable_gold_selling_confirmation = ini->GetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);
	notify_when_friends_online = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    notify_when_friends_offline = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    notify_when_friends_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    notify_when_friends_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

	notify_when_party_member_leaves = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
	notify_when_party_member_joins = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_joins);
    notify_when_players_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    notify_when_players_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    add_special_npcs_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(add_special_npcs_to_party_window), add_special_npcs_to_party_window);

	::LoadChannelColor(ini, Name(), "local", GW::Chat::CHANNEL_ALL);
	::LoadChannelColor(ini, Name(), "guild", GW::Chat::CHANNEL_GUILD);
	::LoadChannelColor(ini, Name(), "team", GW::Chat::CHANNEL_GROUP);
	::LoadChannelColor(ini, Name(), "trade", GW::Chat::CHANNEL_TRADE);
	::LoadChannelColor(ini, Name(), "alliance", GW::Chat::CHANNEL_ALLIANCE);
	::LoadChannelColor(ini, Name(), "whispers", GW::Chat::CHANNEL_WHISPER);

#ifdef ENABLE_BORDERLESS
	if (borderlesswindow) ApplyBorderless(borderlesswindow);
#endif
	if (openlinks) GW::Chat::SetOpenLinks(openlinks);
	GW::PartyMgr::SetTickToggle(tick_is_toggle);
    GW::Chat::ToggleTimestamps(show_timestamps);
    GW::Chat::SetTimestampsColor(timestamps_color);
    GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
	// if (select_with_chat_doubleclick) GW::Chat::SetChatEventCallback(&ChatEventCallback);
	if (auto_url) GW::Chat::SetSendChatCallback(&SendChatCallback);
	if (move_item_on_ctrl_click) GW::Items::SetOnItemClick(GameSettings::ItemClickCallback);
	if (tome_patch) tome_patch->TooglePatch(show_unlearned_skill);
	if (gold_confirm_patch) gold_confirm_patch->TooglePatch(disable_gold_selling_confirmation);

	GW::Chat::SetWhisperCallback(&WhisperCallback);
}

void GameSettings::Terminate() {
	for (GW::MemoryPatcher* patch : patches) {
		if (patch) {
			patch->TooglePatch(false);
			delete patch;
		}
	}
	patches.clear();

	if (ctrl_click_patch) {
		ctrl_click_patch->TooglePatch(false);
		delete ctrl_click_patch;
	}
	if (tome_patch) {
		tome_patch->TooglePatch(false);
		delete tome_patch;
	}
	if (gold_confirm_patch) {
		gold_confirm_patch->TooglePatch(false);
		delete gold_confirm_patch;
	}
    if (OnPingEquippedItem_Func) {
        GW::HookBase::DisableHooks(OnPingEquippedItem_Func);
        GW::HookBase::RemoveHook(OnPingEquippedItem_Func);
    }
}

void GameSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
#ifdef ENABLE_BORDERLESS
	ini->SetBoolValue(Name(), VAR_NAME(borderlesswindow), borderlesswindow);
#endif
	ini->SetBoolValue(Name(), VAR_NAME(maintain_fov), maintain_fov);
	ini->SetDoubleValue(Name(), VAR_NAME(fov), fov);
	ini->SetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

	ini->SetBoolValue(Name(), VAR_NAME(show_timestamps), show_timestamps);
    ini->SetBoolValue(Name(), VAR_NAME(show_timestamp_24h), show_timestamp_24h);
    ini->SetBoolValue(Name(), VAR_NAME(show_timestamp_seconds), show_timestamp_seconds);
	Colors::Save(ini, Name(), VAR_NAME(timestamps_color), timestamps_color);

	ini->SetBoolValue(Name(), VAR_NAME(openlinks), openlinks);
	ini->SetBoolValue(Name(), VAR_NAME(auto_url), auto_url);
    ini->SetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);
	ini->SetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);
    
	ini->SetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
	ini->SetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);
	

	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
	ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);

	ini->SetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
	ini->SetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
	ini->SetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);

	ini->SetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
	ini->SetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);

	ini->SetBoolValue(Name(), VAR_NAME(npc_speech_bubbles_as_chat), npc_speech_bubbles_as_chat);
    ini->SetBoolValue(Name(), VAR_NAME(redirect_npc_messages_to_emote_chat), redirect_npc_messages_to_emote_chat);

	ini->SetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
	ini->SetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);
	ini->SetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);

	ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

	ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
	ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_joins), notify_when_party_member_joins);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    ini->SetBoolValue(Name(), VAR_NAME(add_special_npcs_to_party_window), add_special_npcs_to_party_window);

	::SaveChannelColor(ini, Name(), "local", GW::Chat::CHANNEL_ALL);
	::SaveChannelColor(ini, Name(), "guild", GW::Chat::CHANNEL_GUILD);
	::SaveChannelColor(ini, Name(), "team", GW::Chat::CHANNEL_GROUP);
	::SaveChannelColor(ini, Name(), "trade", GW::Chat::CHANNEL_TRADE);
	::SaveChannelColor(ini, Name(), "alliance", GW::Chat::CHANNEL_ALLIANCE);
	::SaveChannelColor(ini, Name(), "whispers", GW::Chat::CHANNEL_WHISPER);
}

void GameSettings::DrawSettingInternal() {
	if (ImGui::TreeNode("Chat Colors")) {
        ImGui::Text("Channel");
        ImGui::SameLine(chat_colors_grid_x[1]);
        ImGui::Text("Sender");
        ImGui::SameLine(chat_colors_grid_x[2]);
        ImGui::Text("Message");
        ImGui::Spacing();

		DrawChannelColor("Local", GW::Chat::CHANNEL_ALL);
		DrawChannelColor("Guild", GW::Chat::CHANNEL_GUILD);
		DrawChannelColor("Team", GW::Chat::CHANNEL_GROUP);
		DrawChannelColor("Trade", GW::Chat::CHANNEL_TRADE);
		DrawChannelColor("Alliance", GW::Chat::CHANNEL_ALLIANCE);
		DrawChannelColor("Whispers", GW::Chat::CHANNEL_WHISPER);

        ImGui::TextDisabled("(Left-click on a color to edit it)");
		ImGui::TreePop();
        ImGui::Spacing();
	}
#ifdef ENABLE_BORDERLESS
	DrawBorderlessSetting();
#endif
	DrawFOVSetting();

	if (ImGui::Checkbox("Show chat messages timestamp", &show_timestamps))
        GW::Chat::ToggleTimestamps(show_timestamps);
    ImGui::ShowHelp("Show timestamps in message history.");
    if (show_timestamps) {
        ImGui::Indent();
        if (ImGui::Checkbox("Use 24h", &show_timestamp_24h))
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine();
        if (ImGui::Checkbox("Show seconds", &show_timestamp_seconds))
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine(); 
        ImGui::Text("Color:");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Color:", &timestamps_color))
            GW::Chat::SetTimestampsColor(timestamps_color);
        ImGui::Unindent();
    }
	ImGui::Checkbox("Shorthand item description on weapon ping", &shorthand_item_ping);
	ImGui::ShowHelp("Include a concise description of your equipped weapon when ctrl+clicking a weapon set");
	ImGui::Checkbox("Show NPC speech bubbles in emote channel", &npc_speech_bubbles_as_chat);
	ImGui::ShowHelp("Speech bubbles from NPCs and Heroes will appear as emote messages in chat");
    ImGui::Checkbox("Redirect NPC dialog to emote channel", &redirect_npc_messages_to_emote_chat);
    ImGui::ShowHelp("Messages from NPCs that would normally show on-screen and in team chat are instead redirected to the emote channel");

	if (ImGui::Checkbox("Open web links from templates", &openlinks)) {
		GW::Chat::SetOpenLinks(openlinks);
	}
	ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

	if (ImGui::Checkbox("Automatically change urls into build templates.", &auto_url)) {
		GW::Chat::SetSendChatCallback(&SendChatCallback);
	}
	ImGui::ShowHelp("When you write a message starting with 'http://' or 'https://', it will be converted in template format");

	if (ImGui::Checkbox("Tick is a toggle", &tick_is_toggle)) {
		GW::PartyMgr::SetTickToggle(tick_is_toggle);
	}
	ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");

	if (ImGui::Checkbox("Move items from/to storage with Control+Click", &move_item_on_ctrl_click)) {
		GW::Items::SetOnItemClick(GameSettings::ItemClickCallback);
	}
	ImGui::Indent();
	ImGui::Checkbox("Move item to current open storage pane on click", &move_item_to_current_storage_pane);
	ImGui::ShowHelp("Enabled: Using Control+Click on an item in inventory with storage chest open,\n"
					"try to deposit item into the currently displayed storage pane.\n"
					"Disabled: Item will be stored into any available stack/slot in the chest.");
	ImGui::Unindent();

	ImGui::Text("Flash Guild Wars taskbar icon when:");
	ImGui::Indent();
	ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
	ImGui::Checkbox("Receiving a private message", &flash_window_on_pm);
	ImGui::Checkbox("Receiving a party invite", &flash_window_on_party_invite);
	ImGui::Checkbox("Zoning in a new map", &flash_window_on_zoning);
    ImGui::Checkbox("Cinematic start/end", &flash_window_on_cinematic);
    ImGui::Checkbox("A player starts trade with you###flash_window_on_trade", &flash_window_on_trade);
    ImGui::Checkbox("A party member pings your name", &flash_window_on_name_ping);
	ImGui::Unindent();

    ImGui::Text("Show Guild Wars in foreground when:");
    ImGui::ShowHelp("When enabled, GWToolbox++ can automatically restore\n"
        "the window from a minimized state when important events\n"
        "occur, such as entering instances.");
    ImGui::Indent();
    ImGui::Checkbox("Zoning in a new map###focus_window_on_zoning", &focus_window_on_zoning);
    ImGui::Checkbox("A player starts trade with you###focus_window_on_trade", &focus_window_on_trade);
    ImGui::Unindent();

    ImGui::Text("Show a message when a friend:");
	ImGui::Indent();
	ImGui::Checkbox("Logs in", &notify_when_friends_online);
    ImGui::SameLine(256.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::Checkbox("Joins your outpost###notify_when_friends_join_outpost", &notify_when_friends_join_outpost);
    ImGui::Checkbox("Logs out", &notify_when_friends_offline);
    ImGui::SameLine(256.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::Checkbox("Leaves your outpost###notify_when_friends_leave_outpost", &notify_when_friends_leave_outpost);
    ImGui::Unindent();

    ImGui::Text("Show a message when a player:");
    ImGui::Indent();
    ImGui::Checkbox("Joins your party", &notify_when_party_member_joins);
    ImGui::SameLine(256.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::Checkbox("Joins your outpost###notify_when_players_join_outpost", &notify_when_players_join_outpost);
    ImGui::Checkbox("Leaves your party", &notify_when_party_member_leaves);
    ImGui::SameLine(256.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::Checkbox("Leaves your outpost###notify_when_players_leave_outpost", &notify_when_players_leave_outpost);
    ImGui::Unindent();

	ImGui::Checkbox("Automatically set 'Away' after ", &auto_set_away);
	ImGui::SameLine();
	ImGui::PushItemWidth(50);
	ImGui::InputInt("##awaydelay", &auto_set_away_delay, 0);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Text("minutes of inactivity");
	ImGui::ShowHelp("Only if you were 'Online'");

	ImGui::Checkbox("Automatically set 'Online' after an input to Guild Wars", &auto_set_online);
	ImGui::ShowHelp("Only if you were 'Away'");

	if (ImGui::Checkbox("Only show non learned skills when using a tome", &show_unlearned_skill)) {
		if (tome_patch) {
			tome_patch->TooglePatch(show_unlearned_skill);
		}
	}

	ImGui::Checkbox("Automatically skip cinematics", &auto_skip_cinematic);
    ImGui::Checkbox("Automatically return to outpost on defeat", &auto_return_on_defeat);
    ImGui::ShowHelp("Automatically return party to outpost on party wipe if player is leading");
	ImGui::Checkbox("Show warning when earned faction reaches ", &faction_warn_percent);
	ImGui::SameLine();
	ImGui::PushItemWidth(40.0f * ImGui::GetIO().FontGlobalScale);
	ImGui::InputInt("##faction_warn_percent_amount", &faction_warn_percent_amount, 0);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Text("%%");
	ImGui::ShowHelp("Displays when in a challenge mission or elite mission outpost");

	if (ImGui::Checkbox("Disable Gold/Green items confirmation", &disable_gold_selling_confirmation)) {
		if (gold_confirm_patch) {
			gold_confirm_patch->TooglePatch(disable_gold_selling_confirmation);
		}
	}
	ImGui::ShowHelp(
		"Disable the confirmation request when\n"
		"selling Gold and Green items introduced\n"
		"in February 5, 2019 update.");
    ImGui::Checkbox("Add special NPCs to party window", &add_special_npcs_to_party_window);
    ImGui::ShowHelp("Adds the following NPCs to party window when available:\n"
    "Ebon Vanguard Assassin\n"
    "Polymock summon e.g. Ice Imp, Naga Shaman etc\n"
    "Fissure of Woe: Forest Griffons\n"
    "Fissure of Woe: Eternal Forgemaster\n");
}

void GameSettings::DrawBorderlessSetting() {
	if (ImGui::Checkbox("Borderless Window", &borderlesswindow)) {
		ApplyBorderless(borderlesswindow);
	}
}

void GameSettings::ApplyBorderless(bool borderless) {
	if (borderless) {
		borderless_status = WantBorderless;
	} else {
		borderless_status = WantWindowed;
	}
}

void GameSettings::FactionEarnedCheckAndWarn() {
	if (!faction_warn_percent)
		return; // Disabled
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
		faction_checked = false;
		return; // Loading or explorable area.
	}
	if (faction_checked)
		return; // Already checked.
	faction_checked = true;
	GW::WorldContext * world_context = GW::GameContext::instance()->world;
	if (!world_context || !world_context->max_luxon || !world_context->total_earned_kurzick) {
		faction_checked = false;
		return; // No world context yet.
	}
	float percent;
	// Avoid invalid user input values.
	if (faction_warn_percent_amount < 0)
		faction_warn_percent_amount = 0; 
	if (faction_warn_percent_amount > 100)
		faction_warn_percent_amount = 100;
	// Warn user to dump faction if we're in a luxon/kurzick mission outpost
	switch (GW::Map::GetMapID()) {
		case GW::Constants::MapID::The_Deep:
		case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
		case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
		case GW::Constants::MapID::Zos_Shivros_Channel:
		case GW::Constants::MapID::The_Aurios_Mines:
			// Player is in luxon mission outpost
			percent = 100.0f * (float)world_context->current_luxon / (float)world_context->max_luxon;
			if (percent >= (float)faction_warn_percent_amount) {
				// Faction earned is over 75% capacity
				Log::Warning("Luxon faction earned is %d of %d", world_context->current_luxon, world_context->max_luxon );
			}
			else if (world_context->current_kurzick > 4999 && world_context->current_kurzick > world_context->current_luxon) {
				// Kurzick faction > Luxon faction
				Log::Warning("Kurzick faction earned is greater than Luxon");
			}
			break;
		case GW::Constants::MapID::Urgozs_Warren:
		case GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost:
		case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
		case GW::Constants::MapID::Altrumm_Ruins:
		case GW::Constants::MapID::Amatz_Basin:
			// Player is in kurzick mission outpost
			percent = 100.0f * (float)world_context->current_kurzick / (float)world_context->max_kurzick;
			if (percent >= (float)faction_warn_percent_amount) {
				// Faction earned is over 75% capacity
				Log::Warning("Kurzick faction earned is %d of %d", world_context->current_kurzick, world_context->max_kurzick);
			}
			else if (world_context->current_luxon > 4999 && world_context->current_luxon > world_context->current_kurzick) {
				// Luxon faction > Kurzick faction
				Log::Warning("Luxon faction earned is greater than Kurzick");
			}
			break;
	}
}
void GameSettings::SetAfkMessage(std::wstring&& message) {
	
	static size_t MAX_AFK_MSG_LEN = 80;
	if (message.size() <= MAX_AFK_MSG_LEN) {
		afk_message = message;
		afk_message_time = clock();
		Log::Info("Afk message set to \"%S\"", afk_message.c_str());
	} else {
		Log::Error("Afk message must be under 80 characters. (Yours is %zu)", message.size());
	}
}

void GameSettings::Update(float delta) {
    // Add any pending agents to party window
    if (allies_to_add.size()) {
        for (size_t i = 0; i < allies_to_add.size(); i++) {
            PendingAddToParty p = allies_to_add[i];
            GW::GameThread::Enqueue([p]() {
                GW::Packet::StoC::PartyAllyAdd packet;
                packet.header = GW::Packet::StoC::PartyAllyAdd::STATIC_HEADER;
                packet.agent_id = p.agent_id;
                packet.agent_type = p.agent_type;
                packet.allegiance_bits = p.allegiance_bits;// 1886151033;
                GW::StoC::EmulatePacket( & packet);
                });
            allies_added_to_party.push_back(p.agent_id);
        }
        allies_to_add.clear();
    }
    if (allies_to_remove.size()) {
        for (size_t i = 0; i < allies_to_remove.size(); i++) {
            uint32_t agent_id = allies_to_remove[i];
            GW::GameThread::Enqueue([agent_id]() {
                GW::Packet::StoC::PartyRemoveAlly packet;
                packet.header = GW::Packet::StoC::PartyRemoveAlly::STATIC_HEADER;
                packet.agent_id = agent_id;
                GW::StoC::EmulatePacket(&packet);
                });
            // Remove from list of manually added party members if applicable.
            for (size_t i = 0; i < allies_added_to_party.size(); i++) {
                if (allies_added_to_party[i] != agent_id)
                    continue;
                allies_added_to_party.erase(allies_added_to_party.begin() + i);
                break;
            }
        }
        allies_to_remove.clear();
    }
    // Try to print any pending messages.
    for (std::vector<PendingChatMessage*>::iterator it = pending_messages.begin(); it != pending_messages.end(); ++it) {
        PendingChatMessage *m = *it;
        if (m->Consume()) {
            it = pending_messages.erase(it);
            delete m;
            if (it == pending_messages.end()) break;
        }
    }
	if (auto_set_away
		&& TIMER_DIFF(activity_timer) > auto_set_away_delay * 60000
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::ONLINE) {
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
		activity_timer = TIMER_INIT(); // refresh the timer to avoid spamming in case the set status call fails
	}
	UpdateFOV();
	FactionEarnedCheckAndWarn();
#ifdef ENABLE_BORDERLESS
	UpdateBorderless();
#endif

#ifdef APRIL_FOOLS
	AF::ApplyPatchesIfItsTime();
#endif
	if (notify_when_party_member_joins || notify_when_party_member_leaves) {
		if (!check_message_on_party_change && !GW::Map::GetIsMapLoaded()) {
			// Map changed - clear previous party names, flag to re-populate on map load
			previous_party_names.clear();
			check_message_on_party_change = true;
		}
		if (check_message_on_party_change)
			GameSettings::MessageOnPartyChange();
	}
}

void GameSettings::DrawFOVSetting() {
	ImGui::Checkbox("Maintain FOV", &maintain_fov);
	ImGui::ShowHelp("GWToolbox will save and maintain the FOV setting used with /cam fov <value>");
}

void GameSettings::UpdateFOV() {
	if (maintain_fov && GW::CameraMgr::GetFieldOfView() != fov) {
		GW::CameraMgr::SetFieldOfView(fov);
	}
}

void GameSettings::UpdateBorderless() {

	// ==== Check if we have something to do ====
	bool borderless = false;
	switch (borderless_status) {
	case GameSettings::Ok:										return; // nothing to do
	case GameSettings::WantBorderless:	borderless = true;		break;
	case GameSettings::WantWindowed:	borderless = false;		break;
	}

	// ==== Check fullscreen status allows it ====
	switch (GW::Render::GetIsFullscreen()) {
	case -1:
		// Unknown status. Probably has been minimized since toolbox launch. Wait.
		// leave borderless_status as-is
		return;

	case 0:
		// windowed or borderless windowed
		break; // proceed

	case 1:
		// fullscreen
		Log::Info("Please enable Borderless while in Windowed mode");
		borderless_status = Ok;
		return;

	default:
		return; // should never happen
	}

	// ==== Either borderless or windowed, find out which and warn the user if it's a useless operation ====
	RECT windowRect, desktopRect;
	if (GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), &windowRect)
		&& GetWindowRect(GetDesktopWindow(), &desktopRect)) {

		if (windowRect.left == desktopRect.left
			&& windowRect.right == desktopRect.right
			&& windowRect.top == desktopRect.top
			&& windowRect.bottom == desktopRect.bottom) {

			//borderless
			if (borderless_status == WantBorderless) {
				//trying to activate borderless while already in borderless
				Log::Info("Already in Borderless mode");
			}
		} else if (windowRect.left >= desktopRect.left
			&& windowRect.right <= desktopRect.right
			&& windowRect.top >= desktopRect.top
			&& windowRect.bottom <= desktopRect.bottom) {

			// windowed
			if (borderless_status == WantWindowed) {
				//trying to deactivate borderless in window mode
				Log::Info("Already in Window mode");
			}
		}
	}

	DWORD current_style = GetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE);
	DWORD new_style = borderless ? WS_POPUP | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPSIBLINGS
		: WS_SIZEBOX | WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPSIBLINGS;

	//printf("old 0x%X, new 0x%X\n", current_style, new_style);
	//printf("popup: old=%X, new=%X\n", current_style & WS_POPUP, new_style & WS_POPUP);

	for (GW::MemoryPatcher* patch : patches) patch->TooglePatch(borderless);

	SetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE, new_style);

	if (borderless) {
		int width = GetSystemMetrics(SM_CXSCREEN);
		int height = GetSystemMetrics(SM_CYSCREEN);
		MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), 0, 0, width, height, TRUE);
	} else {
		RECT size;
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &size, 0);
		MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), size.top, size.left, size.right, size.bottom, TRUE);
	}

	borderlesswindow = borderless;
	borderless_status = Ok;
}

bool GameSettings::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	// I don't know what would be the best solution here, but the way we capture every messages as a sign of activity can be bad.
	// Added that because when someone was typing "/afk message" he was put online directly, because "enter-up" was captured.
	if (Message == WM_KEYUP)
		return false;

	activity_timer = TIMER_INIT();
	static clock_t set_online_timer = TIMER_INIT();
	if (auto_set_online
		&& TIMER_DIFF(set_online_timer) > 5000 // to avoid spamming in case of failure
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::AWAY) {
		printf("%X\n", Message);
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::ONLINE);
		set_online_timer = TIMER_INIT();
	}

	return false;
}

void GameSettings::ItemClickCallback(uint32_t type, uint32_t slot, GW::Bag *bag) {
	if (!GameSettings::Instance().move_item_on_ctrl_click) return;
    if (!ImGui::IsKeyDown(VK_CONTROL)) return;
	if (type != 7) return;

	// Expected behaviors
	//  When clicking on item in inventory
	//   case storage close (or move_item_to_current_storage_pane = false):
	//    - If the item is a material, it look if it can move it to the material page.
	//    - If the item is stackable, search in all the storage if there is already similar items and completes the stack
	//    - If not everything was moved, move the remaining in the first empty slot of the storage.
	//   case storage open:
	//    - If the item is a material, it look if it can move it to the material page.
	//    - If the item is stackable, search for incomplete stacks in the current storage page and completes them
	//    - If not everything was moved, move the remaining in the first empty slot of the current page.
	//  When clicking on item in chest
	//   - If the item is stackable, search for incomplete stacks in the inventory and completes them.
	//   - If nothing was moved, move the stack in the first empty slot of the inventory.

	// @Fix:
	//  There is a bug in gw that doesn't "save" if the material storage
	//  (or anniversary storage in the case when the player bought all other storage)
	//  so we cannot know if they are the storage selected.
	//  Sol: The solution is to patch the value 7 -> 9 at 0040E851 (EB 20 33 C0 BE 06 [-5])

	bool is_inventory_item = bag->IsInventoryBag();
	bool is_storage_item = bag->IsStorageBag();
	if (!is_inventory_item && !is_storage_item) return;

	GW::Item *item = GW::Items::GetItemBySlot(bag, slot + 1);
	if (!item) return;

	// @Cleanup: Bad
	if (item->model_file_id == 0x0002f301) {
		Log::Error("Ctrl+click doesn't work with birthday presents yet");
		return;
	}

	if (is_inventory_item) {
		if (GW::Items::GetIsStorageOpen() && GameSettings::Instance().move_item_to_current_storage_pane) {
			// If move_item_to_current_storage_pane = true, try to add the item to current storage pane.
			int current_storage = GW::Items::GetStoragePage();
			move_item_to_storage_page(item, current_storage);
		} else {
			// Otherwise, just try to put it in anywhere.
			move_item_to_storage(item);
		}
	} else {
		move_item_to_inventory(item);
	}
}

void GameSettings::FriendStatusCallback(GW::Friend* f, GW::FriendStatus status, const wchar_t *account, const wchar_t *charname) {
	if (!f || !charname || *charname == L'\0')
		return;

	GameSettings& game_setting = GameSettings::Instance();
	if (status == f->status)
		return;
	char buffer[512];
	switch (status) {
	case GW::FriendStatus_Offline:
        if (game_setting.notify_when_friends_offline) {
		    snprintf(buffer, sizeof(buffer), "%S (%S) has just logged out.", charname, account);
		    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
		return;
	case GW::FriendStatus_Away:
	case GW::FriendStatus_DND:
	case GW::FriendStatus_Online:
		if (f->status != GW::FriendStatus_Offline)
            return;
        if (game_setting.notify_when_friends_online) {
		    snprintf(buffer, sizeof(buffer), "<a=1>%S</a> (%S) has just logged in.</c>", charname, account);
		    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
		return;
	}
}

void GameSettings::DrawChannelColor(const char *name, GW::Chat::Channel chan) {
    ImGui::PushID(chan);
	ImGui::Text(name);
	
	GW::Chat::Color color, sender_col, message_col;
	GW::Chat::GetChannelColors(chan, &sender_col, &message_col);

	ImGui::SameLine(chat_colors_grid_x[1]);
    color = sender_col;
    if (Colors::DrawSettingHueWheel("Sender Color:", &color) && color != sender_col) {
        GW::Chat::SetSenderColor(chan, color);
    }

	ImGui::SameLine(chat_colors_grid_x[2]);
    color = message_col;
    if (Colors::DrawSettingHueWheel("Message Color:", &color) && color != message_col) {
        GW::Chat::SetMessageColor(chan, color);
    }

	ImGui::SameLine(chat_colors_grid_x[3]);
	if (ImGui::Button("Reset")) {
		GW::Chat::Color col1, col2;
		GW::Chat::GetDefaultColors(chan, &col1, &col2);
		GW::Chat::SetSenderColor(chan, col1);
		GW::Chat::SetMessageColor(chan, col2);
	}
    ImGui::PopID();
}

