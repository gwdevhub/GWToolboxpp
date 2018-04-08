#include "GameSettings.h"

#include <ctime>

#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\FriendListMgr.h>
#include <GWCA\Managers\Render.h>
#include <GWCA\Managers\CameraMgr.h>

#include <GWCA\Context\GameContext.h>

#include <logger.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Timer.h>
#include <Color.h>

namespace {
	void ChatEventCallback(DWORD id, DWORD type, wchar_t* info, void* unk) {
		if (type == 0x29 && GameSettings::Instance().select_with_chat_doubleclick) {
			static wchar_t last_name[64] = L"";
			static clock_t timer = TIMER_INIT();
			if (TIMER_DIFF(timer) < 500 && wcscmp(last_name, info) == 0) {
				GW::PlayerArray players = GW::Agents::GetPlayerArray();
				if (players.valid()) {
					for (unsigned i = 0; i < players.size(); ++i) {
						GW::Player& player = players[i];
						wchar_t* name = player.Name;
						if (player.AgentID > 0
							&& name != nullptr
							&& wcscmp(info, name) == 0) {
							GW::Agents::ChangeTarget(players[i].AgentID);
						}
					}
				}
			} else {
				timer = TIMER_INIT();
				wcscpy_s(last_name, info);
			}
		}
	}

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

	void WhisperCallback(const wchar_t from[20], const wchar_t msg[140]) {
		if (GameSettings::Instance().flash_window_on_pm) FlashWindow();
	}

	void move_item_to_storage_page(GW::Item *item, int page) {
		assert(item && item->Quantity);
		// 9 being the material storage
		if (page < 0 || 9 < page) return;

		if (page == 9) {
			if (!item->GetIsMaterial()) return;
			int slot = GW::Items::GetMaterialSlot(item);
			if (slot < 0 || (int)GW::Constants::N_MATS <= slot) return;
			int availaible = 250;
			GW::Item *b_item = GW::Items::GetItemBySlot(GW::Constants::Bag::Material_Storage, slot + 1);
			if (b_item) availaible = 250 - b_item->Quantity;
			GW::Items::MoveItem(item, GW::Constants::Bag::Material_Storage, slot, availaible);
			return;
		}

		assert(0 <= page && page < 9);
		const int storage1 = (int)GW::Constants::Bag::Storage_1;
		GW::Bag *bag = GW::Items::GetBag(storage1 + page);
		if (!bag) return;

		int remaining = item->Quantity;
		// if the item is stackable we try to complete stack that already exist in the current storage page
		if (item->GetIsStackable()) {
			size_t slot = bag->find(item->ModelId);
			while (slot != GW::Bag::npos) {
				GW::Item *b_item = bag->Items[slot];
				assert(b_item && (b_item->ModelId == item->ModelId));
				int availaible = 250 - b_item->Quantity;
				int will_move = std::min(availaible, remaining);
				if (will_move > 0) {
					GW::Items::MoveItem(item, b_item, will_move);
					remaining -= will_move;
				}
				if (remaining == 0) break;
				slot = bag->find(item->ModelId, slot + 1);
			}
		}

		// if there is still item, we find the first empty slot and move everything there
		if (remaining == 0) return;
		size_t slot = bag->find(0);
		if (slot != GW::Bag::npos) {
			GW::Items::MoveItem(item, bag, slot);
		}
	}

	void move_item_to_storage(GW::Item *item) {
		assert(item && item->Quantity);

		GW::Bag **bags = GW::Items::GetBagArray();
		if (!bags) return;
		int remaining = item->Quantity;

		// We try to move to the material storage
		if (item->GetIsMaterial()) {
			int slot = GW::Items::GetMaterialSlot(item);
			assert(0 <= slot && slot <= GW::Constants::N_MATS);
			GW::Bag *mats = bags[(DWORD)GW::Constants::Bag::Material_Storage];
			if (mats != NULL) {
				// Could happen if the player didn't bought the material storage
				GW::Item *b_item = GW::Items::GetItemBySlot(mats, slot + 1);
				int availaible = 250;
				if (b_item) availaible = 250 - b_item->Quantity;
				int will_move = std::min(remaining, availaible);
				// bag_item can be NULL if the item quantity is 0, so we cannot move "from item to item"
				if (will_move != 0) {
					GW::Items::MoveItem(item, mats, slot, will_move);
					remaining -= will_move;
				}
			}
		}

		const int storage1 = (int)GW::Constants::Bag::Storage_1;
		const int storage9 = (int)GW::Constants::Bag::Storage_9;

		// We try to complete stacks in any storage page
		if (remaining == 0) return;
		const int model_id = item->ModelId;
		for (int i = storage1; i < storage9; i++) {
			GW::Bag *bag = bags[i];
			if (!bag) continue;
			size_t slot = bag->find(model_id);
			while (slot != GW::Bag::npos) {
				GW::Item *b_item = bag->Items[slot];
				assert(b_item);
				int availaible = 250 - b_item->Quantity;
				int will_move = std::min(availaible, remaining);
				if (will_move != 0) {
					GW::Items::MoveItem(item, b_item, will_move);
					remaining -= will_move;
				}
				if (remaining == 0) break;
				slot = bag->find(model_id, slot + 1);
			}
		}

		// We find the first empty slot and put the remaining there
		if (remaining == 0) return;
		for (int i = storage1; i < storage9; i++) {
			GW::Bag *bag = bags[i];
			if (!bag) continue;
			size_t slot = bag->find(0);
			if (slot != GW::Bag::npos) {
				GW::Items::MoveItem(item, bag, slot, remaining);
				return;
			}
		}
	}

