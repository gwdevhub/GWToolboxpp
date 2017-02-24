#include "Hotkeys.h"

#include <iostream>
#include <sstream>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\PlayerMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "logger.h"
#include "ChatLogger.h"
#include "GWToolbox.h"
#include "Config.h"
#include "BuildPanel.h"

unsigned int TBHotkey::cur_ui_id = 0;

TBHotkey* TBHotkey::HotkeyFactory(CSimpleIni* ini, const char* section) {
	std::string str(section);
	if (str.compare(0, 6, "hotkey") != 0) return nullptr;
	size_t first_sep = 6;
	size_t second_sep = str.find(L':', first_sep);
	std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
	std::string type = str.substr(second_sep + 1);

	if (type.compare(HotkeySendChat::IniSection()) == 0) {
		return new HotkeySendChat(ini, section);
	} else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
		return new HotkeyUseItem(ini, section);
	} else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
		return new HotkeyDropUseBuff(ini, section);
	} else if (type.compare(HotkeyToggle::IniSection()) == 0) {
		return new HotkeyToggle(ini, section);
	} else if (type.compare(HotkeyAction::IniSection()) == 0) {
		return new HotkeyAction(ini, section);
	} else if (type.compare(HotkeyTarget::IniSection()) == 0) {
		return new HotkeyTarget(ini, section);
	} else if (type.compare(HotkeyMove::IniSection()) == 0) {
		return new HotkeyMove(ini, section);
	} else if (type.compare(HotkeyDialog::IniSection()) == 0) {
		return new HotkeyDialog(ini, section);
	} else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
		return new HotkeyPingBuild(ini, section);
	} else {
		return nullptr;
	}
}

