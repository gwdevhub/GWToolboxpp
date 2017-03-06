#include "ChatCommands.h"

#include <algorithm>
#include <cctype>

#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA\Constants\Skills.h>
#include <GWCA\Managers\CameraMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\GuildMgr.h>
#include <GWCA\Managers\FriendListMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include <Windows\MainWindow.h>
#include <Windows\TimerWindow.h>
#include <Windows\BondsWindow.h>
#include <Windows\HealthWindow.h>
#include <Windows\DistanceWindow.h>
#include <Windows\PartyDamage.h>
#include <Windows\Minimap\Minimap.h>
#include <Windows\ClockWindow.h>
#include <Windows\NotePadWindow.h>

#include "Panels\PconPanel.h"
#include "Panels\HotkeyPanel.h"
#include "Panels\TravelPanel.h"
#include "Panels\BuildPanel.h"
#include "Panels\DialogPanel.h"
#include "Panels\InfoPanel.h"
#include "Panels\MaterialsPanel.h"
#include "Panels\SettingsPanel.h"
#include "GWToolbox.h"
#include "ChatLogger.h"

void ChatCommands::Initialize() {
	ToolboxModule::Initialize();

	GW::Chat().RegisterCommand(L"age2", ChatCommands::CmdAge2);
	GW::Chat().RegisterCommand(L"pcons", ChatCommands::CmdPcons);
	GW::Chat().RegisterCommand(L"dialog", ChatCommands::CmdDialog);
	GW::Chat().RegisterCommand(L"show", ChatCommands::CmdShow);
	GW::Chat().RegisterCommand(L"tb", ChatCommands::CmdTB);
	GW::Chat().RegisterCommand(L"tp", ChatCommands::CmdTP);
	GW::Chat().RegisterCommand(L"to", ChatCommands::CmdTP);
	GW::Chat().RegisterCommand(L"travel", ChatCommands::CmdTP);
	GW::Chat().RegisterCommand(L"zoom", ChatCommands::CmdZoom);
	GW::Chat().RegisterCommand(L"camera", ChatCommands::CmdCamera);
	GW::Chat().RegisterCommand(L"cam", ChatCommands::CmdCamera);
	GW::Chat().RegisterCommand(L"damage", ChatCommands::CmdDamage);
	GW::Chat().RegisterCommand(L"dmg", ChatCommands::CmdDamage);
	GW::Chat().RegisterCommand(L"chest", ChatCommands::CmdChest);
	GW::Chat().RegisterCommand(L"xunlai", ChatCommands::CmdChest);
	GW::Chat().RegisterCommand(L"afk", ChatCommands::CmdAfk);
	GW::Chat().RegisterCommand(L"target", ChatCommands::CmdTarget);
	GW::Chat().RegisterCommand(L"tgt", ChatCommands::CmdTarget);
	GW::Chat().RegisterCommand(L"useskill", ChatCommands::CmdUseSkill);
	GW::Chat().RegisterCommand(L"skilluse", ChatCommands::CmdUseSkill);
}

bool ChatCommands::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!GW::Cameramgr().GetCameraUnlock() || IsTyping()) return false;

	const DWORD keyA = 0x41;
	const DWORD keyD = 0x44;
	const DWORD keyS = 0x53;
	const DWORD keyW = 0x57;
	const DWORD keyX = 0x58;
	const DWORD keyZ = 0x5A;

	switch (Message) {
	case WM_KEYDOWN: {
		switch (wParam) {
		case keyW: move_forward = 1; return true;
		case keyS: move_forward = -1; return true;
		case keyD: move_side = 1; return true;
		case keyA: move_side = -1; return true;
		case keyX: move_up = 1; return true;
		case keyZ: move_up = -1; return true;
		}
	}
	case WM_KEYUP: {
		switch (wParam) {
		case keyW:
		case keyS:
			move_forward = 0; 
			return true;
		case keyD:
		case keyA:
			move_side = 0;
			return true;
		case keyX:
		case keyZ:
			move_up = 0;
			return true;
		default:
			break;
		}
	}
	}

	return false;
}

