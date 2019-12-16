#include "stdafx.h"
#include "Missions.h"

#include <GWCA\Constants\Constants.h>
#include <GWCA\GameContainers\Array.h>
#include <GWCA\GameContainers\GamePos.h>

#include <GWCA\GameEntities\Agent.h>
#include <GWCA\GameEntities\Skill.h>

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\PlayerMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "logger.h"
#include <Keys.h>
#include "BuildsWindow.h"
#include "HeroBuildsWindow.h"
#include "MissionsWindow.h"
#include "PconsWindow.h"
#include <ImGuiAddons.h>


bool TBMission::show_active_in_header = true;
bool TBMission::show_run_in_header = true;
bool TBMission::hotkeys_changed = false;
unsigned int TBMission::cur_ui_id = 0;

TBMission* TBMission::HotkeyFactory(CSimpleIni* ini, const char* section) {
	std::string str(section);
	if (str.compare(0, 7, "hotkey-") != 0) return nullptr;
	size_t first_sep = 6;
	size_t second_sep = str.find(L':', first_sep);
	std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
	std::string type = str.substr(second_sep + 1);

	if (type.compare(MissionSendChat::IniSection()) == 0) {
		return new MissionSendChat(ini, section);
	}
	else if (type.compare(MissionUseItem::IniSection()) == 0) {
		return new MissionUseItem(ini, section);
	}
	else {
		return nullptr;
	}
}

