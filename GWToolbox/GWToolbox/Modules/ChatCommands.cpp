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
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include <GuiUtils.h>
#include "GWToolbox.h"
#include <Windows\MainWindow.h>
#include <Windows\SettingsWindow.h>
#include <Windows\TravelWindow.h>
#include <Windows\BuildsWindow.h>
#include <Widgets\PartyDamage.h>

#include <logger.h>

void ChatCommands::DrawHelp() {
	ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
	ImGui::TextDisabled("Below, <xyz> denotes an argument, use an appropriate value without the quotes.\n"
		"(on|off) denotes a mandatory argument, in this case 'on' or 'off'.\n"
		"[on|off] denotes an optional argument, in this case nothing, 'on' or 'off'.");

	//ImGui::BulletText("'/afk' to /sit and set your status to 'Away'.");
	ImGui::Bullet(); ImGui::Text("'/age2' prints the instance time to chat.");
	ImGui::Bullet(); ImGui::Text("'/borderless [on|off]' toggles, enables or disables borderless window.");
	ImGui::Bullet(); ImGui::Text("'/camera (lock|unlock)' to lock or unlock the camera.");
	ImGui::Bullet(); ImGui::Text("'/camera fog (on|off)' sets game fog effect on or off.");
	ImGui::Bullet(); ImGui::Text("'/camera fov <value>' sets the field of view. '/camera fov' resets to default.");
	ImGui::Bullet(); ImGui::Text("'/camera speed <value>' sets the unlocked camera speed.");
	ImGui::Bullet(); ImGui::Text("'/chest' opens xunlai in outposts and locked chests in explorables.");
	ImGui::Bullet(); ImGui::Text("'/damage' or '/dmg' to print party damage to chat.\n"
		"'/damage me' sends your own damage only.\n"
		"'/damage <number>' sends the damage of a party member (e.g. '/damage 3').\n"
		"'/damage reset' resets the damage in party window.");
	ImGui::Bullet(); ImGui::Text("'/dialog <id>' sends a dialog.");
	ImGui::Bullet(); ImGui::Text("'/flag [all|<number>]' to flag a hero in the minimap (same a using the buttons by the minimap).");
	ImGui::Bullet(); ImGui::Text("'/hide <name>' closes the window or widget titled <name>.");
	ImGui::Bullet(); ImGui::Text("'/pcons [on|off]' toggles, enables or disables pcons.");
	ImGui::Bullet(); ImGui::Text("'/show <name>' opens the window or widget titled <name>.");
	ImGui::Bullet(); ImGui::Text("'/target closest' to target the closest agent to you.");
	ImGui::Bullet(); ImGui::Text("'/tb <name>' toggles the window or widget titled <name>.");
	ImGui::Bullet(); ImGui::Text("'/tb reset' moves Toolbox and Settings window to the top-left corner.");
	ImGui::Bullet(); ImGui::Text("'/tb quit' or '/tb exit' completely closes toolbox and all its windows.");
	ImGui::Bullet(); ImGui::Text("'/travel <town> [dis]', '/tp <town> [dis]' or '/to <town> [dis]' to travel to a destination. \n"
		"<town> can be any of: doa, kamadan/kama, embark, vlox, gadds, urgoz, deep, gtob, fav1, fav2, fav3.\n"
		"[dis] can be any of: ae, ae1, ee, eg, int, etc");
	ImGui::Bullet(); ImGui::Text("'/useskill <skill>' starts using the skill on recharge. "
		"Use the skill number instead of <skill> (e.g. '/useskill 5'). "
		"Use empty '/useskill', '/useskill 0' or '/useskill stop' to stop.");
	ImGui::Bullet(); ImGui::Text("'/zoom <value>' to change the maximum zoom to the value. "
		"use empty '/zoom' to reset to the default value of 750.");
	ImGui::Bullet(); ImGui::Text("'/load [build template|build name]' loads a build. The build name must be between \"\"\" if it contains spaces.");
}