void ChatCommands::Update() {
	GW::CameraMgr cam = GW::Cameramgr();
	if (cam.GetCameraUnlock() && !IsTyping()) {
		cam.ForwardMovement(cam_speed_ * move_forward);
		cam.VerticalMovement(-cam_speed_ * move_up);
		cam.SideMovement(-cam_speed_ * move_side);
		cam.UpdateCameraPos();
	}

	if (skill_to_use > 0 && skill_to_use < 9 
		&& GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& clock() - skill_timer / 1000.0f > skill_usage_delay) {

		GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
		if (skillbar.IsValid()) {
			GW::SkillbarSkill skill = skillbar.Skills[skill_to_use - 1]; // -1 to switch range [1,8] -> [0,7]
			if (skill.GetRecharge() == 0) {
				GW::Skillbarmgr().UseSkill(skill_to_use - 1, GW::Agents().GetTargetId());

				GW::Skill skilldata = GW::Skillbarmgr().GetSkillConstantData(skill.SkillId);
				skill_usage_delay = skilldata.Activation + skilldata.Aftercast + 1.0f; // one additional second to account for ping and to avoid spamming in case of bad target
				skill_timer = clock();
			}
		}
	}
}

std::wstring ChatCommands::GetLowerCaseArg(std::vector<std::wstring> args, size_t index) {
	if (index >= args.size()) return L"";
	std::wstring arg = args[index];
	std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
	return arg;
}

bool ChatCommands::CmdAge2(std::wstring& cmd, std::vector<std::wstring>& args) {
	char buffer[32];
	DWORD second = GW::Map().GetInstanceTime() / 1000;
	sprintf_s(buffer, "%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	ChatLogger::Log(buffer);
	return true;
}

bool ChatCommands::CmdPcons(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		PconPanel::Instance().ToggleEnable();
	} else { // we are ignoring parameters after the first
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"on") {
			PconPanel::Instance().SetEnabled(true);
		} else if (arg == L"off") {
			PconPanel::Instance().SetEnabled(false);
		} else {
			ChatLogger::Log("[Error] Invalid argument '%ls', please use /pcons [|on|off]", args[0].c_str());
		}
	}
	return true;
}

bool ChatCommands::CmdDialog(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		ChatLogger::Log("[Error] Please provide an integer or hex argument");
	} else {
		try {
			long id = std::stol(args[0], 0, 0);
			GW::Agents().Dialog(id);
			ChatLogger::Log("Sent Dialog 0x%X", id);
		} catch (...) {
			ChatLogger::Log("[Error] Invalid argument '%ls', please use an integer or hex value", args[0].c_str());
		}
	}
	return true;
}

bool ChatCommands::CmdChest(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		GW::Items().OpenXunlaiWindow();
	}
	return true;
}

bool ChatCommands::CmdTB(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		MainWindow::Instance().ToggleVisible();
	} else {
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"age") {
			CmdAge2(cmd, args);
		} else if (arg == L"hide") {
			MainWindow::Instance().visible = false;
		} else if (arg == L"show") {
			MainWindow::Instance().visible = true;
		} else if (arg == L"reset") {
			ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
			ImGui::SetWindowPos(SettingsPanel::Instance().Name(), ImVec2(50.0f, 50.0f));
		} else if (arg == L"mini" || arg == L"minimize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::Instance().StartSelfDestruct();
		}
	}
	return true;
}

bool ChatCommands::CmdShow(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		MainWindow::Instance().visible = true;
	} else {
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"mainwindow" || arg == L"toolbox") {
			MainWindow::Instance().visible = true;
		} else if (arg == L"pcons") {
			PconPanel::Instance().visible = true;
		} else if (arg == L"hotkeys") {
			HotkeyPanel::Instance().visible = true;
		} else if (arg == L"builds") {
			BuildPanel::Instance().visible = true;
		} else if (arg == L"travel") {
			TravelPanel::Instance().visible = true;
		} else if (arg == L"dialogs") {
			DialogPanel::Instance().visible = true;
		} else if (arg == L"info") {
			InfoPanel::Instance().visible = true;
		} else if (arg == L"materials") {
			MaterialsPanel::Instance().visible = true;
		} else if (arg == L"settings") {
			SettingsPanel::Instance().visible = true;
		} else if (arg == L"timer") {
			TimerWindow::Instance().visible = true;
		} else if (arg == L"health") {
			HealthWindow::Instance().visible = true;
		} else if (arg == L"distance") {
			DistanceWindow::Instance().visible = true;
		} else if (arg == L"minimap") {
			Minimap::Instance().visible = true;
		} else if (arg == L"damage") {
			PartyDamage::Instance().visible = true;
		} else if (arg == L"bonds") {
			BondsWindow::Instance().visible = true;
		} else if (arg == L"clock") {
			ClockWindow::Instance().visible = true;
		} else if (arg == L"notepad") {
			NotePadWindow::Instance().visible = true;
		}
	}
	return true;
}