TBMission::TBMission(CSimpleIni* ini, const char* section) : ui_id(++cur_ui_id) {
	hotkey = ini ? ini->GetLongValue(section, VAR_NAME(hotkey), 0) : 0;
	modifier = ini ? ini->GetLongValue(section, VAR_NAME(modifier), 0) : 0;
	active = ini ? ini->GetBoolValue(section, VAR_NAME(active), true) : true;
}
void TBMission::Save(CSimpleIni* ini, const char* section) const {
	ini->SetLongValue(section, VAR_NAME(hotkey), hotkey);
	ini->SetLongValue(section, VAR_NAME(modifier), modifier);
	ini->SetBoolValue(section, VAR_NAME(active), active);
}
void TBMission::Draw(Op* op) {
	auto ShowHeaderButtons = [&]() {
		if (show_active_in_header || show_run_in_header) {
			ImGui::PushID(ui_id);
			ImGui::PushID("header");
			ImGuiStyle& style = ImGui::GetStyle();
			if (show_active_in_header) {
				ImGui::SameLine(ImGui::GetContentRegionAvailWidth()
					- ImGui::GetTextLineHeight()
					- style.FramePadding.y * 2
					- (show_run_in_header ? (50.0f + ImGui::GetStyle().ItemSpacing.x) : 0));
				if (ImGui::Checkbox("", &active)) hotkeys_changed = true;
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("The hotkey can trigger only when selected");
			}
			if (show_run_in_header) {
				ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - 50.0f);
				if (ImGui::Button("Run", ImVec2(50.0f, 0.0f))) {
					Execute();
				}
			}
			ImGui::PopID();
			ImGui::PopID();
		}
	};

	// === Header ===
	char header[256];
	char desbuf[128];
	char keybuf[128];
	Description(desbuf, 128);
	ModKeyName(keybuf, 128, modifier, hotkey, "<None>");
	snprintf(header, 128, "%s [%s]###header%u", desbuf, keybuf, ui_id);
	ImGuiTreeNodeFlags flags = (show_active_in_header || show_run_in_header)
		? ImGuiTreeNodeFlags_AllowItemOverlap : 0;
	if (!ImGui::CollapsingHeader(header, flags)) {
		ShowHeaderButtons();
	}
	else {
		ShowHeaderButtons();
		ImGui::PushID(ui_id);
		ImGui::PushItemWidth(-70.0f);
		// === Specific section ===
		Draw();

		// === Hotkey section ===
		ImGui::Separator();
		if (ImGui::Checkbox("###active", &active)) hotkeys_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The hotkey can trigger only when selected");
		ImGui::SameLine();
		static LONG newkey = 0;
		char keybuf2[128];
		snprintf(keybuf2, 128, "Hotkey: %s", keybuf);
		if (ImGui::Button(keybuf2, ImVec2(-70.0f, 0))) {
			ImGui::OpenPopup("Select Hotkey");
			newkey = 0;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
		if (ImGui::BeginPopup("Select Hotkey")) {
			*op = Op_BlockInput;
			ImGui::Text("Press key");
			int newmod = 0;
			if (ImGui::GetIO().KeyCtrl) newmod |= ModKey_Control;
			if (ImGui::GetIO().KeyAlt) newmod |= ModKey_Alt;
			if (ImGui::GetIO().KeyShift) newmod |= ModKey_Shift;

			if (newkey == 0) { // we are looking for the key
				for (int i = 0; i < 512; ++i) {
					if (i == VK_CONTROL) continue;
					if (i == VK_SHIFT) continue;
					if (i == VK_MENU) continue;
					if (ImGui::GetIO().KeysDown[i]) {
						newkey = i;
					}
				}
			}
			else { // key was pressed, close if it's released
				if (!ImGui::GetIO().KeysDown[newkey]) {
					hotkey = newkey;
					modifier = newmod;
					ImGui::CloseCurrentPopup();
					hotkeys_changed = true;
				}
			}

			// write the key
			char newkey_buf[256];
			ModKeyName(newkey_buf, 256, newmod, newkey);
			ImGui::Text("%s", newkey_buf);
			if (ImGui::Button("Cancel", ImVec2(-1.0f, 0))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Run", ImVec2(70.0f, 0.0f))) {
			Execute();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute the hotkey now");

		// === Move and delete buttons ===
		if (ImGui::Button("Move Up", ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f, 0))) {
			*op = Op_MoveUp;
			hotkeys_changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the hotkey up in the list");
		ImGui::SameLine();
		if (ImGui::Button("Move Down", ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f, 0))) {
			*op = Op_MoveDown;
			hotkeys_changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the hotkey down in the list");
		ImGui::SameLine();
		if (ImGui::Button("Delete", ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f, 0))) {
			ImGui::OpenPopup("Delete Hotkey?");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the hotkey");
		if (ImGui::BeginPopupModal("Delete Hotkey?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n", Name());
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				*op = Op_Delete;
				hotkeys_changed = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::PopItemWidth();
		ImGui::PopID();
	}
}

MissionSendChat::MissionSendChat(CSimpleIni* ini, const char* section)
	: TBMission(ini, section) {
	strcpy_s(message, ini ? ini->GetValue(section, "msg", "") : "");
	channel = ini ? ini->GetValue(section, "channel", "/")[0] : '/';
}
void MissionSendChat::Save(CSimpleIni* ini, const char* section) const {
	TBMission::Save(ini, section);
	ini->SetValue(section, "msg", message);
	char buf[8];
	snprintf(buf, 8, "%c", channel);
	ini->SetValue(section, "channel", buf);
}
void MissionSendChat::Description(char* buf, int bufsz) const {
	snprintf(buf, bufsz, "Send chat '%c%s'", channel, message);
}
void MissionSendChat::Draw() {
	int index = 0;
	switch (channel) {
	case '/': index = 0; break;
	case '!': index = 1; break;
	case '@': index = 2; break;
	case '#': index = 3; break;
	case '$': index = 4; break;
	case '%': index = 5; break;
	case '"': index = 6; break;
	}
	static const char* channels[] = {
		"/ Commands",
		"! All",
		"@ Guild",
		"# Group",
		"$ Trade",
		"% Alliance",
		"\" Whisper"
	};
	if (ImGui::Combo("Channel", &index, channels, 7)) {
		switch (index) {
		case 0: channel = '/'; break;
		case 1: channel = '!'; break;
		case 2: channel = '@'; break;
		case 3: channel = '#'; break;
		case 4: channel = '$'; break;
		case 5: channel = '%'; break;
		case 6: channel = '"'; break;
		default: channel = '/'; break;
		}
		hotkeys_changed = true;
	}
	if (ImGui::InputText("Message", message, 139)) hotkeys_changed = true;
}
void MissionSendChat::Execute() {
	if (isLoading()) return;
	if (channel == L'/') {
		Log::Info("/%s", message);
	}
	GW::Chat::SendChat(channel, message);
}

MissionUseItem::MissionUseItem(CSimpleIni* ini, const char* section) : TBMission(ini, section) {
	item_id = ini ? ini->GetLongValue(section, "ItemID", 0) : 0;
	strcpy_s(name, ini ? ini->GetValue(section, "ItemName", "") : "");
}
void MissionUseItem::Save(CSimpleIni* ini, const char* section) const {
	TBMission::Save(ini, section);
	ini->SetLongValue(section, "ItemID", item_id);
	ini->SetValue(section, "ItemName", name);
}
void MissionUseItem::Description(char* buf, int bufsz) const {
	if (name[0] == '\0') {
		snprintf(buf, bufsz, "Use #%d", item_id);
	}
	else {
		snprintf(buf, bufsz, "Use %s", name);
	}
}
void MissionUseItem::Draw() {
	if (ImGui::InputInt("Item ID", (int*)&item_id)) hotkeys_changed = true;
	if (ImGui::InputText("Item Name", name, 140)) hotkeys_changed = true;
}
void MissionUseItem::Execute() {
	if (isLoading()) return;
	if (item_id == 0) return;

	bool used = GW::Items::UseItemByModelId(item_id, 1, 4);
	if (!used && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		used = GW::Items::UseItemByModelId(item_id, 8, 16);
	}

	if (!used) {
		if (name[0] == '\0') {
			Log::Info("Item #%d not found!", item_id);
		}
		else {
			Log::Info("%s not found!", name);
		}
	}
}