void ChatCommands::Initialize() {
	ToolboxModule::Initialize();

	GW::Chat::CreateCommand(L"age2", ChatCommands::CmdAge2);
	GW::Chat::CreateCommand(L"dialog", ChatCommands::CmdDialog);
	GW::Chat::CreateCommand(L"show", ChatCommands::CmdShow);
	GW::Chat::CreateCommand(L"hide", ChatCommands::CmdHide);
	GW::Chat::CreateCommand(L"tb", ChatCommands::CmdTB);
	GW::Chat::CreateCommand(L"tp", ChatCommands::CmdTP);
	GW::Chat::CreateCommand(L"to", ChatCommands::CmdTP);
	GW::Chat::CreateCommand(L"travel", ChatCommands::CmdTP);
	GW::Chat::CreateCommand(L"zoom", ChatCommands::CmdZoom);
	GW::Chat::CreateCommand(L"camera", ChatCommands::CmdCamera);
	GW::Chat::CreateCommand(L"cam", ChatCommands::CmdCamera);
	GW::Chat::CreateCommand(L"damage", ChatCommands::CmdDamage);
	GW::Chat::CreateCommand(L"dmg", ChatCommands::CmdDamage);
	GW::Chat::CreateCommand(L"chest", ChatCommands::CmdChest);
	GW::Chat::CreateCommand(L"xunlai", ChatCommands::CmdChest);
	GW::Chat::CreateCommand(L"afk", ChatCommands::CmdAfk);
	GW::Chat::CreateCommand(L"target", ChatCommands::CmdTarget);
	GW::Chat::CreateCommand(L"tgt", ChatCommands::CmdTarget);
	GW::Chat::CreateCommand(L"useskill", ChatCommands::CmdUseSkill);
	GW::Chat::CreateCommand(L"skilluse", ChatCommands::CmdUseSkill);
	GW::Chat::CreateCommand(L"scwiki", ChatCommands::CmdSCWiki);
	GW::Chat::CreateCommand(L"load", ChatCommands::CmdLoad);
}

bool ChatCommands::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!GW::CameraMgr::GetCameraUnlock() || IsTyping()) return false;

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
	if (GW::CameraMgr::GetCameraUnlock() && !IsTyping()) {
		GW::CameraMgr::ForwardMovement(cam_speed_ * move_forward);
		GW::CameraMgr::VerticalMovement(-cam_speed_ * move_up);
		GW::CameraMgr::SideMovement(-cam_speed_ * move_side);
		GW::CameraMgr::UpdateCameraPos();
	}

	if (skill_to_use > 0 && skill_to_use < 9 
		&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& clock() - skill_timer / 1000.0f > skill_usage_delay) {

		GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
		if (skillbar.IsValid()) {
			GW::SkillbarSkill skill = skillbar.Skills[skill_to_use - 1]; // -1 to switch range [1,8] -> [0,7]
			if (skill.GetRecharge() == 0) {
				int slot = skill_to_use - 1;
				GW::GameThread::Enqueue([slot] {
					GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId());
				});

				GW::Skill skilldata = GW::SkillbarMgr::GetSkillConstantData(skill.SkillId);
				skill_usage_delay = skilldata.Activation + skilldata.Aftercast + 1.0f; // one additional second to account for ping and to avoid spamming in case of bad target
				skill_timer = clock();
			}
		}
	}
}

bool ChatCommands::ReadTemplateFile(std::wstring path, char *buff, size_t buffSize) {
	HANDLE fileHandle = CreateFileW(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		Log::Error("Failed openning file '%S'", path.c_str());
		return false;
	}

	DWORD fileSize = GetFileSize(fileHandle, NULL);
	if (fileSize >= buffSize) {
		Log::Error("Buffer size too small, file size is %d", fileSize);
		CloseHandle(fileHandle);
		return false;
	}

	DWORD bytesReaded; // @Remark, necessary !!!!! failed on some Windows 7.
	if (ReadFile(fileHandle, buff, fileSize, &bytesReaded, NULL) == FALSE) {
		Log::Error("ReadFile failed ! (%u)", GetLastError());
		CloseHandle(fileHandle);
		return false;
	}

	buff[fileSize] = 0;
	CloseHandle(fileHandle);
	return true;
}