	void move_item_to_inventory(GW::Item *item) {
		assert(item && item->Quantity);

		const int backpack = (int)GW::Constants::Bag::Backpack;
		const int bag2 = (int)GW::Constants::Bag::Bag_2;

		GW::Bag **bags = GW::Items::GetBagArray();
		if (!bags) return;

		int total = item->Quantity;
		int remaining = total;

		// If item is stackable, try to fill every stack of the same type
		if (item->GetIsStackable()) {
			for (int i = backpack; i <= bag2; i++) {
				GW::Bag *bag = bags[i];
				if (!bag) continue;
				size_t slot = bag->find(item->ModelId);
				while (slot != GW::Bag::npos) {
					GW::Item *b_item = bag->Items[slot];
					assert(b_item);
					int availaible = 250 - b_item->Quantity;
					int will_move = std::min(availaible, remaining);
					if (will_move != 0) {
						GW::Items::MoveItem(item, b_item, will_move);
						remaining -= will_move;
					}
					if (remaining == 0) break;
					slot = bag->find(item->ModelId, slot + 1);
				}
			}
		}

		// If we didn't move any item (i.e. there was no stack to complete), move the stack to first empty slot
		if (remaining == total) {
			for (int i = backpack; i <= bag2; i++) {
				GW::Bag *bag = bags[i];
				if (!bag) continue;
				size_t slot = bag->find(0);
				if (slot != GW::Bag::npos) {
					GW::Items::MoveItem(item, bag, slot);	
					return;
				}
			}
		}
	}

	// April's Fool
	namespace AF {
		void CreateXunlaiAgentFromGameThread(void) {
			{
				GW::Packet::StoC::NpcGeneralStats packet;
				packet.header = 79;
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
				GW::Packet::StoC::P080 packet;
				packet.header = 80;
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

				GW::Packet::StoC::P167 packet;
				packet.header = 167;
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
			GW::StoC::AddCallback<GW::Packet::StoC::P065>(
				[](GW::Packet::StoC::P065 *packet) -> bool {
				DWORD agent_id = packet->agent_id;
				GW::Agent *agent = GW::Agents::GetAgentByID(agent_id);
				ApplySkinSafe(agent, 221);
				return false;
			});

			// override tonic usage
			GW::StoC::AddCallback<GW::Packet::StoC::P167>(
				[](GW::Packet::StoC::P167 *packet) -> bool {
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
}

void GameSettings::Initialize() {
	ToolboxModule::Initialize();
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D9D8,
		(BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 16));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D530, (BYTE*)"\xEB", 1));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D54D, (BYTE*)"\xEB", 1));