bool ChatCommands::CmdHide(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		MainWindow::Instance().visible = false;
	} else {
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"mainwindow" || arg == L"toolbox") {
			MainWindow::Instance().visible = false;
		} else if (arg == L"pcons") {
			PconPanel::Instance().visible = false;
		} else if (arg == L"hotkeys") {
			HotkeyPanel::Instance().visible = false;
		} else if (arg == L"builds") {
			BuildPanel::Instance().visible = false;
		} else if (arg == L"travel") {
			TravelPanel::Instance().visible = false;
		} else if (arg == L"dialogs") {
			DialogPanel::Instance().visible = false;
		} else if (arg == L"info") {
			InfoPanel::Instance().visible = false;
		} else if (arg == L"materials") {
			MaterialsPanel::Instance().visible = false;
		} else if (arg == L"settings") {
			SettingsPanel::Instance().visible = false;
		} else if (arg == L"timer") {
			TimerWindow::Instance().visible = false;
		} else if (arg == L"health") {
			HealthWindow::Instance().visible = false;
		} else if (arg == L"distance") {
			DistanceWindow::Instance().visible = false;
		} else if (arg == L"minimap") {
			Minimap::Instance().visible = false;
		} else if (arg == L"damage") {
			PartyDamage::Instance().visible = false;
		} else if (arg == L"bonds") {
			BondsWindow::Instance().visible = false;
		} else if (arg == L"clock") {
			ClockWindow::Instance().visible = false;
		} else if (arg == L"notepad") {
			NotePadWindow::Instance().visible = false;
		}
	}
	return true;
}

bool ChatCommands::CmdTP(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		ChatLogger::Log("[Error] Please provide an argument");
	} else {
		std::wstring town = GetLowerCaseArg(args, 0);

		GW::Constants::District district = GW::Constants::District::Current;
		int district_number = 0;
		if (args.size() >= 2) {
			std::wstring dis = GetLowerCaseArg(args, 1);
			if (dis == L"ae1") {
				district = GW::Constants::District::American;
				district_number = 1;
			} else if (dis == L"ee1") {
				district = GW::Constants::District::EuropeEnglish;
				district_number = 1;
			} else if (dis == L"eg1" || dis == L"dd1") {  // dd1 is german: deutche dist
				district = GW::Constants::District::EuropeGerman;
				district_number = 1;
			} else if (dis == L"int") {
				district = GW::Constants::District::International;
			} else {
				ChatLogger::Log("Invalid district '%ls'", dis.c_str());
			}
		}

		if (town == L"toa") {
			GW::Map().Travel(GW::Constants::MapID::Temple_of_the_Ages, district, district_number);
		} else if (town == L"doa") {
			GW::Map().Travel(GW::Constants::MapID::Domain_of_Anguish, district, district_number);
		} else if (town == L"kamadan" || town == L"kama") {
			GW::Map().Travel(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost, district, district_number);
		} else if (town == L"embark") {
			GW::Map().Travel(GW::Constants::MapID::Embark_Beach, district, district_number);
		} else if (town == L"vlox" || town == L"vloxs") {
			GW::Map().Travel(GW::Constants::MapID::Vloxs_Falls, district, district_number);
		} else if (town == L"gadd" || town == L"gadds") {
			GW::Map().Travel(GW::Constants::MapID::Gadds_Encampment_outpost, district, district_number);
		} else if (town == L"urgoz") {
			GW::Map().Travel(GW::Constants::MapID::Urgozs_Warren, district, district_number);
		} else if (town == L"deep") {
			GW::Map().Travel(GW::Constants::MapID::The_Deep, district, district_number);
		} else if (town == L"fav1") {
			TravelPanel::Instance().TravelFavorite(0);
		} else if (town == L"fav2") {
			TravelPanel::Instance().TravelFavorite(1);
		} else if (town == L"fav3") {
			TravelPanel::Instance().TravelFavorite(2);
		} else if (town == L"gh") {
			GW::Guildmgr().TravelGH();
		}
	}
	return true;
}

bool ChatCommands::CmdZoom(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		GW::Cameramgr().SetMaxDist(750.0f);
	} else {
		try {
			long distance = std::stol(args[0]);
			if (distance > 0) {
				GW::Cameramgr().SetMaxDist(static_cast<float>(distance));
			} else {
				ChatLogger::Log("[Error] Invalid argument '%ls', please use a positive integer value", args[0].c_str());
			}
			
		} catch (...) {
			ChatLogger::Log("[Error] Invalid argument '%ls', please use an integer value", args[0].c_str());
		}
	}
	return true;
}