void ChatCommands::CmdAge2(int argc, LPWSTR *argv) {
	char buffer[32];
	DWORD second = GW::Map::GetInstanceTime() / 1000;
	sprintf_s(buffer, "%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	Log::Info(buffer);
}

void ChatCommands::CmdDialog(int argc, LPWSTR *argv) {
	if (argc <= 1) {
		Log::Error("Please provide an integer or hex argument");
	} else {
	#if 1
		int id;
		if (GuiUtils::ParseInt(argv[1], &id)) {
			GW::Agents::Dialog(id);
			Log::Info("Sent Dialog 0x%X", id);
		} else {
			Log::Error("Invalid argument '%ls', please use an integer or hex value", argv[0]);
		}
	#else
		try {
			long id = std::stol(argv[1], 0, 0);
			GW::Agents::Dialog(id);
			Log::Info("Sent Dialog 0x%X", id);
		} catch (...) {
			Log::Error("Invalid argument '%ls', please use an integer or hex value", argv[0]);
		}
	#endif
	}
}

void ChatCommands::CmdChest(int argc, LPWSTR *argv) {
	switch (GW::Map::GetInstanceType()) {
	case GW::Constants::InstanceType::Outpost:
		GW::Items::OpenXunlaiWindow();
		break;
	case GW::Constants::InstanceType::Explorable: {
		GW::Agent* target = GW::Agents::GetTarget();
		if (target && target->Type == 0x200) {
			GW::Agents::GoSignpost(target);
			GW::Items::OpenLockedChest();
		}
	}
		break;
	default:
		break;
	}
}

void ChatCommands::CmdTB(int argc, LPWSTR *argv) {
	if (argc <= 1) {
		MainWindow::Instance().visible ^= 1;
	} else {
		std::wstring arg = GuiUtils::ToLower(argv[1]);
		if (arg == L"age") {
			CmdAge2(0, argv);
		} else if (arg == L"hide") {
			MainWindow::Instance().visible = false;
		} else if (arg == L"show") {
			MainWindow::Instance().visible = true;
		} else if (arg == L"reset") {
			ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
			ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
		} else if (arg == L"settings") {
			SettingsWindow::Instance().visible = true;
			ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
		} else if (arg == L"mini" || arg == L"minimize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::Instance().StartSelfDestruct();
		} else if (arg == L"ignore") {

		} else {
			std::string name = std::string(arg.begin(), arg.end());
			for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
				if (name == GuiUtils::ToLower(window->Name())) {
					window->visible ^= 1;
				}
			}
		}
	}
}

std::vector<ToolboxUIElement*> ChatCommands::MatchingWindows(int argc, LPWSTR *argv) {
	std::vector<ToolboxUIElement*> ret;
	if (argc <= 1) {
		ret.push_back(&MainWindow::Instance());
	} else {
		std::wstring arg = GuiUtils::ToLower(argv[1]);
		if (arg == L"all") {
			for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
				ret.push_back(window);
			}
		} else {
			std::string name = std::string(arg.begin(), arg.end());
			for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
				if (name == GuiUtils::ToLower(window->Name())) {
					ret.push_back(window);
				}
			}
		}
	}
	return ret;
}

void ChatCommands::CmdShow(int argc, LPWSTR *argv) {
	auto windows = MatchingWindows(argc, argv);
	if (windows.empty()) {
		if (argc == 2 && argv[1] == L"settings") {
			SettingsWindow::Instance().visible = true;
		} else {
			Log::Error("Cannot find window '%ls'", argc > 1 ? argv[1] : L"");
		}
	} else {
		for (ToolboxUIElement* window : windows) {
			window->visible = true;
		}
	}
}

void ChatCommands::CmdHide(int argc, LPWSTR *argv) {
	auto windows = MatchingWindows(argc, argv);
	if (windows.empty()) {
		Log::Error("Cannot find window '%ls'", argc > 1 ? argv[1] : L"");
	} else {
		for (ToolboxUIElement* window : windows) {
			window->visible = false;
		}
	}
}