TBHotkey::TBHotkey(CSimpleIni* ini, const char* section) : ui_id(++cur_ui_id) {
	key = ini ? ini->GetLongValue(section, "hotkey", 0) : 0;
	modifier = ini ? ini->GetLongValue(section, "modifier", 0) : 0;
	active = ini ? ini->GetBoolValue(section, "active", true) : true;
}
void TBHotkey::Save(CSimpleIni* ini, const char* section) const {
	ini->SetLongValue(section, "hotkey", (int)key);
	ini->SetLongValue(section, "modifier", (int)modifier);
	ini->SetBoolValue(section, "active", active);
}
void TBHotkey::Draw(Op* op) {
	// === Header ===
	char header[256];
	char desbuf[128];
	char keybuf[128];
	Description(desbuf, 128);
	ModKeyName((long)modifier, (long)key, keybuf, 128);
	sprintf_s(header, "%s [%s]###header%u", desbuf, keybuf, ui_id);
	if (ImGui::CollapsingHeader(header)) {
		ImGui::PushID(ui_id);
		ImGui::PushItemWidth(-70.0f);
		// === Specific section ===
		//ImGui::Text("Functionality:");
		Draw();

		// === Hotkey section ===
		ImGui::Separator();
		//ImGui::Text("Hotkey:");
		ImGui::Checkbox("###active", &active);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The hotkey can trigger only when selected");
		ImGui::SameLine();
		static LONG newkey = 0;
		char keybuf2[128];
		sprintf_s(keybuf2, "Hotkey: %s", keybuf);
		if (ImGui::Button(keybuf2, ImVec2(-70.0f, 0))) {
			ImGui::OpenPopup("Select Hotkey");
			newkey = 0;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
		if (ImGui::BeginPopup("Select Hotkey")) {
			*op = Op_BlockInput;
			ImGui::Text("Press key");
			int newmod = 0;
			if (ImGui::GetIO().KeyCtrl) newmod |= Key_Control;
			if (ImGui::GetIO().KeyAlt) newmod |= Key_Alt;
			if (ImGui::GetIO().KeyShift) newmod |= Key_Shift;

			if (newkey == 0) { // we are looking for the key
				for (int i = 0; i < 512; ++i) {
					if (i == VK_CONTROL) continue;
					if (i == VK_SHIFT) continue;
					if (i == VK_MENU) continue;
					if (ImGui::GetIO().KeysDown[i]) {
						newkey = i;
					}
				}
			} else { // key was pressed, close if it's released
				if (!ImGui::GetIO().KeysDown[newkey]) {
					key = newkey;
					modifier = newmod;
					ImGui::CloseCurrentPopup();
				}
			}

			// write the key
			char newkey_buf[256];
			ModKeyName(newmod, newkey, newkey_buf, 256);
			ImGui::Text("%s", newkey_buf);
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
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
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the hotkey up in the list");
		ImGui::SameLine();
		if (ImGui::Button("Move Down", ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f, 0))) {
			*op = Op_MoveDown;
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

HotkeySendChat::HotkeySendChat(CSimpleIni* ini, const char* section) 
	: TBHotkey(ini, section) {
	strcpy_s(message, ini ? ini->GetValue(section, "msg", "") : "");
	channel = ini ? ini->GetValue(section, "channel", "/")[0] : '/';
}
void HotkeySendChat::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetValue(section, "msg", message);
	char buf[8];
	sprintf_s(buf, "%c", channel);
	ini->SetValue(section, "channel", buf);
}
void HotkeySendChat::Description(char* buf, int bufsz) const {
	sprintf_s(buf, bufsz, "Send chat '%c%s'", channel, message);
}
void HotkeySendChat::Draw() {
	int index = 0;
	switch (channel) {
	case '/': index = 0; break;
	case '!': index = 1; break;
	case '@': index = 2; break;
	case '#': index = 3; break;
	case '$': index = 4; break;
	case '%': index = 5; break;
	}
	if (ImGui::Combo("Channel", &index, "/\0!\0@\0#\0$\0%")) {
		switch (index) {
		case 0: channel = '/'; break;
		case 1: channel = '!'; break;
		case 2: channel = '@'; break;
		case 3: channel = '#'; break;
		case 4: channel = '$'; break;
		case 5: channel = '%'; break;
		default: channel = '/';break;
		}
	}
	ImGui::InputText("Message", message, 139);
}
void HotkeySendChat::Execute() {
	if (isLoading()) return;
	if (channel == L'/') {
		ChatLogger::Log("/%s", message);
	}
	GW::Chat().SendChat(message, channel);
}

HotkeyUseItem::HotkeyUseItem(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	item_id = ini ? ini->GetLongValue(section, "ItemID", 0) : 0;
	strcpy_s(name, ini ? ini->GetValue(section, "ItemName", "") : "");
}
void HotkeyUseItem::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "ItemID", item_id);
	ini->SetValue(section, "ItemName", name);
}
void HotkeyUseItem::Description(char* buf, int bufsz) const {
	sprintf_s(buf, bufsz, "Use %s", name);
}
void HotkeyUseItem::Draw() {
	ImGui::InputInt("Item ID", (int*)&item_id);
	ImGui::InputText("Item Name", name, 140);
}
void HotkeyUseItem::Execute() {
	if (isLoading()) return;
	if (item_id == 0) return;
	if (!GW::Items().UseItemByModelId(item_id)) {
		ChatLogger::Log("[Warning] %s not found!", name);
	}
}

bool HotkeyDropUseBuff::GetText(void* data, int idx, const char** out_text) {
	static char other_buf[64];
	switch ((SkillIndex)idx) {
	case Recall: *out_text = "Recall"; break;
	case UA: *out_text = "UA"; break;
	case HolyVeil: *out_text = "Holy Veil"; break;
	default:
		sprintf_s(other_buf, "Skill#%d", (int)data);
		*out_text = other_buf;
		break;
	}
	return true;
}
HotkeyDropUseBuff::SkillIndex HotkeyDropUseBuff::GetIndex() const {
	switch (id) {
	case GW::Constants::SkillID::Recall: return Recall;
	case GW::Constants::SkillID::Unyielding_Aura: return UA;
	case GW::Constants::SkillID::Holy_Veil: return HolyVeil;
	default: return Other;
	}
}
HotkeyDropUseBuff::HotkeyDropUseBuff(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	id = ini ? (GW::Constants::SkillID)ini->GetLongValue(
		section, "SkillID", (long)GW::Constants::SkillID::Recall) : GW::Constants::SkillID::Recall;
}
void HotkeyDropUseBuff::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "SkillID", (long)id);
}
void HotkeyDropUseBuff::Description(char* buf, int bufsz) const {
	const char* skillname;
	GetText((void*)id, GetIndex(), &skillname);
	sprintf_s(buf, bufsz, "Drop/Use %s", skillname);
}
void HotkeyDropUseBuff::Draw() {
	SkillIndex index = GetIndex();
	if (ImGui::Combo("Skill", (int*)&index, GetText, (void*)id, 4)) {
		switch (index) {
		case HotkeyDropUseBuff::Recall: id = GW::Constants::SkillID::Recall; break;
		case HotkeyDropUseBuff::UA: id = GW::Constants::SkillID::Unyielding_Aura; break;
		case HotkeyDropUseBuff::HolyVeil: id = GW::Constants::SkillID::Holy_Veil; break;
		case HotkeyDropUseBuff::Other: /* don't change id */ break;
		default: break;
		}
	}
	if (index == Other) {
		ImGui::InputInt("Skill ID", (int*)&id);
	}
}
void HotkeyDropUseBuff::Execute() {
	if (!isExplorable()) return;

	GW::Buff buff = GW::Effects().GetPlayerBuffBySkillId(id);
	if (buff.SkillId) {
		GW::Effects().DropBuff(buff.BuffId);
	} else {
		int slot = GW::Skillbarmgr().GetSkillSlot(id);
		if (slot >= 0 && GW::Skillbar::GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			GW::Skillbarmgr().UseSkill(slot, GW::Agents().GetTargetId());
		}
	}
}

