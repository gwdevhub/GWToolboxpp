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
#include <GWCA\Context\GameContext.h>

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
	ImGui::Bullet(); ImGui::Text("'/load [build template|build name] [Hero index]' loads a build. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
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
		"It's possible to use more than one skill on recharge. "
		"Use empty '/useskill' or '/useskill stop' to stop all. "
		"Use '/useskill <skill>' to stop the skill.");
	ImGui::Bullet(); ImGui::Text("'/zoom <value>' to change the maximum zoom to the value. "
		"use empty '/zoom' to reset to the default value of 750.");
}

void ChatCommands::DrawSettingInternal() {
	ImGui::Checkbox("Fix height when moving forward", &forward_fix_z);
	ImGui::InputFloat("Camera speed", &cam_speed, 0.0f, 0.0f, 3);
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
	GW::Chat::CreateCommand(L"transmo", ChatCommands::CmdTransmo);
}

#define KEY_ESC 0x1B
#define KEY_A 0x41
#define KEY_D 0x44
#define KEY_E 0x45
#define KEY_Q 0x51
#define KEY_R 0x52
#define KEY_S 0x53
#define KEY_W 0x57
#define KEY_X 0x58
#define KEY_Z 0x5A
#define KEY_LEFT 0x25
#define KEY_UP 0x26
#define KEY_RIGHT 0x27
#define KEY_DOWN 0x28

bool ChatCommands::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!GW::CameraMgr::GetCameraUnlock()) return false;
	if (GW::Chat::IsTyping()) return false;
	if (ImGui::GetIO().WantTextInput) return false;

	switch (Message) {
		case WM_KEYDOWN:
		case WM_KEYUP:
			switch (wParam) {
				case KEY_A:
				case KEY_D:
				case KEY_E:
				case KEY_Q:
				case KEY_R:
				case KEY_S:
				case KEY_W:
				case KEY_X:
				case KEY_Z:
					return true;
			}
	}
	return false;
}