void ChatCommands::CmdTP(int argc, LPWSTR *argv) {
	if (argc == 1) {
		Log::Error("[Error] Please provide an argument");
	} else {
		std::wstring town = GuiUtils::ToLower(argv[1]);

		GW::Constants::District district = GW::Constants::District::Current;
		int district_number = 0;
		if (argc > 2 && town != L"gh") {
			std::wstring dis = GuiUtils::ToLower(argv[2]);
			if (dis == L"ae") {
				district = GW::Constants::District::American;
			} else if (dis == L"ae1") {
				district = GW::Constants::District::American;
				district_number = 1;
			} else if (dis == L"int") {
				district = GW::Constants::District::International;
			} else if (dis == L"ee") {
				district = GW::Constants::District::EuropeEnglish;
			} else if (dis == L"eg" || dis == L"dd") {
				district = GW::Constants::District::EuropeGerman;
			} else if (dis == L"ef") {
				district = GW::Constants::District::EuropeFrench;
			} else if (dis == L"ei") {
				district = GW::Constants::District::EuropeItalian;
			} else if (dis == L"es") {
				district = GW::Constants::District::EuropeSpanish;
			} else if (dis == L"ep") {
				district = GW::Constants::District::EuropePolish;
			} else if (dis == L"er") {
				district = GW::Constants::District::EuropeRussian;
			} else if (dis == L"ak") {
				district = GW::Constants::District::AsiaKorean;
			} else if (dis == L"ac" || dis == L"atc") {
				district = GW::Constants::District::AsiaChinese;
			} else if (dis == L"aj") {
				district = GW::Constants::District::AsiaJapanese;
			} else {
				Log::Error("Invalid district '%ls'", dis.c_str());
			}
		}

		if (town.compare(0, 3, L"fav", 3) == 0) {
			std::wstring fav_s_num = town.substr(3, std::wstring::npos);
			if (fav_s_num.empty()) {
				TravelWindow::Instance().TravelFavorite(0);
			} else {
				int fav_num = wcstol(fav_s_num.c_str(), nullptr, 0);
				TravelWindow::Instance().TravelFavorite(fav_num - 1);
			}
		} else if (town == L"toa") {
			GW::Map::Travel(GW::Constants::MapID::Temple_of_the_Ages, district, district_number);
		} else if (town == L"doa") {
			GW::Map::Travel(GW::Constants::MapID::Domain_of_Anguish, district, district_number);
		} else if (town == L"kamadan" || town == L"kama") {
			GW::Map::Travel(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost, district, district_number);
		} else if (town == L"embark") {
			GW::Map::Travel(GW::Constants::MapID::Embark_Beach, district, district_number);
		} else if (town == L"eee") {
			GW::Map::Travel(GW::Constants::MapID::Embark_Beach, GW::Constants::District::EuropeEnglish, 0);
		} else if (town == L"vlox" || town == L"vloxs") {
			GW::Map::Travel(GW::Constants::MapID::Vloxs_Falls, district, district_number);
		} else if (town == L"gadd" || town == L"gadds") {
			GW::Map::Travel(GW::Constants::MapID::Gadds_Encampment_outpost, district, district_number);
		} else if (town == L"urgoz") {
			GW::Map::Travel(GW::Constants::MapID::Urgozs_Warren, district, district_number);
		} else if (town == L"deep") {
			GW::Map::Travel(GW::Constants::MapID::The_Deep, district, district_number);
		} else if (town == L"gtob") {
			GW::Map::Travel(GW::Constants::MapID::Great_Temple_of_Balthazar_outpost, district, district_number);
		} else if (town == L"la") {
			GW::Map::Travel(GW::Constants::MapID::Lions_Arch_outpost, district, district_number);
		} else if (town == L"kaineng") {
			GW::Map::Travel(GW::Constants::MapID::Kaineng_Center_outpost, district, district_number);
		} else if (town == L"eotn") {
			GW::Map::Travel(GW::Constants::MapID::Eye_of_the_North_outpost, district, district_number);
		} else if (town == L"sif" || town == L"sifhalla") {
			GW::Map::Travel(GW::Constants::MapID::Sifhalla_outpost, district, district_number);
		} else if (town == L"doom" || town == L"doomlore") {
			GW::Map::Travel(GW::Constants::MapID::Doomlore_Shrine_outpost, district, district_number);
		} else if (town == L"gh") {
			if (argc == 2) {
				GW::GuildMgr::TravelGH();
			} else {
				std::wstring tag = GuiUtils::ToLower(argv[2]);
				GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
				for (GW::Guild* guild : guilds) {
					if (guild && GuiUtils::ToLower(guild->tag) == tag) {
						GW::GuildMgr::TravelGH(guild->key);
						break;
					}
				}
			}
		} else {
			int mapid = wcstol(town.c_str(), nullptr, 0);
			if (mapid) {
				GW::Map::Travel((GW::Constants::MapID)mapid, district, district_number);
			}
		}
	}
}

