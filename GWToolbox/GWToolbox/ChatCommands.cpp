#include "ChatCommands.h"

#include <algorithm>

#include <GWCA\CameraMgr.h>
#include <GWCA\ChatMgr.h>
#include <GWCA\ItemMgr.h>
#include <GWCA\GuildMgr.h>
#include <GWCA\FriendListMgr.h>
#include <GWCA\StoCMgr.h>
#include <GWCA\SkillbarMgr.h>

#include "PconPanel.h"
#include "GWToolbox.h"
#include "ChatLogger.h"

using namespace std;

ChatCommands::ChatCommands() {
	
	move_forward = 0;
	move_side = 0;
	move_up = 0;
	cam_speed_ = DEFAULT_CAM_SPEED;

	skill_to_use = 0;
	skill_usage_delay = 1.0f;
	skill_timer = TBTimer::init();

	AddCommand(L"age2", ChatCommands::CmdAge2);
	AddCommand(L"pcons", ChatCommands::CmdPcons);
	AddCommand(L"dialog", ChatCommands::CmdDialog);
	AddCommand(L"tb", ChatCommands::CmdTB);
	AddCommand(L"tp", ChatCommands::CmdTP);
	AddCommand(L"to", ChatCommands::CmdTP);
	AddCommand(L"travel", ChatCommands::CmdTP);
	AddCommand(L"zoom", ChatCommands::CmdZoom);
	AddCommand(L"camera", ChatCommands::CmdCamera);
	AddCommand(L"cam", ChatCommands::CmdCamera);
	AddCommand(L"damage", ChatCommands::CmdDamage);
	AddCommand(L"dmg", ChatCommands::CmdDamage);
	AddCommand(L"chest", ChatCommands::CmdChest);
	AddCommand(L"xunlai", ChatCommands::CmdChest);
	AddCommand(L"afk", ChatCommands::CmdAfk, false);
	AddCommand(L"target", ChatCommands::CmdTarget);
	AddCommand(L"tgt", ChatCommands::CmdTarget);

	AddCommand(L"useskill", ChatCommands::CmdUseSkill);
	AddCommand(L"skilluse", ChatCommands::CmdUseSkill);

	auto Get1stSegment = [](GWCA::StoC_Pak::P081* pak) -> const wchar_t* {
		for (size_t i = 0; pak->message[i] != 0; ++i) {
			if (pak->message[i] == 0x10A) return pak->message + i + 1;
		}
		return nullptr;
	};
	auto Get2ndSegment = [](GWCA::StoC_Pak::P081* pak) -> const wchar_t* {
		for (size_t i = 0; pak->message[i] != 0; ++i) {
			if (pak->message[i] == 0x10B) return pak->message + i + 1;
		}
		return nullptr;
	};

	auto GetNumericSegment = [](GWCA::StoC_Pak::P081* pak) -> DWORD {
		for (size_t i = 0; pak->message[i] != 0; ++i) {
			if (pak->message[i] == 0x10F) return (pak->message[i + 1] & 0xFF);
		}
		return 0;
	};

	auto ShouldIgnoreByAgentThatDropped = [](const wchar_t* agent_segment) -> bool {
		if (agent_segment == nullptr) return false;		// something went wrong, don't ignore
		if (agent_segment[0] == 0xBA9 && agent_segment[1] == 0x107) return false;
		return true;
	};

	auto ShouldIgnoreItem = [](const wchar_t* item_segment) -> bool {
		if (item_segment == nullptr) return false;		// something went wrong, don't ignore
		if (item_segment[0] == 0xA40) return false;		// don't ignore gold items
		if (   item_segment[0] == 0x108
			&& item_segment[1] == 0x10A
			&& item_segment[2] == 0x22D9
			&& item_segment[3] == 0xE7B8
			&& item_segment[4] == 0xE9DD
			&& item_segment[5] == 0x2322) return false;	// don't ignore ectos
		return true;
	};

	auto ShouldIgnore = [&](GWCA::StoC_Pak::P081* pak) -> bool {
		switch (pak->message[0]) {
		// ==== Messages not ignored ====
		case 0x108:	// player message
		case 0x7F2: // you drop item x
		case 0x7FC: // you pick up item y (note: item can be unassigned gold)
		case 0x807: // player joined the game
			return false;

		// ==== Ignored Messages ====
		case 0x7E0: // party shares gold
		case 0x7DF: // party shares gold ?
		case 0x7F6: // player x picks up item y (note: item can be unassigned gold)
		case 0x816: // you gain a skill point
		case 0x817: // player x gained a skill point
			return true;

		// ==== Monster drops: ignored unless gold/ecto for player ====
		case 0x7F0: { // monster/player x drops item y (no assignment)
			// first segment describes the agent who dropped
			return ShouldIgnoreByAgentThatDropped(Get1stSegment(pak)) && ShouldIgnoreItem(Get2ndSegment(pak));
		}
		case 0x7F1: { // monster x drops item y, your party assigns to player z
			// 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
			// <monster> is wchar_t id of several wchars
			// <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
			GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
			if (me && me->PlayerNumber != GetNumericSegment(pak)) return true;	// ignore drops not for the player
			return ShouldIgnoreItem(Get2ndSegment(pak));
		}

		case 0x8101:
			switch (pak->message[1]) {
			case 0x1868: return true; // teilah takes 10 festival tickets
			case 0x1867: return true; // stay where you are, nine rings is about to begin
			case 0x1869: return true; // big winner! 55 tickets
			case 0x186A: return true; // you win 40 tickets
			case 0x186B: return true; // you win 25 festival tickets
			case 0x186C: return true; // you win 15 festival tickets
			case 0x186D: return true; // did not win 9rings
			default: return false;
			}

		case 0x8102:
			switch (pak->message[1]) {
			case 0x4650: // skill has been updated for pvp
			case 0x4651: // a hero skill has been updated for pvp
				return true;

			case 0x223B: // a party won hall of heroes
			case 0x23E4: // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
			default:
				return false;
			}

		default: 
			//for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
			//printf("\n");
			return false;
		}
	};

	suppress_next_message = false;
	suppress_messages_active = false;
	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P081>([&](GWCA::StoC_Pak::P081* pak) -> bool {
		for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
		printf("\n");
		if (suppress_messages_active && ShouldIgnore(pak)) {
			suppress_next_message = true;
			return true;
		}
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P082>([&](GWCA::StoC_Pak::P082* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P085>([&](GWCA::StoC_Pak::P085* pak) -> bool {
		if (suppress_next_message) {
			suppress_next_message = false;
			return true;
		}
		return false;
	});
}

void ChatCommands::AddCommand(wstring cmd, Handler_t handler, bool override) {
	GWCA::Chat().RegisterCommand(cmd,
		[handler](std::wstring cmd, std::vector<std::wstring> args) {
		handler(args);
	}, override);
}

bool ChatCommands::ProcessMessage(LPMSG msg) {
	GWCA::CameraMgr cam = GWCA::Camera();
	float speed = 25.f;

	if (!cam.GetCameraUnlock() || IsTyping()) return false;

	switch (msg->message) {
	case WM_KEYDOWN: {
		OSHGui::Key key = (OSHGui::Key)msg->wParam;
		switch (key) {
		case OSHGui::Key::W: move_forward = 1; return true;
		case OSHGui::Key::S: move_forward = -1; return true;
		case OSHGui::Key::D: move_side = 1; return true;
		case OSHGui::Key::A: move_side = -1; return true;
		case OSHGui::Key::X: move_up = 1; return true;
		case OSHGui::Key::Z: move_up = -1; return true;
		}
	}
	case WM_KEYUP: {
		OSHGui::Key key = (OSHGui::Key)msg->wParam;
		switch (key) {
		case OSHGui::Key::W:
		case OSHGui::Key::S:
			move_forward = 0; 
			return true;
		case OSHGui::Key::D:
		case OSHGui::Key::A:
			move_side = 0;
			return true;
		case OSHGui::Key::X:
		case OSHGui::Key::Z:
			move_up = 0;
			return true;
		default:
			break;
		}
	}
	}

	return false;
}

void ChatCommands::UpdateUI() {
	GWCA::CameraMgr cam = GWCA::Camera();
	if (cam.GetCameraUnlock() && !IsTyping()) {
		cam.ForwardMovement(cam_speed_ * move_forward);
		cam.VerticalMovement(-cam_speed_ * move_up);
		cam.SideMovement(-cam_speed_ * move_side);
		cam.UpdateCameraPos();
	}
}

void ChatCommands::MainRoutine() {
	if (skill_to_use > 0 && skill_to_use < 9 
		&& GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Explorable
		&& TBTimer::diff(skill_timer) / 1000.0f > skill_usage_delay) {

		GWCA::SkillbarMgr manager = GWCA::Skillbar();
		GWCA::GW::Skillbar skillbar = manager.GetPlayerSkillbar();
		if (skillbar.IsValid()) {
			GWCA::GW::SkillbarSkill skill = skillbar.Skills[skill_to_use - 1]; // -1 to switch range [1,8] -> [0,7]
			if (skill.GetRecharge() == 0) {
				printf("using skill \n");
				manager.UseSkill(skill_to_use - 1, GWCA::Agents().GetTargetId());

				GWCA::GW::Skill skilldata = manager.GetSkillConstantData(skill.SkillId);
				skill_usage_delay = skilldata.Activation + skilldata.Aftercast + 1.0f; // one additional second to account for ping and to avoid spamming in case of bad target
				skill_timer = TBTimer::init();
			}
		}
	}
}

wstring ChatCommands::GetLowerCaseArg(vector<wstring> args, size_t index) {
	if (index >= args.size()) return L"";
	wstring arg = args[index];
	std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
	return arg;
}

void ChatCommands::CmdAge2(vector<wstring>) {
	wchar_t buffer[30];
	DWORD second = GWCA::Map().GetInstanceTime() / 1000;
	wsprintf(buffer, L"%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	ChatLogger::Log(buffer);
}

void ChatCommands::CmdPcons(vector<wstring> args) {
	PconPanel& pcons = GWToolbox::instance().main_window().pcon_panel();
	if (args.size() == 0) {
		pcons.ToggleActive();
	} else { // we are ignoring parameters after the first
		wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"on") {
			pcons.SetActive(true);
		} else if (arg == L"off") {
			pcons.SetActive(false);
		} else {
			ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use /pcons [|on|off]", args[0].c_str());
		}
	}
}

void ChatCommands::CmdDialog(vector<wstring> args) {
	if (args.size() == 0) {
		ChatLogger::LogF(L"[Error] Please provide an integer or hex argument");
	} else {
		try {
			long id = std::stol(args[0], 0, 0);
			GWCA::Agents().Dialog(id);
			ChatLogger::LogF(L"Sent Dialog 0x%X", id);
		} catch (...) {
			ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use an integer or hex value", args[0].c_str());
		}
	}
}

void ChatCommands::CmdChest(vector<wstring> args) {
	if (GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Outpost) {
		GWCA::Items().OpenXunlaiWindow();
	}
}

void ChatCommands::CmdTB(vector<wstring> args) {
	if (args.size() == 0) {
		GWToolbox::instance().main_window().ToggleHidden();
	} else {
		wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"age") {
			CmdAge2(args);
		} else if (arg == L"hide") {
			GWToolbox::instance().main_window().SetHidden(true);
		} else if (arg == L"show") {
			GWToolbox::instance().main_window().SetHidden(false);
		} else if (arg == L"reset") {
			GWToolbox::instance().main_window().SetLocation(PointI(0, 0));
		} else if (arg == L"mini" || arg == L"minimize") {
			GWToolbox::instance().main_window().SetMinimized(true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			GWToolbox::instance().main_window().SetMinimized(false);
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::instance().StartSelfDestruct();
		}
	}
}

void ChatCommands::CmdTP(vector<wstring> args) {
	if (args.size() == 0) {
		ChatLogger::Log(L"[Error] Please provide an argument");
	} else {
		wstring town = GetLowerCaseArg(args, 0);

		GwConstants::District district = GwConstants::District::Current;
		int district_number = 0;
		if (args.size() >= 2) {
			wstring dis = GetLowerCaseArg(args, 1);
			if (dis == L"ae1") {
				district = GwConstants::District::American;
				district_number = 1;
			} else if (dis == L"ee1") {
				district = GwConstants::District::EuropeEnglish;
				district_number = 1;
			} else if (dis == L"eg1" || dis == L"dd1") {  // dd1 is german: deutche dist
				district = GwConstants::District::EuropeGerman;
				district_number = 1;
			} else if (dis == L"int") {
				district = GwConstants::District::International;
			} else {
				ChatLogger::LogF(L"Invalid district '%ls'", dis.c_str());
			}
		}

		if (town == L"toa") {
			GWCA::Map().Travel(GwConstants::MapID::Temple_of_the_Ages, district, district_number);
		} else if (town == L"doa") {
			GWCA::Map().Travel(GwConstants::MapID::Domain_of_Anguish, district, district_number);
		} else if (town == L"kamadan" || town == L"kama") {
			GWCA::Map().Travel(GwConstants::MapID::Kamadan_Jewel_of_Istan_outpost, district, district_number);
		} else if (town == L"embark") {
			GWCA::Map().Travel(GwConstants::MapID::Embark_Beach, district, district_number);
		} else if (town == L"vlox" || town == L"vloxs") {
			GWCA::Map().Travel(GwConstants::MapID::Vloxs_Falls, district, district_number);
		} else if (town == L"gadd" || town == L"gadds") {
			GWCA::Map().Travel(GwConstants::MapID::Gadds_Encampment_outpost, district, district_number);
		} else if (town == L"urgoz") {
			GWCA::Map().Travel(GwConstants::MapID::Urgozs_Warren, district, district_number);
		} else if (town == L"deep") {
			GWCA::Map().Travel(GwConstants::MapID::The_Deep, district, district_number);
		} else if (town == L"fav1") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(0);
		} else if (town == L"fav2") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(1);
		} else if (town == L"fav3") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(2);
		} else if (town == L"gh") {
			GWCA::Guild().TravelGH();
		}
	}
}

void ChatCommands::CmdZoom(vector<wstring> args) {
	if (args.empty()) {
		GWCA::Camera().SetMaxDist(750.0f);
	} else {
		try {
			long distance = std::stol(args[0]);
			GWCA::Camera().SetMaxDist(static_cast<float>(distance));
		} catch (...) {
			ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use an integer value", args[0].c_str());
		}
	}
}

void ChatCommands::CmdCamera(vector<wstring> args) {
	if (args.empty()) {
		GWCA::Camera().UnlockCam(false);
	} else {
		wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"lock") {
			GWCA::Camera().UnlockCam(false);
		} else if (arg0 == L"unlock") {
			GWCA::Camera().UnlockCam(true);
			ChatLogger::Log(L"Use W,A,S,D,X,Z for camera movement");
		} else if (arg0 == L"fog") {
			wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"on") {
				GWCA::Camera().SetFog(true);
			} else if (arg1 == L"off") {
				GWCA::Camera().SetFog(false);
			}
		} else if (arg0 == L"fov") {
			wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"default") {
				GWCA::Camera().SetFieldOfView(1.308997f);
			} else {
				try {
					float fovnew = std::stof(arg1);
					GWCA::Camera().SetFieldOfView(fovnew);
					ChatLogger::LogF(L"Field of View is %f", fovnew);
				} catch (...) {
					ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use a float value", args[1].c_str());
				}
			}
		} else if (arg0 == L"speed") {
			wstring arg1 = GetLowerCaseArg(args, 1);
			ChatCommands& self = GWToolbox::instance().chat_commands();
			if (arg1 == L"default") {
				self.cam_speed_ = self.DEFAULT_CAM_SPEED;
			} else {
				try {
					float speed = std::stof(arg1);
					self.cam_speed_ = speed;
					ChatLogger::LogF(L"Camera speed is now %f", speed);
				} catch (...) {
					ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use a float value", args[1].c_str());
				}
			}
		} else {
			ChatLogger::Log(L"[Error] Invalid argument.");
		}
	}
}