	BYTE* a = (BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
	patches.push_back(new GW::MemoryPatcher((void*)0x00669A56, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x00669AA2, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x00669ADE, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D7E6, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D832, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D86E, a, 10));

	{
		// Patch that allow storage page (and Anniversary page) to work... (ask Ziox for more info)
		uintptr_t found = GW::Scanner::Find("\xEB\x20\x33\xC0\xBE\x06", "xxxxxx", -4);
		printf("[SCAN] StoragePatch = %p\n", (void *)found);
		ctrl_click_patch = new GW::MemoryPatcher((void*)found, (BYTE *)"\x09", 1);
		ctrl_click_patch->TooglePatch(true);
	}

	GW::Chat::CreateCommand(L"borderless",
		[&](int argc, LPWSTR *argv) {
		if (argc <= 1) {
			ApplyBorderless(!borderlesswindow);
		} else {
			std::wstring arg1 = GuiUtils::ToLower(argv[1]);
			if (arg1 == L"on") {
				ApplyBorderless(true);
			} else if (arg1 == L"off") {
				ApplyBorderless(false);
			} else {
				Log::Error("Invalid argument '%ls', please use /borderless [|on|off]", argv[1]);
			}
		}
	});

	GW::StoC::AddCallback<GW::Packet::StoC::PartyPlayerAdd>(
		[](GW::Packet::StoC::PartyPlayerAdd*) -> bool {
		if (GameSettings::Instance().flash_window_on_party_invite) FlashWindow();
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::GameSrvTransfer>(
		[](GW::Packet::StoC::GameSrvTransfer *pak) -> bool {

		GW::CharContext *ctx = GW::GameContext::instance()->character;
		if (GameSettings::Instance().flash_window_on_zoning) FlashWindow();
		if (GameSettings::Instance().focus_window_on_zoning && pak->is_explorable) {
			HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
			SetForegroundWindow(hwnd);
			ShowWindow(hwnd, SW_RESTORE);
		}

		return false;
	});

	AF::ApplyPatchesIfItsTime();
}

void GameSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	borderlesswindow = ini->GetBoolValue(Name(), VAR_NAME(borderlesswindow), false);
	maintain_fov = ini->GetBoolValue(Name(), VAR_NAME(maintain_fov), false);
	fov = (float)ini->GetDoubleValue(Name(), VAR_NAME(fov), 1.308997f);
	tick_is_toggle = ini->GetBoolValue(Name(), VAR_NAME(tick_is_toggle), true);

	GW::Chat::ShowTimestamps = ini->GetBoolValue(Name(), "show_timestamps", false);
	GW::Chat::TimestampsColor = Colors::Load(ini, Name(), "timestamps_color", Colors::White());
	GW::Chat::KeepChatHistory = ini->GetBoolValue(Name(), "keep_chat_history", true);

	openlinks = ini->GetBoolValue(Name(), VAR_NAME(openlinks), true);
	auto_url = ini->GetBoolValue(Name(), VAR_NAME(auto_url), true);
	select_with_chat_doubleclick = ini->GetBoolValue(Name(), VAR_NAME(select_with_chat_doubleclick), true);
	move_item_on_ctrl_click = ini->GetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), true);

	flash_window_on_pm = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_pm), true);
	flash_window_on_party_invite = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), true);
	flash_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), true);
	focus_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), false);

	auto_set_away = ini->GetBoolValue(Name(), VAR_NAME(auto_set_away), false);
	auto_set_away_delay = ini->GetLongValue(Name(), VAR_NAME(auto_set_away_delay), 10);
	auto_set_online = ini->GetBoolValue(Name(), VAR_NAME(auto_set_online), false);

	if (borderlesswindow) ApplyBorderless(borderlesswindow);
	if (openlinks) GW::Chat::SetOpenLinks(openlinks);
	if (tick_is_toggle) GW::PartyMgr::SetTickToggle();
	if (select_with_chat_doubleclick) GW::Chat::SetChatEventCallback(&ChatEventCallback);
	if (auto_url) GW::Chat::SetSendChatCallback(&SendChatCallback);
	if (flash_window_on_pm) GW::Chat::SetWhisperCallback(&WhisperCallback);
	if (move_item_on_ctrl_click) GW::Items::SetOnItemClick(GameSettings::ItemClickCallback);

}

void GameSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(borderlesswindow), borderlesswindow);
	ini->SetBoolValue(Name(), VAR_NAME(maintain_fov), maintain_fov);
	ini->SetDoubleValue(Name(), VAR_NAME(fov), fov);
	ini->SetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

	ini->SetBoolValue(Name(), "show_timestamps", GW::Chat::ShowTimestamps);
	Colors::Save(ini, Name(), "timestamps_color", GW::Chat::TimestampsColor);
	ini->SetBoolValue(Name(), "keep_chat_history", GW::Chat::KeepChatHistory);

	ini->SetBoolValue(Name(), VAR_NAME(openlinks), openlinks);
	ini->SetBoolValue(Name(), VAR_NAME(auto_url), auto_url);
	ini->SetBoolValue(Name(), VAR_NAME(select_with_chat_doubleclick), select_with_chat_doubleclick);
	ini->SetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);

	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
	ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);

	ini->SetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
	ini->SetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
	ini->SetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);
}