bool HotkeyToggle::GetText(void*, int idx, const char** out_text) {
	switch ((Toggle)idx) {
	case Clicker: *out_text = "Clicker"; return true;
	case Pcons: *out_text = "Pcons"; return true;
	case CoinDrop: *out_text = "Coin Drop"; return true;
	default: return false;
	}
}
HotkeyToggle::HotkeyToggle(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	target = ini ? (Toggle)ini->GetLongValue(section, "ToggleID", (long)Clicker) : Clicker;
}
void HotkeyToggle::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "ToggleID", (long)target);
}
void HotkeyToggle::Description(char* buf, int bufsz) const {
	const char* name;
	GetText(nullptr, (int)target, &name);
	sprintf_s(buf, bufsz, "Toggle %s", name);
}
void HotkeyToggle::Draw() {
	ImGui::Combo("Toggle###combo", (int*)&target, GetText, nullptr, n_targets);
}
void HotkeyToggle::Execute() {
	GWToolbox& tb = GWToolbox::instance();
	bool active;
	switch (target) {
	case HotkeyToggle::Clicker:
		active = tb.main_window->hotkey_panel->ToggleClicker();
		ChatLogger::Log("Clicker is %s", active ? "active" : "disabled");
		break;
	case HotkeyToggle::Pcons:
		tb.main_window->pcon_panel->ToggleEnable();
		// writing to chat is done by ToggleActive if needed
		break;
	case HotkeyToggle::CoinDrop:
		active = tb.main_window->hotkey_panel->ToggleCoinDrop();
		ChatLogger::Log("Coin dropper is %s", active ? "active" : "disabled");
		break;
	}
}

bool HotkeyAction::GetText(void*, int idx, const char** out_text) {
	switch ((Action)idx) {
	case OpenXunlaiChest: *out_text = "Open Xunlai Chest"; return true;
	case OpenLockedChest: *out_text = "Open Locked Chest"; return true;
	case DropGoldCoin: *out_text = "Drop Gold Coin"; return true;
	case ReapplyLBTitle: *out_text = "Reapply Lightbringer Title"; return true;
	default: return false;
	}
}
HotkeyAction::HotkeyAction(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	action = ini ? (Action)ini->GetLongValue(section, "ActionID", (long)OpenXunlaiChest) : OpenXunlaiChest;
}
void HotkeyAction::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "ActionID", (long)action);
}
void HotkeyAction::Description(char* buf, int bufsz) const {
	const char* name;
	GetText(nullptr, (int)action, &name);
	sprintf_s(buf, bufsz, "%s", name);
}
void HotkeyAction::Draw() {
	ImGui::Combo("Action###combo", (int*)&action, GetText, nullptr, n_actions);
}
void HotkeyAction::Execute() {
	if (isLoading()) return;
	switch (action) {
	case HotkeyAction::OpenXunlaiChest:
		if (isOutpost()) {
			GW::Items().OpenXunlaiWindow();
		}
		break;
	case HotkeyAction::OpenLockedChest: {
		if (isExplorable()) {
			GW::Agent* target = GW::Agents().GetTarget();
			if (target && target->Type == 0x200) {
				GW::Agents().GoSignpost(target);
				GW::Items().OpenLockedChest();
			}
		}
		break;
	}
	case HotkeyAction::DropGoldCoin:
		if (isExplorable()) {
			GW::Items().DropGold(1);
		}
		break;
	case HotkeyAction::ReapplyLBTitle:
		GW::Playermgr().RemoveActiveTitle();
		GW::Playermgr().SetActiveTitle(GW::Constants::TitleID::Lightbringer);
		break;
	}
}