void ChatCommands::CmdZoom(int argc, LPWSTR *argv) {
	if (argc <= 1) {
		GW::CameraMgr::SetMaxDist();
	} else {
	#if 1
		int distance;
		if (GuiUtils::ParseInt(argv[1], &distance)) {
			if (distance > 0) {
				GW::CameraMgr::SetMaxDist(static_cast<float>(distance));
			} else {
				Log::Error("Invalid argument '%ls', please use a positive integer value", argv[1]);
			}
		} else {
			Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
		}
	#else
		try {
			long distance = std::stol(argv[1]);
			if (distance > 0) {
				GW::CameraMgr::SetMaxDist(static_cast<float>(distance));
			} else {
				Log::Error("Invalid argument '%ls', please use a positive integer value", argv[1]);
			}
			
		} catch (...) {
			Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
		}
	#endif
	}
}

void ChatCommands::CmdCamera(int argc, LPWSTR *argv) {
	if (argc == 1) {
		GW::CameraMgr::UnlockCam(false);
	} else {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"lock") {
			GW::CameraMgr::UnlockCam(false);
		} else if (arg1 == L"unlock") {
			GW::CameraMgr::UnlockCam(true);
			Log::Info("Use W,A,S,D,X,Z for camera movement");
		} else if (arg1 == L"fog") {
			std::wstring arg2 = GuiUtils::ToLower(argv[2]);
			if (arg2 == L"on") {
				GW::CameraMgr::SetFog(true);
			} else if (arg2 == L"off") {
				GW::CameraMgr::SetFog(false);
			}
		} else if (arg1 == L"fov") {
			if (argc == 2) {
				GW::CameraMgr::SetFieldOfView(1.308997f);
			} else {
				std::wstring arg2 = GuiUtils::ToLower(argv[2]);
				if (arg2 == L"default") {
					GW::CameraMgr::SetFieldOfView(1.308997f);
				} else {
					try {
						float fovnew = std::stof(arg2);
						if (fovnew > 0) {
							GW::CameraMgr::SetFieldOfView(fovnew);
							Log::Info("Field of View is %f", fovnew);
						} else {
							Log::Error("Invalid argument '%ls', please use a positive value", argv[2]);
						}
					} catch (...) {
						Log::Error("Invalid argument '%ls', please use a float value", argv[2]);
					}
				}
			}
		} else if (arg1 == L"speed") {
			if (argc < 3) {
				Instance().cam_speed_ = Instance().DEFAULT_CAM_SPEED;
			} else {
				std::wstring arg2 = GuiUtils::ToLower(argv[2]);
				if (arg2 == L"default") {
					Instance().cam_speed_ = Instance().DEFAULT_CAM_SPEED;
				} else {
					try {
						float speed = std::stof(arg2);
						Instance().cam_speed_ = speed;
						Log::Info("Camera speed is now %f", speed);
					} catch (...) {
						Log::Error("Invalid argument '%ls', please use a float value", argv[2]);
					}
				}
			}
		} else {
			Log::Error("Invalid argument.");
		}
	}
}

void ChatCommands::CmdDamage(int argc, LPWSTR *argv) {
	if (argc <= 1) {
		PartyDamage::Instance().WritePartyDamage();
	} else {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"print" || arg1 == L"report") {
			PartyDamage::Instance().WritePartyDamage();
		} else if (arg1 == L"me") {
			PartyDamage::Instance().WriteOwnDamage();
		} else if (arg1 == L"reset") {
			PartyDamage::Instance().ResetDamage();
		} else {
		#if 1
			int idx;
			if (GuiUtils::ParseInt(argv[1], &idx)) {
				PartyDamage::Instance().WriteDamageOf(idx - 1);
			} else {
			}
		#else
			try {
				long idx = std::stol(arg1);
				PartyDamage::Instance().WriteDamageOf(idx - 1);
			} catch (...) {}
		#endif
		}
	}
}

void ChatCommands::CmdAfk(int argc, LPWSTR *argv) {
	GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
}