void ChatCommands::Update(float delta) {
	static bool keep_forward;
	float dist_dist = delta * cam_speed;
	float dist_rot  = delta * ROTATION_SPEED;

	if (GW::CameraMgr::GetCameraUnlock() 
		&& !GW::Chat::IsTyping() 
		&& !ImGui::GetIO().WantTextInput) {

		float forward = 0;
		float vertical = 0;
		float rotate = 0;
		float side = 0;
		if (ImGui::IsKeyDown(KEY_W) || ImGui::IsKeyDown(KEY_UP) || keep_forward) forward += 1.0f;
		if (ImGui::IsKeyDown(KEY_S) || ImGui::IsKeyDown(KEY_DOWN)) forward -= 1.0f;
		if (ImGui::IsKeyDown(KEY_Q)) side += 1.0f;
		if (ImGui::IsKeyDown(KEY_E)) side -= 1.0f;
		if (ImGui::IsKeyDown(KEY_Z)) vertical -= 1.0f;
		if (ImGui::IsKeyDown(KEY_X)) vertical += 1.0f;
		if (ImGui::IsKeyDown(KEY_A) || ImGui::IsKeyDown(KEY_LEFT)) rotate += 1.0f;
		if (ImGui::IsKeyDown(KEY_D) || ImGui::IsKeyDown(KEY_RIGHT)) rotate -= 1.0f;
		if (ImGui::IsKeyDown(KEY_R)) keep_forward = true;
		if (ImGui::IsKeyDown(KEY_W) || ImGui::IsKeyDown(KEY_S) || ImGui::IsKeyDown(KEY_ESC)) keep_forward = false;

		if (GWToolbox::Instance().right_mouse_down && (rotate != 0.f)) {
			side = rotate;
			rotate = 0.f;
		}

		GW::CameraMgr::ForwardMovement(forward * delta * cam_speed, !forward_fix_z);
		GW::CameraMgr::VerticalMovement(vertical * delta * cam_speed);
		GW::CameraMgr::RotateMovement(rotate * delta * ROTATION_SPEED);
		GW::CameraMgr::SideMovement(side * delta * cam_speed);
		GW::CameraMgr::UpdateCameraPos();
	}

	for (int slot : skills_to_use) {
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& (clock() - skill_timer) / 1000.0f > skill_usage_delay) {
			GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
			if (skillbar.IsValid()) {
				GW::SkillbarSkill skill = skillbar.Skills[slot];
				if (skill.GetRecharge() == 0) {
					GW::GameThread::Enqueue([slot] {
						GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId());
					});

					GW::Skill skilldata = GW::SkillbarMgr::GetSkillConstantData(skill.SkillId);
					skill_usage_delay = std::max(skilldata.Activation + skilldata.Aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
					skill_timer = clock();
				}
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
	snprintf(buffer, 32, "%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
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

void ChatCommands::ParseDistrict(const std::wstring& s, GW::Constants::District& district, int& number) {
	district = GW::Constants::District::Current;
	number = 0;
	if (s == L"ae") {
		district = GW::Constants::District::American;
	} else if (s == L"ae1") {
		district = GW::Constants::District::American;
		number = 1;
	} else if (s == L"int") {
		district = GW::Constants::District::International;
	} else if (s == L"ee") {
		district = GW::Constants::District::EuropeEnglish;
	} else if (s == L"eg" || s == L"dd") {
		district = GW::Constants::District::EuropeGerman;
	} else if (s == L"ef" || s == L"fr") {
		district = GW::Constants::District::EuropeFrench;
	} else if (s == L"ei") {
		district = GW::Constants::District::EuropeItalian;
	} else if (s == L"es") {
		district = GW::Constants::District::EuropeSpanish;
	} else if (s == L"ep") {
		district = GW::Constants::District::EuropePolish;
	} else if (s == L"er") {
		district = GW::Constants::District::EuropeRussian;
	} else if (s == L"ak") {
		district = GW::Constants::District::AsiaKorean;
	} else if (s == L"ac" || s == L"atc") {
		district = GW::Constants::District::AsiaChinese;
	} else if (s == L"aj") {
		district = GW::Constants::District::AsiaJapanese;
	}
}

void ChatCommands::CmdTP(int argc, LPWSTR *argv) {
	// zero argument error
	if (argc == 1) {
		Log::Error("[Error] Please provide an argument");
		return;
	}

	std::wstring arg1 = GuiUtils::ToLower(argv[1]);

	// own guild hall
	if (arg1 == L"gh" && argc == 2) {
		GW::GuildMgr::TravelGH();
		return;
	}
	// ally guild hall
	if (arg1 == L"gh" && argc > 2) {
		std::wstring tag = GuiUtils::ToLower(argv[2]);
		GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
		for (GW::Guild* guild : guilds) {
			if (guild && GuiUtils::ToLower(guild->tag) == tag) {
				GW::GuildMgr::TravelGH(guild->key);
				return;
			}
		}
		Log::Error("[Error] Did not recognize guild '%ls'\n", argv[2]);
		return;
	}

	// current outpost - different district
	GW::Constants::District district;
	int district_number;
	ParseDistrict(arg1, district, district_number);
	if (district != GW::Constants::District::Current) {
		// we have a match
		GW::Constants::MapID current = GW::Map::GetMapID();
		GW::Map::Travel(current, district, district_number);
		return;
	}
	
	// different outpost - district optional
	if (argc > 2) {
		std::wstring dis = GuiUtils::ToLower(argv[2]);
		ParseDistrict(dis, district, district_number);
		if (district == GW::Constants::District::Current) {
			// did not match district, but continue with travel anyway
			Log::Error("Invalid district '%ls'", dis.c_str());
		}
	}

	std::wstring town = GuiUtils::ToLower(argv[1]);
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
	} else {
		int mapid = wcstol(town.c_str(), nullptr, 0);
		if (mapid) {
			GW::Map::Travel((GW::Constants::MapID)mapid, district, district_number);
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
			Log::Info("Use Q/E, A/D, W/S, X/Z for camera movement");
		} else if (arg1 == L"fog") {
			if (argc == 3) {
				std::wstring arg2 = GuiUtils::ToLower(argv[2]);
				if (arg2 == L"on") {
					GW::CameraMgr::SetFog(true);
				} else if (arg2 == L"off") {
					GW::CameraMgr::SetFog(false);
				}
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
				Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
			} else {
				std::wstring arg2 = GuiUtils::ToLower(argv[2]);
				if (arg2 == L"default") {
					Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
				} else {
					try {
						float speed = std::stof(arg2);
						Instance().cam_speed = speed;
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
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
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

void ChatCommands::ToggleSkill(int skill) {
	if (skill <= 0 || skill > 8) return;
	auto i = std::find(skills_to_use.begin(), skills_to_use.end(), skill - 1);
	if (i == skills_to_use.end()) {
		skills_to_use.push_front(skill - 1);
	} else {
		skills_to_use.erase(i);
	}
}

void ChatCommands::CmdUseSkill(int argc, LPWSTR *argv) {
	if (argc == 1) {
		Instance().skills_to_use.clear();
	} else if (argc >= 2) {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"stop" || arg1 == L"off" || arg1 == L"0") {
			Instance().skills_to_use.clear();
		} else {
			for (int i = argc - 1; i > 0; --i) {
				try {
					int num = std::stoi(argv[i]);
					if (num >= 0) {
						// note: num can be one or more skills
						while (num > 10) {
							Instance().ToggleSkill(num % 10);
							num = num / 10;
						}
						Instance().ToggleSkill(num);
					}
				} catch (...) {
					Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
				}
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
	*(BYTE**)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
	if (argc == 1) {
		// We could open the build template window, but any interaction with it would make gw crash.
		// int32_t param[2] = { 0, 2 };
		// SendUIMessage(0x100001B4, param, NULL);
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
	if (argc == 2)
		GW::SkillbarMgr::LoadSkillTemplate(temp);	
	else if (argc == 3)
		GW::SkillbarMgr::LoadSkillTemplate(temp, std::stoi(argv[2]));
}

void ChatCommands::CmdTransmo(int argc, LPWSTR *argv) {
	int scale = 0;
	if (argc == 2) {
		GuiUtils::ParseInt(argv[1], &scale);
		if (scale < 6 || scale > 255) {
			Log::Error("scale must be between [6, 255]");
			return;
		}
	}

	GW::Agent *target = GW::Agents::GetTarget();
	if (!target || !target->GetIsCharacterType()) return;

	DWORD npc_id = 0;
	if (target->IsPlayer()) {
		npc_id = target->TransmogNpcId & 0x0FFFFFFF;
	} else {
		npc_id = target->PlayerNumber;
	}

	if (!npc_id) return;
	GW::NPCArray &npcs = GW::GameContext::instance()->world->npcs;
	GW::NPC &npc = npcs[npc_id];

#if 0
	npc_id = 12;
	GW::NPC npc = {0};
	npc.ModelFileID = 193245;
	npc.scale = 0x64000000; // I think, 2 highest order bytes are percent of size, so 0x64000000 is 100%

	// Those 2 packets (P074 & P075) are used to create a new model, for instance if we want to "use" a tonic.
	// We have to find the data that are in the NPC structure and feed them to those 2 packets.

	GW::GameThread::Enqueue([npc_id, npc]()
	{
		GW::Packet::StoC::P074_NpcGeneralStats packet;
		packet.header = 74;
		packet.npc_id = npc_id;
		packet.file_id = npc.ModelFileID;
		packet.data1 = 0;
		packet.scale = npc.scale;
		packet.data2 = 0;
		packet.flags = npc.NpcFlags;
		packet.profession = npc.Primary;
		packet.level = 0;
		packet.name[0] = 0;

		GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
	});

	GW::GameThread::Enqueue([npc_id, npc]()
	{
		GW::Packet::StoC::P075 packet;
		packet.header = 75;
		packet.npc_id = npc_id;

		// If we found a npc that have more than 1 ModelFiles, we can determine which one of h0028, h002C are size and capacity.
		assert(npc.FilesCount <= 8);
		packet.count = npc.FilesCount;
		for (size_t i = 0; i < npc.FilesCount; i++)
			packet.data[i] = npc.ModelFiles[i];

		GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
	});
#endif

	GW::GameThread::Enqueue([scale]()
	{
		GW::Packet::StoC::P142 packet;
		packet.header = 142;
		packet.agent_id = GW::Agents::GetPlayerId();
		packet.scale = (DWORD)scale << 24;

		GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
	});

	GW::GameThread::Enqueue([npc_id]()
	{
		GW::Packet::StoC::P162 packet;
		packet.header = 162;
		packet.agent_id = GW::Agents::GetPlayerId();
		packet.model_id = npc_id;
		GW::StoC::EmulatePacket((GW::Packet::StoC::PacketBase *)&packet);
	});
}