void ChatCommands::CmdDamage(vector<wstring> args) {
	if (!args.empty()) {
		wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"print" || arg0 == L"report") {
			GWToolbox::instance().party_damage().WriteChat();
		} else if (arg0 == L"reset") {
			GWToolbox::instance().party_damage().Reset();
		}
	}
}

void ChatCommands::CmdAfk(vector<wstring> args) {
	GWCA::FriendList().SetFriendListStatus(GwConstants::OnlineStatus::AWAY);
}

void ChatCommands::CmdTarget(vector<wstring> args) {
	if (!args.empty()) {
		wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"closest" || arg0 == L"nearest") {
			// target nearest agent
			GWCA::GW::AgentArray agents = GWCA::Agents().GetAgentArray();
			if (!agents.valid()) return;

			GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
			if (me == nullptr) return;

			unsigned long distance = GwConstants::SqrRange::Compass;
			int closest = -1;

			for (size_t i = 0; i < agents.size(); ++i) {
				GWCA::GW::Agent* agent = agents[i];
				if (agent == nullptr) continue;
				if (agent->PlayerNumber != me->PlayerNumber) {
					unsigned long newDistance = GWCA::Agents().GetSqrDistance(me->pos, agents[i]->pos);
					if (newDistance < distance) {
						closest = i;
						distance = newDistance;
					}
				}
			}
			if (closest > 0) {
				GWCA::Agents().ChangeTarget(agents[closest]);
			}

		} else if (arg0 == L"getid") {
			GWCA::GW::Agent* target = GWCA::Agents().GetTarget();
			if (target == nullptr) {
				ChatLogger::Log(L"No target selected!");
			} else {
				ChatLogger::LogF(L"Target model id (PlayerNumber) is %d", target->PlayerNumber);
			}
		} else if (arg0 == L"getpos") {
			GWCA::GW::Agent* target = GWCA::Agents().GetTarget();
			if (target == nullptr) {
				ChatLogger::Log(L"No target selected!");
			} else {
				ChatLogger::LogF(L"Target coordinates are (%f, %f)", target->X, target->Y);
			}
		}
	}
}

void ChatCommands::CmdUseSkill(vector<wstring> args) {
	ChatCommands& self = GWToolbox::instance().chat_commands();
	if (args.empty()) {
		self.skill_to_use = 0;
	} else if (args.size() == 1) {
		wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"stop" || arg0 == L"off") {
			self.skill_to_use = 0;
		} else {
			try {
				int skill = std::stoi(args[0]);
				if (skill >= 0 && skill <= 8) self.skill_to_use = skill;
			} catch (...) {
				ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use an integer value", args[0].c_str());
			}
		}
	}
}