void ChatCommands::CmdTarget(int argc, LPWSTR *argv) {
	if (argc > 1) {
		std::wstring arg1 = GuiUtils::ToLower(argv[2]);
		if (arg1 == L"closest" || arg1 == L"nearest") {
			// target nearest agent
			GW::AgentArray agents = GW::Agents::GetAgentArray();
			if (!agents.valid()) return;

			GW::Agent* me = GW::Agents::GetPlayer();
			if (me == nullptr) return;

			float distance = GW::Constants::SqrRange::Compass;
			int closest = -1;

			for (size_t i = 0; i < agents.size(); ++i) {
				GW::Agent* agent = agents[i];
				if (agent == nullptr) continue;
				if (agent->PlayerNumber != me->PlayerNumber) {
					float newDistance = GW::Agents::GetSqrDistance(me->pos, agents[i]->pos);
					if (newDistance < distance) {
						closest = i;
						distance = newDistance;
					}
				}
			}
			if (closest > 0) {
				GW::Agents::ChangeTarget(agents[closest]);
			}

		} else if (arg1 == L"getid") {
			GW::Agent* target = GW::Agents::GetTarget();
			if (target == nullptr) {
				Log::Error("No target selected!");
			} else {
				Log::Info("Target model id (PlayerNumber) is %d", target->PlayerNumber);
			}
		} else if (arg1 == L"getpos") {
			GW::Agent* target = GW::Agents::GetTarget();
			if (target == nullptr) {
				Log::Error("No target selected!");
			} else {
				Log::Info("Target coordinates are (%f, %f)", target->pos.x, target->pos.y);
			}
		}
	}
}

void ChatCommands::CmdUseSkill(int argc, LPWSTR *argv) {
	if (argc == 1) {
		Instance().skill_to_use = 0;
	} else if (argc == 2) {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"stop" || arg1 == L"off") {
			Instance().skill_to_use = 0;
		} else {
			try {
				int skill = std::stoi(argv[1]);
				if (skill >= 0 && skill <= 8) {
					Instance().skill_to_use = skill;
					Log::Info("Using skill %d on recharge. Use /useskill to stop", skill);
				}
			} catch (...) {
				Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
			}
		}
	}
}

void ChatCommands::CmdSCWiki(int argc, LPWSTR *argv) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (argc == 1) {
		ShellExecuteW(NULL, L"open", L"http://wiki.fbgmguild.com/Main_Page", NULL, NULL, SW_SHOWNORMAL);
	} else {
		// the buffer is large enough, because you can type only 120 characters at once in the chat.
		wchar_t link[256] = L"http://wiki.fbgmguild.com/index.php?search=";
		int i;
		for (i = 1; i < argc - 1; i++) {
			wcscat_s(link, argv[i]);
			wcscat_s(link, L"+");
		}
		wcscat_s(link, argv[i]);
		ShellExecuteW(NULL, L"open", link, NULL, NULL, SW_SHOWNORMAL);
	}
}

void ChatCommands::CmdLoad(int argc, LPWSTR *argv) {
	// We will & should move that to GWCA.
	static int(__fastcall *GetPersonalDir)(size_t size, wchar_t *dir) = 0;
	if (!GetPersonalDir) *(DWORD*)&GetPersonalDir = 0x005AAB60; // need scan!
	if (argc == 1) {
		// It open the build window, click on build will make Gw crash.
		/*
		typedef void(__fastcall *SendUIMessage_t)(int id, void *param1, void *param2);
		SendUIMessage_t SendUIMessage = SendUIMessage_t(0x00605AC0); // need scan!
		int32_t param[2] = { 0, 2 };
		SendUIMessage(0x100001B4, param, NULL);
		*/
		return;
	}

	LPWSTR arg1 = argv[1];
	wchar_t dir[512];
	GetPersonalDir(512, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
	wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
	wcscat_s(dir, arg1);
	wcscat_s(dir, L".txt");

	char temp[64];
	if (!ReadTemplateFile(dir, temp, 64)) {
		// If it failed, we will interpret the input as the code models.
		size_t len = wcslen(arg1);
		if (len > 64) return;
		for (size_t i = 0; i < len; i++)
			temp[i] = (char)arg1[i];
		temp[len] = 0;
	}

	GW::SkillbarMgr::LoadSkillTemplate(temp);
}
