#include "stdafx.h"
#include "FriendListWindow.h"

#include <algorithm>

#include <GWCA/Packets/StoC.h>

#include <GWCA\Constants\Constants.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/Resources.h>

#include <GuiUtils.h>

#include "logger.h"
#include "base64.h"

/* Out of scope namespecey lookups */
namespace {
	enum ProfColors {
		None = IM_COL32_WHITE,
		Warrior = IM_COL32(0xb4, 0x82, 0x46, 255),
		Ranger = IM_COL32(0x91, 0xc0, 0x4c, 255),
		Monk = IM_COL32(0x7a, 0xbb, 0xd1, 255),
		Necromancer = IM_COL32(0x4c, 0xA2, 0x64, 255),
		Mesmer = IM_COL32(0x71, 0x35, 0x66, 255), 
		Elementalist = IM_COL32(0xc4, 0x4b, 0x4b, 255), 
		Assassin = IM_COL32(0xe1, 0x18, 0x7c, 255), 
		Ritualist = IM_COL32(0x1e, 0xd6, 0xba, 255), 
		Paragon = IM_COL32(0xea, 0xde, 0x00, 255), 
		Dervish = IM_COL32(0x57, 0x68, 0x95, 255)
	};
	wchar_t* ProfNames[10] = {
		L"Unknown",
		L"Warrior",
		L"Ranger"
		L"Monk",
		L"Necromancer",
		L"Mesmer",
		L"Elementalist",
		L"Assassin",
		L"Ritualist",
		L"Paragon",
		L"Dervish"
	};
	ImColor StatusColors[4] = {
		IM_COL32(0x99,0x99,0x99,0), // offline
		IM_COL32(0x0,0xc8,0x0,255),  // online
		IM_COL32(0xc8,0x0,0x0,255), // busy
		IM_COL32(0xc8,0xc8,0x0,255)  // away
	};
	ImColor GetProfessionColor(uint8_t prof = 0) {
		switch (static_cast<GW::Constants::Profession>(prof)) {
		case GW::Constants::Profession::Warrior:		return ProfColors::Warrior;
		case GW::Constants::Profession::Ranger:			return ProfColors::Ranger;
		case GW::Constants::Profession::Monk:			return ProfColors::Monk;
		case GW::Constants::Profession::Necromancer:	return ProfColors::Necromancer;
		case GW::Constants::Profession::Mesmer:			return ProfColors::Mesmer;
		case GW::Constants::Profession::Elementalist:	return ProfColors::Elementalist;
		case GW::Constants::Profession::Assassin:		return ProfColors::Assassin;
		case GW::Constants::Profession::Ritualist:		return ProfColors::Ritualist;
		case GW::Constants::Profession::Paragon:		return ProfColors::Paragon;
		case GW::Constants::Profession::Dervish:		return ProfColors::Dervish;
		}
		return ProfColors::None;
	}
	char* GetStatusText(uint8_t status) {
		switch (status) {
		case 0: return "Offline";
		case 1: return "Online";
		case 2: return "Do not disturb";
		case 3: return "Away";
		}
		return "Unknown";
	}
	std::map<uint32_t, wchar_t*> map_names;
}
/*	FriendListWindow::Friend	*/