bool ChatCommands::CmdCamera(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		GW::Cameramgr().UnlockCam(false);
	} else {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"lock") {
			GW::Cameramgr().UnlockCam(false);
		} else if (arg0 == L"unlock") {
			GW::Cameramgr().UnlockCam(true);
			ChatLogger::Log("Use W,A,S,D,X,Z for camera movement");
		} else if (arg0 == L"fog") {
			std::wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"on") {
				GW::Cameramgr().SetFog(true);
			} else if (arg1 == L"off") {
				GW::Cameramgr().SetFog(false);
			}
		} else if (arg0 == L"fov") {
			if (args.size() == 1) {
				GW::Cameramgr().SetFieldOfView(1.308997f);
			} else {
				std::wstring arg1 = GetLowerCaseArg(args, 1);
				if (arg1 == L"default") {
					GW::Cameramgr().SetFieldOfView(1.308997f);
				} else {
					try {
						float fovnew = std::stof(arg1);
						if (fovnew > 0) {
							GW::Cameramgr().SetFieldOfView(fovnew);
							ChatLogger::Log("Field of View is %f", fovnew);
						} else {
							ChatLogger::Log("[Error] Invalid argument '%ls', please use a positive value", args[1].c_str());
						}
					} catch (...) {
						ChatLogger::Log("[Error] Invalid argument '%ls', please use a float value", args[1].c_str());
					}
				}
			}
		} else if (arg0 == L"speed") {
			std::wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"default") {
				Instance().cam_speed_ = Instance().DEFAULT_CAM_SPEED;
			} else {
				try {
					float speed = std::stof(arg1);
					Instance().cam_speed_ = speed;
					ChatLogger::Log("Camera speed is now %f", speed);
				} catch (...) {
					ChatLogger::Log("[Error] Invalid argument '%ls', please use a float value", args[1].c_str());
				}
			}
		} else {
			ChatLogger::Log("[Error] Invalid argument.");
		}
	}
	return true;
}

bool ChatCommands::CmdDamage(std::wstring& cmd, std::vector<std::wstring>& args) {
	std::wstring arg0 = GetLowerCaseArg(args, 0);
	if (args.empty() || arg0 == L"print" || arg0 == L"report") {
		if (args.size() <= 1) {
			// if no argument or just one, print the whole party
			PartyDamage::Instance().WritePartyDamage();
		} else {
			// else print a specific party member
			std::wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"me") {
				PartyDamage::Instance().WriteOwnDamage();
			} else {
				try {
					long idx = std::stol(arg1);
					PartyDamage::Instance().WriteDamageOf(idx - 1);
				} catch (...) {}
			}
		}
	} else if (arg0 == L"reset") {
		PartyDamage::Instance().ResetDamage();
	}
	return true;
}

bool ChatCommands::CmdAfk(std::wstring& cmd, std::vector<std::wstring>& args) {
	GW::FriendListmgr().SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
	return false;
}

bool ChatCommands::CmdTarget(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (!args.empty()) {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"closest" || arg0 == L"nearest") {
			// target nearest agent
			GW::AgentArray agents = GW::Agents().GetAgentArray();
			if (!agents.valid()) return true;

			GW::Agent* me = GW::Agents().GetPlayer();
			if (me == nullptr) return true;

			float distance = GW::Constants::SqrRange::Compass;
			int closest = -1;

			for (size_t i = 0; i < agents.size(); ++i) {
				GW::Agent* agent = agents[i];
				if (agent == nullptr) continue;
				if (agent->PlayerNumber != me->PlayerNumber) {
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

		} else if (arg0 == L"getid") {
			GW::Agent* target = GW::Agents().GetTarget();
			if (target == nullptr) {
				ChatLogger::Log("No target selected!");
			} else {
				ChatLogger::Log("Target model id (PlayerNumber) is %d", target->PlayerNumber);
			}
		} else if (arg0 == L"getpos") {
			GW::Agent* target = GW::Agents().GetTarget();
			if (target == nullptr) {
				ChatLogger::Log("No target selected!");
			} else {
				ChatLogger::Log("Target coordinates are (%f, %f)", target->X, target->Y);
			}
		}
	}
	return true;
}

bool ChatCommands::CmdUseSkill(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		Instance().skill_to_use = 0;
	} else if (args.size() == 1) {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"stop" || arg0 == L"off") {
			Instance().skill_to_use = 0;
		} else {
			try {
				int skill = std::stoi(args[0]);
				if (skill >= 0 && skill <= 8) Instance().skill_to_use = skill;
			} catch (...) {
				ChatLogger::Log("[Error] Invalid argument '%ls', please use an integer value", args[0].c_str());
			}
		}
	}
	return true;
}
