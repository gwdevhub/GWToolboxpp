#include "ChatCommands.h"

#include <algorithm>
#include <cctype>

#include <GWCA\Constants\Skills.h>
#include <GWCA\Managers\CameraMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\GuildMgr.h>
#include <GWCA\Managers\FriendListMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "PconPanel.h"
#include "GWToolbox.h"
#include "ChatLogger.h"

ChatCommands::ChatCommands() {
	
	move_forward = 0;
	move_side = 0;
	move_up = 0;
	cam_speed_ = DEFAULT_CAM_SPEED;

	skill_to_use = 0;
	skill_usage_delay = 1.0f;
	skill_timer = clock();

	GW::Chat().RegisterCommand(L"age2", ChatCommands::CmdAge2);
	GW::Chat().RegisterCommand(L"pcons", ChatCommands::CmdPcons);
	GW::Chat().RegisterCommand(L"dialog", ChatCommands::CmdDialog);
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

bool ChatCommands::ProcessMessage(LPMSG msg) {
	if (!GW::Cameramgr().GetCameraUnlock() || IsTyping()) return false;

	const DWORD keyA = 0x41;
	const DWORD keyD = 0x44;
	const DWORD keyS = 0x53;
	const DWORD keyW = 0x57;
	const DWORD keyX = 0x58;
	const DWORD keyZ = 0x5A;

	switch (msg->message) {
	case WM_KEYDOWN: {
		switch (msg->wParam) {
		case keyW: move_forward = 1; return true;
		case keyS: move_forward = -1; return true;
		case keyD: move_side = 1; return true;
		case keyA: move_side = -1; return true;
		case keyX: move_up = 1; return true;
		case keyZ: move_up = -1; return true;
		}
	}
	case WM_KEYUP: {
		switch (msg->wParam) {
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
	char buffer[30];
	DWORD second = GW::Map().GetInstanceTime() / 1000;
	sprintf_s(buffer, "%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	ChatLogger::Log(buffer);
	return true;
}

bool ChatCommands::CmdPcons(std::wstring& cmd, std::vector<std::wstring>& args) {
	PconPanel* pcons = GWToolbox::instance().main_window->pcon_panel;
	if (args.empty()) {
		pcons->ToggleEnable();
	} else { // we are ignoring parameters after the first
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"on") {
			pcons->SetEnabled(true);
		} else if (arg == L"off") {
			pcons->SetEnabled(false);
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
		GWToolbox::instance().main_window->ToggleVisible();
	} else {
		std::wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"age") {
			CmdAge2(cmd, args);
		} else if (arg == L"hide") {
			GWToolbox::instance().main_window->visible = false;
		} else if (arg == L"show") {
			GWToolbox::instance().main_window->visible = true;
		} else if (arg == L"reset") {
			//GWToolbox::instance().main_window->SetLocation(PointI(0, 0));
		} else if (arg == L"mini" || arg == L"minimize") {
			GWToolbox::instance().main_window->minimized = true;
		} else if (arg == L"maxi" || arg == L"maximize") {
			GWToolbox::instance().main_window->minimized = false;
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::instance().StartSelfDestruct();
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
			GWToolbox::instance().main_window->travel_panel->TravelFavorite(0);
		} else if (town == L"fav2") {
			GWToolbox::instance().main_window->travel_panel->TravelFavorite(1);
		} else if (town == L"fav3") {
			GWToolbox::instance().main_window->travel_panel->TravelFavorite(2);
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
			ChatCommands* self = GWToolbox::instance().chat_commands;
			if (arg1 == L"default") {
				self->cam_speed_ = self->DEFAULT_CAM_SPEED;
			} else {
				try {
					float speed = std::stof(arg1);
					self->cam_speed_ = speed;
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
			GWToolbox::instance().party_damage->WritePartyDamage();
		} else {
			// else print a specific party member
			std::wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"me") {
				GWToolbox::instance().party_damage->WriteOwnDamage();
			} else {
				try {
					long idx = std::stol(arg1);
					GWToolbox::instance().party_damage->WriteDamageOf(idx - 1);
				} catch (...) {}
			}
		}
	} else if (arg0 == L"reset") {
		GWToolbox::instance().party_damage->ResetDamage();
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
	ChatCommands* self = GWToolbox::instance().chat_commands;
	if (args.empty()) {
		self->skill_to_use = 0;
	} else if (args.size() == 1) {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"stop" || arg0 == L"off") {
			self->skill_to_use = 0;
		} else {
			try {
				int skill = std::stoi(args[0]);
				if (skill >= 0 && skill <= 8) self->skill_to_use = skill;
			} catch (...) {
				ChatLogger::Log("[Error] Invalid argument '%ls', please use an integer value", args[0].c_str());
			}
		}
	}
	return true;
}
