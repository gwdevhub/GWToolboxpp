#include "stdafx.h"

#include "FriendListWindow.h"

#include <algorithm>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Modules/Resources.h>

#include "logger.h"
#include "base64.h"


static const std::wstring GetPlayerNameFromEncodedString(const wchar_t* message) {
	int start_idx = -1;
	for (size_t i = 0; message[i] != 0; ++i) {
		if (start_idx < 0 && message[i] == 0x107)
			start_idx = ++i;
		if (message[i] == 0x1)
			return std::wstring(&message[start_idx], i - start_idx);
	}
	return L"";
}

/*	FriendListWindow::Friend	*/

void FriendListWindow::Friend::GetMapName() {
	if (!current_map_id)
		return;
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
	return GW::FriendListMgr::GetFriend((uint8_t*)&uuid);
}
// Start whisper to this player via their current char name.
void FriendListWindow::Friend::StartWhisper() {
    const wchar_t* alias_c = alias.c_str();
    const wchar_t* charname = current_char ? current_char->name.c_str() : '\0';
    
	GW::GameThread::Enqueue([charname, alias_c]() {
        if(!charname[0])
            return Log::Error("Player %S is not logged in", alias_c);
		GW::UI::SendUIMessage(GW::UI::kOpenWhisper, (wchar_t*)charname, nullptr);
	});
}
// Send a whisper to this player advertising your current party
void FriendListWindow::Friend::InviteToParty() {
    const wchar_t* alias_c = alias.c_str();
    const wchar_t* charname = current_char ? current_char->name.c_str() : '\0';

    GW::GameThread::Enqueue([charname, alias_c]() {
        if (!charname[0])
            return Log::Error("Player %S is not logged in", alias_c);
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
            return Log::Error("You are not in an outpost");
        std::wstring* map_name = GetCurrentMapName();
        if (!map_name) return;
        GW::AreaInfo* ai = GW::Map::GetCurrentMapInfo();
        if (!ai) return;
        GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
        if (!p || !p->players.valid())
            return;
        wchar_t buffer[139] = { 0 };
        swprintf(buffer, 138, L"%s,%s %d/%d", charname, map_name->c_str(), p->players.size(), ai->max_party_size);
        buffer[138] = 0;
        GW::Chat::SendChat('"', buffer);
        });
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::GetCharacter(const wchar_t* char_name) {
	std::map<std::wstring, Character>::iterator it = characters.find(char_name);
	if (it == characters.end())
		return nullptr; // Not found
	return &it->second;
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::SetCharacter(const wchar_t* char_name, uint8_t profession = 0) {
	Character* existing = GetCharacter(char_name);
	if (!existing) {
		Character c;
		c.name = std::wstring(char_name);
		characters.emplace(c.name, c);
		existing = GetCharacter(c.name.c_str());
		cached_charnames_hover = false;
	}
	if (profession && profession != existing->profession) {
		existing->profession = profession;
		cached_charnames_hover = false;
	}
	return existing;
}
// Add this friend to the GW Friend list to find out online status etc.
bool FriendListWindow::Friend::AddGWFriend() {
	GW::Friend* f = GetFriend();
	if (f) return true;
	if (characters.empty()) return false; // Cant add a friend that doesn't have a char name
	clock_t now = clock();
	if (added_via_toolbox > now - 5)
		return true; // Waiting for friend to be added.
	added_via_toolbox = clock();
	GW::FriendListMgr::AddFriend(characters.begin()->first.c_str());
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
	std::lock_guard<std::mutex> lock(friends_mutex);
	char uuid_c[128];
	GuidToString(*(UUID*)uuid, uuid_c);
	if(!lf) {
		// New friend
        lf = new Friend();
        lf->type = type;
        lf->alias = std::wstring(alias);
		friends.emplace(uuid_c, lf);
	}
	if (strcmp(lf->uuid.c_str(),uuid_c) != 0) {
		// UUID is different. This could be because toolbox created a uuid and it needs updating.
		lf->uuid = std::string(uuid_c);
	}
	if (alias && wcscmp(alias, lf->alias.c_str())) {
		lf->alias = std::wstring(alias);
	}
	if (lf->current_map_id != map_id) {
		lf->current_map_id = map_id;
		memset(lf->current_map_name, 0, sizeof lf->current_map_name);
		lf->GetMapName();
	}		

	// Check and copy charnames, only if player is NOT offline
	if (!charname || status == 0)
		lf->current_char = nullptr;
	if (status != 0 && charname) {
		lf->current_char = lf->SetCharacter(charname);
		uuid_by_name.emplace(charname, lf->uuid);
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
	return SetFriend(f->uuid, f->type, f->status, f->zone_id, (wchar_t*)&f->charname[0], (wchar_t*)&f->alias[0]);
}

/* Getters */
// Find existing record for friend by char name.
FriendListWindow::Friend* FriendListWindow::GetFriend(const wchar_t* name) {
	std::map<std::wstring, std::string>::iterator it = uuid_by_name.find(name);
	if (it == uuid_by_name.end())
		return nullptr; // Not found
	return GetFriendByUUID(it->second.c_str());
}
// Find existing record for friend by GW Friend object
FriendListWindow::Friend* FriendListWindow::GetFriend(GW::Friend* f) {
	return f ? GetFriend(f->uuid) : nullptr;
}
FriendListWindow::Friend* FriendListWindow::GetFriend(uint8_t* uuid) {
	char uuid_c[128];
	GuidToString(*(UUID*)uuid, uuid_c);
	return GetFriendByUUID(uuid_c);
}
// Find existing record for friend by uuid
FriendListWindow::Friend* FriendListWindow::GetFriendByUUID(const char* uuid) {
    std::lock_guard<std::mutex> lock(friends_mutex);
	std::map<std::string, Friend*>::iterator it = friends.find(uuid);
	if (it == friends.end())
		return nullptr;
	return it->second; // Found in cache
}

/* FriendListWindow basic functions etc */
void FriendListWindow::Initialize() {
    ToolboxWindow::Initialize();
	worker.Run();
    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusUpdate_Entry, [this](GW::HookStatus*,
        GW::Friend* f,
        GW::FriendStatus status,
        const wchar_t* alias,
        const wchar_t* charname) {
        // Keep a log mapping char name to uuid. This is saved to disk.
		Friend* lf = SetFriend(f->uuid, f->type, status, f->zone_id, charname, alias);
		// If we only added this user to get an updated status, then remove this user now.
		if (lf->is_tb_friend) {
			lf->RemoveGWFriend();
		}
		lf->last_update = clock();
    });
	// If a friend has just logged in on a character in this map, record their profession.
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) {
        const wchar_t* player_name = std::wstring(pak->player_name).c_str();
		worker.Add([this, player_name](){
			//Log::Log("%s: Checking player profession %ls\n", player_name);
			GW::Player* a = GW::PlayerMgr::GetPlayerByName(player_name);
			if (!a || !a->primary) return;
			uint8_t profession = a->primary;
			Friend* f = GetFriend(player_name);
			if (!f) return;
			Character* fc = f->GetCharacter(player_name);
			if (!fc) return;
			Log::Log("%s: Friend found; setting %ls profession to %s\n", Name(), player_name, ProfNames[profession]);
			fc->profession = profession;
		});
	});
    // "Failed to add friend" or "Friend <name> already added as <name>"
    GW::Chat::RegisterLocalMessageCallback(&ErrorMessage_Entry,
        [this](GW::HookStatus* status, int channel, wchar_t* message) -> void {
            wchar_t buffer[64] = { 0 };
            wcsncpy(buffer, message, 64);
            worker.Add([this, buffer]() {
                std::wstring player_name;
                switch (buffer[0]) {
                case 0x2f3: // You cannot add yourself as a friend.
                    break;
                case 0x2f4: // You have exceeded the maximum number of characters on your Friends list.
                    break;
                case 0x2F5: // The Character name "" does not exist
					player_name = GetPlayerNameFromEncodedString(buffer);
                    break;
                case 0x2F6: // The Character you're trying to add is already in your friend list as "".
					player_name = GetPlayerNameFromEncodedString(buffer);
                    break;
				case 0x881: // Player "" is not online.
					player_name = GetPlayerNameFromEncodedString(buffer);
					// TODO: Try to redirect whisper to the right person.
					break;
                }
                });
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
	if (!poll_queued) {
		int interval_check = poll_interval_seconds * CLOCKS_PER_SEC;
		if (!friends_list_checked || clock() - friends_list_checked > interval_check) {
			//Log::Log("Queueing poll friends list\n");
			poll_queued = true;
			worker.Add([this]() {
				poll_queued = false;
				Poll();
				});
		}
	}
}
void FriendListWindow::Poll() {
	if (loading || polling) return;
	polling = true;
    clock_t now = clock();
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    if (!fl) return;
    //Log::Log("Polling friends list\n");
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
	//Log::Log("Polling non-gw friends list\n");
    std::map<std::string, Friend*>::iterator it = friends.begin();
    while (it != friends.end()) {
        if (fl->number_of_friend >= 100)
            break; // No more space to add friends.
        Friend* lf = it->second;
        if (lf->is_tb_friend && lf->NeedToUpdate(now) && !lf->GetFriend()) {
            // Add the friend to friend list. The RegisterFriendStatusCallback will remove it once we get the response.
			lf->AddGWFriend();
            //GW::FriendListMgr::AddFriend(lf->charnames.front().c_str(),lf->alias.c_str());
            lf->last_update = now;
        }
		it++;
    }
	//Log::Log("Friends list polled\n");
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
    for (std::map<std::string, Friend*>::iterator it = friends.begin(); it != friends.end(); ++it) {
		colIdx = 0;
		Friend* lfp = it->second;
        if (lfp->type != GW::FriendType_Friend) continue;
		// Get actual object instead of pointer just in case it becomes invalid half way through the draw.
		Friend lf = *lfp;
		if (lf.IsOffline()) continue;
		if (lf.alias.empty()) continue;
		
		float height = ImGui::GetTextLineHeightWithSpacing();
        
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushID(lf.uuid.c_str());
		bool clicked = ImGui::Button("", ImVec2(ImGui::GetContentRegionAvailWidth(), height));
		
		ImGui::PopStyleVar(4);
		ImGui::SameLine(2.0f,0);
		ImGui::PushStyleColor(ImGuiCol_Text,StatusColors[lf.status].Value);
		ImGui::Bullet();
		ImGui::PopStyleColor(2);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip(GetStatusText(lf.status));
		ImGui::SameLine(0);
		ImGui::Text(GuiUtils::WStringToString(lf.alias).c_str());
		if (lf.current_char != nullptr) {
			ImGui::SameLine(cols[colIdx]);
			std::string current_char_name_s = GuiUtils::WStringToString(lf.current_char->name);
			uint8_t prof = lf.current_char->profession;
			if (prof) ImGui::PushStyleColor(ImGuiCol_Text, ProfColors[lf.current_char->profession].Value);
			ImGui::Text("%s", current_char_name_s.c_str());
			if (prof) ImGui::PopStyleColor();
			if (lf.characters.size() > 1) {
				bool hovered = ImGui::IsItemHovered();
				ImGui::SameLine(0,0);
				ImGui::Text(" (+%d)", lf.characters.size() - 1);
				hovered |= ImGui::IsItemHovered();
				if (hovered) {
					ImGui::SetTooltip(lf.GetCharactersHover().c_str());
					
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
		if (clicked && !lf.IsOffline())
			lf.StartWhisper();
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
void FriendListWindow::SignalTerminate() {
    // Try to remove callbacks here.
	GW::FriendListMgr::RemoveFriendStatusCallback(&FriendStatusUpdate_Entry);
	GW::StoC::RemoveCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry);
	// Remove any friends added via toolbox.
	worker.Add([this]() {
		GW::GameThread::Enqueue([this]() {
            std::lock_guard<std::mutex> lock(friends_mutex);
			for (std::map<std::string, Friend*>::iterator it = friends.begin();  it != friends.end(); it++) {
				Friend* f = it->second;
                if (f->is_tb_friend && f->GetFriend()) {
                    f->RemoveGWFriend();
                }
			}
			worker.Stop();
			});
		});
}
bool FriendListWindow::CanTerminate() {
	return !worker.IsRunning();
}
void FriendListWindow::Terminate() {
    ToolboxWindow::Terminate();
    // Try to remove callbacks AGAIN here.
    GW::FriendListMgr::RemoveFriendStatusCallback(&FriendStatusUpdate_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry);
    // Free memory for Friends list.
    std::lock_guard<std::mutex> lock(friends_mutex);
    for (std::map<std::string, Friend*>::iterator it = friends.begin(); it != friends.end(); it++) {
        if (it->second)
            delete it->second;
    }
    friends.clear();
}
void FriendListWindow::LoadFromFile() {
	loading = true;
	Log::Log("%s: Loading friends from ini", Name());
	worker.Add([this]() {
        std::lock_guard<std::mutex> lock(friends_mutex);
		// clear builds from toolbox
		uuid_by_name.clear();
		friends.clear();

		if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
		inifile->SetMultiKey(true);
		inifile->LoadFile(Resources::GetPath(ini_filename).c_str());

		CSimpleIni::TNamesDepend entries;
		inifile->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			uint8_t type = 0;
			Friend* lf = new Friend();
			lf->uuid = entry.pItem;
			lf->alias = GuiUtils::StringToWString(inifile->GetValue(entry.pItem, "alias", ""));
			lf->type = (uint8_t)inifile->GetLongValue(entry.pItem, "type", lf->type);
            if (lf->uuid.empty() || lf->alias.empty()) {
                delete lf;
                continue; // Error, alias or uuid empty.
            }

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
				std::wstring name = parts[0];
				uint8_t profession = 0;
				if (parts.size() > 1) {
					int p = _wtoi(&parts[1][0]);
					if (p > 0 && p < 10)
						profession = (uint8_t)p;
				}
				lf->SetCharacter(name.c_str(), profession);
			}
            if (lf->characters.empty()) {
                delete lf;
                continue; // Error, should have at least 1 charname...
            }
			friends.emplace(lf->uuid, lf);
			for (std::map<std::wstring, Character>::iterator it2 = lf->characters.begin(); it2 != lf->characters.end(); ++it2) {
				uuid_by_name.emplace(it2->first, lf->uuid);
			}
		}
		Log::Log("%s: Loaded friends from ini", Name());
		loading = false;
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
    std::lock_guard<std::mutex> lock(friends_mutex);
    for (std::map<std::string, Friend*>::iterator it = friends.begin(); it != friends.end(); ++it) {
        // do something
        Friend lf = *it->second;
		const char* uuid = lf.uuid.c_str();
        inifile->SetLongValue(uuid, "type", lf.type);
		inifile->SetBoolValue(uuid, "is_tb_friend", lf.is_tb_friend);
		inifile->SetValue(uuid, "alias", GuiUtils::WStringToString(lf.alias).c_str());
		for (std::map<std::wstring, Character>::iterator it2 = lf.characters.begin(); it2 != lf.characters.end(); ++it2) {
			char charname[128] = { 0 };
			snprintf(charname, 128, "%s,%d", GuiUtils::WStringToString(it2->first).c_str(), it2->second.profession);
			inifile->SetValue(uuid, "charname", charname);
		}
    }
    inifile->SaveFile(Resources::GetPath(ini_filename).c_str());
}