void FriendListWindow::Friend::GetMapName() {
	GW::GameThread::Enqueue([this]() {
		GW::AreaInfo* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(current_map_id));
		if (!info) {
			current_map_name[0] = 0;
			return;
		}
		static wchar_t enc_str[16];
		if (!GW::UI::UInt32ToEncStr(info->name_id, enc_str, 16)) {
			current_map_name[0] = 0;
			return;
		}
		GW::UI::AsyncDecodeStr(enc_str, current_map_name, 128);
	});
}
// Get the Guild Wars friend object for this friend (if it exists)
GW::Friend* FriendListWindow::Friend::GetFriend() {
	return GW::FriendListMgr::GetFriend((uint8_t*)uuid.c_str());
}
// Start whisper to this player via their current char name.
void FriendListWindow::Friend::StartWhisper() {
	// open whisper to player
	GW::GameThread::Enqueue([this]() {
		if (current_char == nullptr)
			return Log::Error("Player %s is not logged in", alias);
		GW::UI::SendUIMessage(GW::UI::kOpenWhisper, &current_char->name, nullptr);
	});
}
// Send a whisper to this player advertising your current party
void FriendListWindow::Friend::InviteToParty() {
	Log::Log("INVITE TODO\n");
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::GetCharacter(const wchar_t* char_name) {
	for (unsigned int i = 0; i < characters.size(); i++) {
		if (characters[i].name._Equal(char_name))
			return &characters[i];
	}
	return nullptr;
}
// Add this friend to the GW Friend list to find out online status etc.
bool FriendListWindow::Friend::AddGWFriend() {
	GW::Friend* f = GetFriend();
	if (f) return true;
	if (characters.empty()) return false; // Cant add a friend that doesn't have a char name
	GW::FriendListMgr::AddFriend(characters.front().name.c_str());
	added_via_toolbox = true;
	last_update = clock();
	return true;
}
// Remove this friend from the GW Friend list e.g. if its a toolbox friend, and we only added them for info.
bool FriendListWindow::Friend::RemoveGWFriend() {
	GW::Friend* f = GetFriend();
	if (!f) return false;
	GW::FriendListMgr::RemoveFriend(f);
	last_update = clock();
	return true;
}

/* Setters */
// Update local friend record from raw info.
FriendListWindow::Friend* FriendListWindow::SetFriend(uint8_t* uuid, uint8_t type, uint8_t status, uint32_t map_id, const wchar_t* charname, const wchar_t* alias) {
	Friend* lf = GetFriend(uuid);
	if (!lf && charname)
		lf = GetFriend(charname);
	if(!lf && alias)
		lf = GetFriend(alias);
	if(!lf) {
		// New friend
		Friend nlf;
		nlf.uuid = std::string((char*)uuid);
		nlf.type = type;
		nlf.alias = std::wstring(alias);
		friends.emplace(nlf.uuid, nlf);
		lf = GetFriend(uuid);
	}
	if (!lf->uuid[0] || strcmp((char*)uuid, lf->uuid.c_str())) {
		// UUID is different. This could be because toolbox created a uuid and it needs updating.
		lf->uuid = std::string((char*)uuid);
	}
	if (alias && wcscmp(alias, lf->alias.c_str())) {
		lf->alias = std::wstring(alias);
	}
	if (map_id && lf->current_map_id != map_id && !lf->current_map_id) {
		lf->current_map_id = map_id;
		memset(lf->current_map_name, 0, sizeof lf->current_map_name);
		lf->GetMapName();
	}		

	// Check and copy charnames, only if player is NOT offline
	if (!charname || status == 0)
		lf->current_char = nullptr;
	if (status != 0 && charname) {
		bool has_name = false;
		for (unsigned int i = 0; i < lf->characters.size(); i++) {
			if (!has_name && lf->characters[i].name._Equal(charname)) {
				lf->current_char = &lf->characters[i];
				has_name = true;
				break;
			}
		}
		if (!has_name) {
			lf->characters.push_back({ charname,0 });
			lf->current_char = &lf->characters.back();
			uuid_by_name.emplace(charname, lf->uuid);
		}
	}
	if (status != 255) {
		if (lf->status != status)
			need_to_reorder_friends = true;
		lf->status = status;
	}
	friends_changed = true;
	return lf;
}
// Update local friend record from existing friend.
FriendListWindow::Friend* FriendListWindow::SetFriend(GW::Friend* f) {
	return SetFriend((uint8_t*)&f->uuid[0], f->type, f->status, f->zone_id, (wchar_t*)&f->charname[0], (wchar_t*)&f->alias[0]);
}

/* Getters */
// Find existing record for friend by char name.
FriendListWindow::Friend* FriendListWindow::GetFriend(const wchar_t* name) {
	std::map<std::wstring, std::string>::iterator it = uuid_by_name.find(name);
	if (it == uuid_by_name.end())
		return nullptr; // Not found
	return GetFriend((uint8_t*)it->second.c_str());
}
// Find existing record for friend by GW Friend object
FriendListWindow::Friend* FriendListWindow::GetFriend(GW::Friend* f) {
	return f ? GetFriend(f->uuid) : nullptr;
}
// Find existing record for friend by uuid
FriendListWindow::Friend* FriendListWindow::GetFriend(uint8_t* uuid) {
	std::map<std::string, Friend>::iterator it = friends.find((char*)uuid);
	if (it == friends.end())
		return nullptr;
	return &it->second; // Found in cache
}

/* FriendListWindow basic functions etc */
void FriendListWindow::Initialize() {
    ToolboxWindow::Initialize();
    worker = std::thread([this]() {
        while (!should_stop) {
            if(thread_jobs.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else {
                thread_jobs.front()();
                thread_jobs.pop();
            }
        }
    });

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusUpdate_Entry, [this](GW::HookStatus*,
        GW::Friend* f,
        GW::FriendStatus status,
        const wchar_t* alias,
        const wchar_t* charname) {
        // Keep a log mapping char name to uuid. This is saved to disk.
        Friend* lf = SetFriend(f->uuid, f->type, status, f->zone_id, charname, alias);
        // If we only added this user to get an updated status, then remove this user now.
        if (lf->added_via_toolbox) {
			lf->RemoveGWFriend();
        }
        lf->last_update = clock();
    });
	// If a friend has just logged in on a character in this map, record their profession.
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) {
		wchar_t player_name[30] = { 0 };
		
		wcscpy(player_name, pak->player_name);
		thread_jobs.push([this, player_name](){
			Log::Log("Checking player profession %ls", player_name);
			GW::Player* a = GW::PlayerMgr::GetPlayerByName(player_name);
			if (!a || !a->primary) return;
			uint8_t profession = a->primary;
			Friend* f = GetFriend(player_name);
			if (!f) return;
			Character* fc = f->GetCharacter(player_name);
			if (!fc) return;
			Log::Log("Friend found; setting %ls profession to %d", player_name, profession);
			fc->profession = profession;
		});
		
	});
	// Does "Failed to add friend" or "Friend <name> already added as <name>" come into this hook???
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&ErrorMessage_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::MessageServer* pak) {
        Log::Log("%s: Error message received: %d", Name(), pak->id);
    });
}
void FriendListWindow::Update(float delta) {
	if (loading)
		return;
	if (!GW::Map::GetIsMapLoaded())
		return;
	GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
	friend_list_ready = fl && fl->friends.valid();
	if (!friend_list_ready) 
		return;
	if (need_to_reorder_friends) {
		need_to_reorder_friends = false;
		thread_jobs.push([this]() {
			need_to_reorder_friends = false;
			Reorder();
			});
	}
	if (!poll_queued) {
		int interval_check = poll_interval_seconds * CLOCKS_PER_SEC;
		if (clock() - friends_list_checked > interval_check) {
			Log::Log("Queueing poll friends list\n");
			poll_queued = true;
			thread_jobs.push([this]() {
				poll_queued = false;
				Poll();
				});
		}
	}
}
void FriendListWindow::Reorder() {
	if (loading) return;
	loading = true;

}
void FriendListWindow::Poll() {
	if (loading || polling) return;
	polling = true;
    clock_t now = clock();
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    if (!fl || fl->number_of_friend >= 100) return;
    Log::Log("Polling friends list\n");
	for (unsigned int i = 0; i < fl->friends.size(); i++) {
		GW::Friend* f = fl->friends[i];
		if (!f) continue;
		Friend* lf = SetFriend(f->uuid, f->type, f->status, f->zone_id, f->charname, f->alias);
		// If we only added this user to get an updated status, then remove this user now.
		if (lf->added_via_toolbox) {
			lf->RemoveGWFriend();
		}
		lf->last_update = now;
	}
	Log::Log("Polling non-gw friends list\n");
    std::map<std::string, Friend>::iterator it = friends.begin();
    while (it != friends.end()) {
        if (fl->number_of_friend >= 100)
            break; // No more space to add friends.
        Friend* lf = &it->second;
        if (lf->is_tb_friend && lf->NeedToUpdate(now) && !lf->GetFriend()) {
            // Add the friend to friend list. The RegisterFriendStatusCallback will remove it once we get the response.
			lf->AddGWFriend();
            //GW::FriendListMgr::AddFriend(lf->charnames.front().c_str(),lf->alias.c_str());
            lf->last_update = now;
        }
		it++;
    }
	Log::Log("Friends list polled\n");
	friends_list_checked = now;
	polling = false;
}
void FriendListWindow::Draw(IDirect3DDevice9* pDevice) {
    if (!visible)
        return;
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGuiIO* io = &ImGui::GetIO();
	ImVec2 window_size = ImVec2(540.0f * io->FontGlobalScale, 512.0f * io->FontGlobalScale);
	if (!show_location)
		window_size.x -= 180.0f * io->FontGlobalScale;
    ImGui::SetNextWindowSize(window_size, ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    float cols[3] = { 180.0f, 360.0f, 540.0f};
	unsigned int colIdx = 0;
    ImGui::Text("Name");
    ImGui::SameLine(cols[colIdx] *= io->FontGlobalScale);
    ImGui::Text("Character(s)");
	if (show_location) {
		ImGui::SameLine(cols[++colIdx] *= io->FontGlobalScale);
		ImGui::Text("Map");
	}
    ImGui::Separator();
    ImGui::BeginChild("friend_list_scroll");
    for (std::map<std::string, Friend>::iterator it = friends.begin(); it != friends.end(); ++it) {
		colIdx = 0;
		Friend* lfp = &it->second;
        if (lfp->type != GW::FriendType_Friend) continue;
		if (lfp->status < 1) continue;
		if (lfp->alias.empty()) continue;
		// Get actual object instead of pointer just in case it becomes invalid half way through the draw.
		Friend lf = it->second; 
		float height = ImGui::GetTextLineHeightWithSpacing();
        
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushID(lf.uuid.c_str());
		bool clicked = ImGui::Button("", ImVec2(ImGui::GetContentRegionAvailWidth(), height));
		
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::SameLine(2.0f,0);
		ImGui::PushStyleColor(ImGuiCol_Text,StatusColors[lf.status].Value);
		ImGui::Bullet();
		ImGui::PopStyleColor();
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip(GetStatusText(lf.status));
		ImGui::SameLine(0);
		ImGui::Text(GuiUtils::WStringToString(lf.alias).c_str());
		ImGui::SameLine(cols[colIdx]);
		if (lf.current_char != nullptr) {
			std::string current_char_name_s = GuiUtils::WStringToString(lf.current_char->name);
			uint8_t prof = lf.current_char->profession;
			if (prof) ImGui::PushStyleColor(ImGuiCol_Text, GetProfessionColor(lf.current_char->profession).Value);
			ImGui::Text("%s", current_char_name_s.c_str());
			if (prof) ImGui::PopStyleColor();
			if (lf.characters.size() > 1) {
				bool hovered = ImGui::IsItemHovered();
				ImGui::SameLine(0,0);
				ImGui::Text(" (+%d)", lf.characters.size() - 1);
				hovered |= ImGui::IsItemHovered();
				if (hovered) {
					std::wstring charnames(L"Characters for ");
					charnames += lf.alias;
					charnames += L":\n";
					for (unsigned int i = 0; i < lf.characters.size(); i++) {
						if(i > 0) charnames += L"\n  ";
						charnames += lf.characters[i].name;
						if (lf.characters[i].profession) {
							charnames += L" (";
							charnames += ProfNames[lf.characters[i].profession];
							charnames += L")";
						}
					}
					ImGui::SetTooltip(GuiUtils::WStringToString(charnames).c_str());
				}
			}
			if (show_location) {
				ImGui::SameLine(cols[++colIdx]);
				if (lf.current_map_name) {
					ImGui::Text(lf.current_map_name);
				}
			}
		}
		ImGui::PopID();
		
    }
    ImGui::EndChild();
    ImGui::End();
}
void FriendListWindow::DrawSettingInternal() {
	bool edited = false;
	edited |= ImGui::Checkbox("Show Friend Location", &show_location);
	ImGui::ShowHelp("Show 'Map' column in friend list");
}
void FriendListWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
	show_location = ini->GetBoolValue(Name(), VAR_NAME(show_location), show_location);

    LoadFromFile();
}
void FriendListWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(show_location), show_location);

    SaveToFile();
}
void FriendListWindow::Terminate() {
    ToolboxWindow::Terminate();
    should_stop = true;
    if (worker.joinable()) worker.join();
}
void FriendListWindow::LoadFromFile() {
	loading = true;
	Log::Log("%s: Loading friends from ini", Name());
	thread_jobs.push([this]() {
		// clear builds from toolbox
		uuid_by_name.clear();
		friends.clear();

		if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
		inifile->SetMultiKey(true);
		inifile->LoadFile(Resources::GetPath(ini_filename).c_str());

		CSimpleIni::TNamesDepend entries;
		inifile->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			char uuid[128];
			b64_dec(entry.pItem, uuid);
			uint8_t type = 0;

			Friend lf;
			lf.uuid = std::string(uuid);
			lf.alias = GuiUtils::StringToWString(inifile->GetValue(entry.pItem, "alias", ""));
			lf.type = inifile->GetLongValue(entry.pItem, "type", lf.type);
			if (lf.uuid.empty() || lf.alias.empty())
				continue; // Error, alias or uuid empty.
			GW::Friend* f = lf.GetFriend();
			lf.is_tb_friend = f == nullptr;

			// Grab char names
			CSimpleIniA::TNamesDepend values;
			inifile->GetAllValues(entry.pItem, "charname", values);
			CSimpleIniA::TNamesDepend::const_iterator i;
			for (i = values.begin(); i != values.end(); ++i) {
				std::wstring char_wstr = GuiUtils::StringToWString(i->pItem), temp;
				std::vector<std::wstring> parts;
				std::wstringstream wss(char_wstr);
				while (std::getline(wss, temp, L','))
					parts.push_back(temp);
				Character c;
				c.name = parts[0];
				if (parts.size() > 1) {
					int p = _wtoi(&parts[1][0]);
					if (p > 0 && p < 10)
						c.profession = (uint8_t)p;
				}
				lf.characters.push_back(c);
			}
			if (lf.characters.empty())
				continue; // Error, should have at least 1 charname...
			friends.emplace(lf.uuid, lf);
			for (unsigned int i = 0; i < lf.characters.size(); i++) {
				uuid_by_name.emplace(lf.characters[i].name, uuid);
			}
			if (f) SetFriend(f);
		}
		Log::Log("%s: Loaded friends from ini", Name());
		loading = false;
		Poll();
	});
}
void FriendListWindow::SaveToFile() {
    if (!friends_changed)
        return;
    friends_changed = false;
    if (inifile == nullptr) inifile = new CSimpleIni();
	inifile->SetMultiKey(true);
    if (friends.empty())
        return; // Error, should have at least 1 friend
    inifile->Reset();
    for (std::map<std::string, Friend>::iterator it = friends.begin(); it != friends.end(); ++it) {
        // do something
        Friend lf = it->second;
		char uuid[128];
		b64_enc((wchar_t*)lf.uuid.data(),lf.uuid.size(),uuid);
        inifile->SetLongValue(uuid, "type", lf.type);
		inifile->SetValue(uuid, "alias", GuiUtils::WStringToString(lf.alias).c_str());
        for (unsigned int i = 0; i < lf.characters.size(); i++) {
			char charname[128] = { 0 };
			snprintf(charname, 128, "%s,%d", GuiUtils::WStringToString(lf.characters[i].name).c_str(), lf.characters[i].profession);
            inifile->SetValue(uuid, "charname", charname);
        }
    }
    inifile->SaveFile(Resources::GetPath(ini_filename).c_str());
}