HotkeyTarget::HotkeyTarget(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	id = ini ? ini->GetLongValue(section, "TargetID", 0) : 0;
	strcpy_s(name, ini ? ini->GetValue(section, "TargetName", "") : "");
}
void HotkeyTarget::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "TargetID", id);
	ini->SetValue(section, "TargetName", name);
}
void HotkeyTarget::Description(char* buf, int bufsz) const {
	sprintf_s(buf, bufsz, "Target %s", name);
}
void HotkeyTarget::Draw() {
	ImGui::InputInt("Target ID", (int*)&id);
	ImGui::InputText("Target Name", name, 140);
}
void HotkeyTarget::Execute() {
	if (isLoading()) return;
	if (id == 0) return;

	GW::AgentArray agents = GW::Agents().GetAgentArray();
	if (!agents.valid()) {
		return;
	}

	GW::Agent* me = agents[GW::Agents().GetPlayerId()];
	if (me == nullptr) return;

	float distance = GW::Constants::SqrRange::Compass;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber == id && agent->HP > 0) {
			float newDistance = GW::Agents().GetSqrDistance(me->pos, agents[i]->pos);
			if (newDistance < distance) {
				closest = i;
				distance = newDistance;
			}
		}
	}
	if (closest > 0) {
		GW::Agents().ChangeTarget(agents[closest]);
	}
}

HotkeyMove::HotkeyMove(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	x = ini ? (float)ini->GetDoubleValue(section, "x", 0.0) : 0.0f;
	y = ini ? (float)ini->GetDoubleValue(section, "y", 0.0) : 0.0f;
	strcpy_s(name, ini ? ini->GetValue(section, "name", "") : "");
}
void HotkeyMove::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetDoubleValue(section, "x", x);
	ini->SetDoubleValue(section, "y", y);
	ini->SetValue(section, "name", name);
}
void HotkeyMove::Description(char* buf, int bufsz) const {
	sprintf_s(buf, bufsz, "Move %s", name);
}
void HotkeyMove::Draw() {
	ImGui::InputFloat("x", &x, 0.0f, 0.0f, 3);
	ImGui::InputFloat("y", &y, 0.0f, 0.0f, 3);
	ImGui::InputText("Name", name, 140);
}
void HotkeyMove::Execute() {
	if (!isExplorable()) return;

	GW::Agent* me = GW::Agents().GetPlayer();
	double sqrDist = (me->X - x) * (me->X - x) + (me->Y - y) * (me->Y - y);
	if (sqrDist < GW::Constants::SqrRange::Compass) {
		GW::Agents().Move(x, y);
		ChatLogger::Log("%s movement macro activated", name);
	}
}

HotkeyDialog::HotkeyDialog(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	id = ini ? ini->GetLongValue(section, "DialogID", 0) : 0;
	strcpy_s(name, ini ? ini->GetValue(section, "DialogName", "") : "");
}
void HotkeyDialog::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "DialogID", id);
	ini->SetValue(section, "DialogName", name);
}
void HotkeyDialog::Description(char* buf, int bufsz) const {
	sprintf_s(buf, bufsz, "Dialog %s", name);
}
void HotkeyDialog::Draw() {
	ImGui::InputInt("Dialog ID", (int*)&id);
	ImGui::InputText("Dialog Name", name, 140);
}
void HotkeyDialog::Execute() {
	if (isLoading()) return;
	if (id == 0) return;

	GW::Agents().Dialog(id);
	ChatLogger::Log("Sent dialog %s (%d)", name, id);
}

bool HotkeyPingBuild::GetText(void*, int idx, const char** out_text) {
	const BuildPanel* bp = GWToolbox::instance().main_window->build_panel;
	if (idx >= (int)bp->BuildCount()) return false;
	*out_text = bp->BuildName(idx);
	return true;
}
HotkeyPingBuild::HotkeyPingBuild(CSimpleIni* ini, const char* section) : TBHotkey(ini, section) {
	index = ini ? ini->GetLongValue(section, "BuildIndex", 0) : 0;
}
void HotkeyPingBuild::Save(CSimpleIni* ini, const char* section) const {
	TBHotkey::Save(ini, section);
	ini->SetLongValue(section, "BuildIndex", index);
}
void HotkeyPingBuild::Description(char* buf, int bufsz) const {
	const BuildPanel* bp = GWToolbox::instance().main_window->build_panel;
	const char* buildname = bp->BuildName(index);
	if (buildname == nullptr) buildname = "<not found>";
	sprintf_s(buf, bufsz, "Ping build '%s'", buildname);
}
void HotkeyPingBuild::Draw() {
	const BuildPanel* bp = GWToolbox::instance().main_window->build_panel;
	ImGui::Combo("Build", &index, GetText, nullptr, bp->BuildCount());
}
void HotkeyPingBuild::Execute() {
	if (isLoading()) return;

	GWToolbox::instance().main_window->build_panel->Send(index);
}
