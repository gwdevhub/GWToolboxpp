#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA\Constants\Constants.h>
#include <GWCA\GameContainers\Array.h>

#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\ChatMgr.h>

#include <Keys.h>
#include <logger.h>
#include <GuiUtils.h>
#include <Modules\Resources.h>

void MissionsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);
	clickerTimer = TIMER_INIT();
	dropCoinsTimer = TIMER_INIT();
}
void MissionsWindow::Terminate() {
	for (TBMission* hotkey : hotkeys) {
		delete hotkey;
	}
}

void MissionsWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	// === hotkey panel ===
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		if (ImGui::Button("Create Hotkey...", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
			ImGui::OpenPopup("Create Hotkey");
		}
		if (ImGui::BeginPopup("Create Hotkey")) {
			if (ImGui::Selectable("Send Chat")) {
				hotkeys.push_back(new MissionSendChat(nullptr, nullptr));
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a message or command to chat");
			if (ImGui::Selectable("Use Item")) {
				hotkeys.push_back(new MissionUseItem(nullptr, nullptr));
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use an item from your inventory");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use or cancel a skill such as Recall or UA");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle a GWToolbox++ functionality such as clicker");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute a single task such as opening chests\nor reapplying lightbringer title");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Target a game entity by its ID");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move to a specific (x,y) coordinate");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a Dialog");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ping a build from the Build Panel");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load a team hero build from the Hero Build Panel");
			ImGui::EndPopup();
		}

		// === each hotkey ===
		block_hotkeys = false;
		for (unsigned int i = 0; i < hotkeys.size(); ++i) {
			TBMission::Op op = TBMission::Op_None;
			hotkeys[i]->Draw(&op);
			switch (op) {
			case TBMission::Op_None: break;
			case TBMission::Op_MoveUp:
				if (i > 0) std::swap(hotkeys[i], hotkeys[i - 1]);
				break;
			case TBMission::Op_MoveDown:
				if (i < hotkeys.size() - 1) {
					std::swap(hotkeys[i], hotkeys[i + 1]);
					// render the moved one and increase i
					TBMission::Op op2;
					hotkeys[i++]->Draw(&op2);
				}
				break;
			case TBMission::Op_Delete: {
				TBMission* hk = hotkeys[i];
				hotkeys.erase(hotkeys.begin() + i);
				delete hk;
				--i;
			}
									break;
			case TBMission::Op_BlockInput:
				block_hotkeys = true;
				break;

			default:
				break;
			}
		}
	}
	ImGui::End();
}

void MissionsWindow::DrawSettingInternal() {
	ToolboxWindow::DrawSettingInternal();
	ImGui::Checkbox("Show 'Active' checkbox in header", &TBMission::show_active_in_header);
	ImGui::Checkbox("Show 'Run' button in header", &TBMission::show_run_in_header);
}

void MissionsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

	TBMission::show_active_in_header = ini->GetBoolValue(Name(), "show_active_in_header", false);
	TBMission::show_run_in_header = ini->GetBoolValue(Name(), "show_run_in_header", false);

	// clear hotkeys from toolbox
	for (TBMission* hotkey : hotkeys) {
		delete hotkey;
	}
	hotkeys.clear();

	// then load again
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		TBMission* hk = TBMission::HotkeyFactory(ini, entry.pItem);
		if (hk) hotkeys.push_back(hk);
	}

	TBMission::hotkeys_changed = false;
}
void MissionsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
	ini->SetBoolValue(Name(), "show_active_in_header", TBMission::show_active_in_header);
	ini->SetBoolValue(Name(), "show_run_in_header", TBMission::show_run_in_header);

	if (TBMission::hotkeys_changed) {
		// clear hotkeys from ini
		CSimpleIni::TNamesDepend entries;
		ini->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			if (strncmp(entry.pItem, "hotkey-", 7) == 0) {
				ini->Delete(entry.pItem, nullptr);
			}
		}

		// then save again
		char buf[256];
		for (unsigned int i = 0; i < hotkeys.size(); ++i) {
			snprintf(buf, 256, "hotkey-%03d:%s", i, hotkeys[i]->Name());
			hotkeys[i]->Save(ini, buf);
		}
	}
}

bool MissionsWindow::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (GW::Chat::GetIsTyping())
		return false;

	long keyData = 0;
	switch (Message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		keyData = wParam;
		break;
	case WM_XBUTTONDOWN:
	case WM_MBUTTONDOWN:
		if (LOWORD(wParam) & MK_MBUTTON) keyData = VK_MBUTTON;
		if (LOWORD(wParam) & MK_XBUTTON1) keyData = VK_XBUTTON1;
		if (LOWORD(wParam) & MK_XBUTTON2) keyData = VK_XBUTTON2;
		break;
	case WM_XBUTTONUP:
	case WM_MBUTTONUP:
		// leave keydata to none, need to handle special case below
		break;
	default:
		break;
	}

	switch (Message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_XBUTTONDOWN:
	case WM_MBUTTONDOWN: {
		long modifier = 0;
		if (GetKeyState(VK_CONTROL) < 0)
			modifier |= ModKey_Control;
		if (GetKeyState(VK_SHIFT) < 0)
			modifier |= ModKey_Shift;
		if (GetKeyState(VK_MENU) < 0)
			modifier |= ModKey_Alt;

		bool triggered = false;
		for (TBMission* hk : hotkeys) {
			if (!block_hotkeys && hk->active
				&& !hk->pressed && keyData == hk->hotkey
				&& modifier == hk->modifier) {

				hk->pressed = true;
				hk->Execute();
				triggered = true;
			}
		}
		return triggered;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
		for (TBMission* hk : hotkeys) {
			if (hk->pressed && keyData == hk->hotkey) {
				hk->pressed = false;
			}
		}
		return false;

	case WM_XBUTTONUP:
		for (TBMission* hk : hotkeys) {
			if (hk->pressed && (hk->hotkey == VK_XBUTTON1 || hk->hotkey == VK_XBUTTON2)) {
				hk->pressed = false;
			}
		}
		return false;
	case WM_MBUTTONUP:
		for (TBMission* hk : hotkeys) {
			if (hk->pressed && hk->hotkey == VK_MBUTTON) {
				hk->pressed = false;
			}
		}
	default:
		return false;
	}
}


void MissionsWindow::Update(float delta) {
	if (clickerActive && TIMER_DIFF(clickerTimer) > 20) {
		clickerTimer = TIMER_INIT();
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.dx = 0;
		input.mi.dy = 0;
		input.mi.mouseData = 0;
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
		input.mi.time = 0;
		input.mi.dwExtraInfo = NULL;

		SendInput(1, &input, sizeof(INPUT));
	}

	if (dropCoinsActive && TIMER_DIFF(dropCoinsTimer) > 500) {
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
			dropCoinsTimer = TIMER_INIT();
			GW::Items::DropGold(1);
		}
	}

	// TODO rupt?
}