void GameSettings::DrawSettingInternal() {
	DrawBorderlessSetting();
	DrawFOVSetting();

	ImGui::Checkbox("Show chat messages timestamp. Color:", &GW::Chat::ShowTimestamps);
	ImGui::SameLine();

	ImVec4 col = ImGui::ColorConvertU32ToFloat4(GW::Chat::TimestampsColor);
	if (ImGui::ColorEdit4("Color:", &col.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel)) {
		GW::Chat::TimestampsColor = ImGui::ColorConvertFloat4ToU32(col);
	}
	ImGui::ShowHelp("Show timestamps in message history.");

	ImGui::Checkbox("Keep chat history.", &GW::Chat::KeepChatHistory);
	ImGui::ShowHelp("Messages in the chat do not disappear on character change.");

	if (ImGui::Checkbox("Open web links from templates", &openlinks)) {
		GW::Chat::SetOpenLinks(openlinks);
	}
	ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

	if (ImGui::Checkbox("Automatically change urls into build templates.", &openlinks)) {
		GW::Chat::SetSendChatCallback(&SendChatCallback);
	}
	ImGui::ShowHelp("When you write a message starting with 'http://' or 'https://', it will be converted in template format");

	if (ImGui::Checkbox("Tick is a toggle", &tick_is_toggle)) {
		if (tick_is_toggle) {
			GW::PartyMgr::SetTickToggle();
		} else {
			GW::PartyMgr::RestoreTickToggle();
		}
	}
	ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");

	if (ImGui::Checkbox("Target with double-click on message author", &select_with_chat_doubleclick)) {
		GW::Chat::SetChatEventCallback(&ChatEventCallback);
	}
	ImGui::ShowHelp("Double clicking on the author of a message in chat will target the author");

	if (ImGui::Checkbox("Move items from/to storage with Control+Click", &move_item_on_ctrl_click)) {
		GW::Items::SetOnItemClick(GameSettings::ItemClickCallback);
	}

	ImGui::Text("Flash Guild Wars taskbar icon when:");
	ImGui::Indent();
	ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
	if (ImGui::Checkbox("Receiving a private message", &flash_window_on_pm)) {
		GW::Chat::SetWhisperCallback(&WhisperCallback);
	}
	ImGui::Checkbox("Receiving a party invite", &flash_window_on_party_invite);
	ImGui::Checkbox("Zoning in a new map", &flash_window_on_zoning);
	ImGui::Unindent();

	ImGui::Checkbox("Allow window restore", &focus_window_on_zoning);
	ImGui::ShowHelp("When enabled, GWToolbox++ can automatically restore\n"
		"the window from a minimized state when important events\n"
		"occur, such as entering instances.");

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

void GameSettings::Update(float delta) {
	if (auto_set_away
		&& TIMER_DIFF(activity_timer) > auto_set_away_delay * 60000
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::ONLINE) {
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
		activity_timer = TIMER_INIT(); // refresh the timer to avoid spamming in case the set status call fails
	}
	UpdateFOV();
	UpdateBorderless();
	AF::ApplyPatchesIfItsTime();
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

	activity_timer = TIMER_INIT();
	static clock_t set_online_timer = TIMER_INIT();
	if (auto_set_online
		&& TIMER_DIFF(set_online_timer) > 5000 // to avoid spamming in case of failure
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::AWAY) {
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::ONLINE);
		set_online_timer = TIMER_INIT();
	}

	return false;
}

void GameSettings::ItemClickCallback(uint32_t type, uint32_t slot, GW::Bag *bag) {
	if (!GameSettings::Instance().move_item_on_ctrl_click) return;
	// ImGui::IsKeyDown doesn't work (probably unitialize value)
	if (!(GetKeyState(VK_CONTROL) & 0x8000)) return;
	if (type != 7) return;

	// Expected behaviors
	//  When clicking on item in inventory
	//   case storage close:
	//    - If the item is stackable, search in all the storage (including mats page) if there is already
	//      similar items and completes the stack. In mats page we check at the "right" slot.
	//    - If not everything was moved, move the remaining in the first empty slot of the storage.
	//   case storage open:
	//    - If the item is stackable, search for incomplete stacks in the current storage page and completes them
	//    - If not everything was moved, move the remaining in the first empty slot of the current page.
	// When clicking on item in chest
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

	if (is_inventory_item) {
		if (GW::Items::IsStorageOpen()) {
			int current_storage = GW::Items::GetStoragePage();
			move_item_to_storage_page(item, current_storage);
		} else {
			move_item_to_storage(item);
		}
	} else {
		move_item_to_inventory(item);
	}
}
