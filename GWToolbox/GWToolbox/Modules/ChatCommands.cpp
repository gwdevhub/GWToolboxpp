#include "stdafx.h"
#include "ChatCommands.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GuiUtils.h>
#include "GWToolbox.h"
#include <Keys.h>
#include <logger.h>
#include <Modules/GameSettings.h>
#include <Windows/MainWindow.h>
#include <Windows/SettingsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/BuildsWindow.h>
#include <Widgets/PartyDamage.h>

namespace {
	const wchar_t *next_word(const wchar_t *str) {
		while (*str && !isspace(*str))
			str++;
		while (*str && isspace(*str))
			str++;
		return *str ? str : NULL;
	}
}

void ChatCommands::DrawHelp() {
	ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
	ImGui::TextDisabled("Below, <xyz> denotes an argument, use an appropriate value without the quotes.\n"
		"(a|b) denotes a mandatory argument, in this case 'a' or 'b'.\n"
		"[a|b] denotes an optional argument, in this case nothing, 'a' or 'b'.");

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
	ImGui::Bullet(); ImGui::Text("'/flag [all|clear|<number>]' to flag a hero in the minimap (same as using the buttons by the minimap).");
	ImGui::Bullet(); ImGui::Text("'/flag [all|<number>] [x] [y]' to flag a hero to coordinates [x],[y].");
	ImGui::Bullet(); ImGui::Text("'/flag <number> clear' to clear flag for a hero.");
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
	ImGui::Text("'/cam unlock' options");
	ImGui::Indent();
	ImGui::Checkbox("Fix height when moving forward", &forward_fix_z);
	ImGui::InputFloat("Camera speed", &cam_speed, 0.0f, 0.0f, 3);
	ImGui::Unindent();
}

void ChatCommands::LoadSettings(CSimpleIni* ini) {
	forward_fix_z = ini->GetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
	cam_speed = (float)ini->GetDoubleValue(Name(), VAR_NAME(cam_speed), DEFAULT_CAM_SPEED);
}

void ChatCommands::SaveSettings(CSimpleIni* ini) {
	ini->SetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
	ini->SetDoubleValue(Name(), VAR_NAME(cam_speed), cam_speed);
}

void ChatCommands::Initialize() {
	ToolboxModule::Initialize();

	// you can create commands here in-line with a lambda, but only if they are only 
	// a couple of lines and not used multiple times
	GW::Chat::CreateCommand(L"ff", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
		GW::Chat::SendChat('/', "resign");
	});
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
	GW::Chat::CreateCommand(L"scwiki", ChatCommands::CmdSCWiki);
	GW::Chat::CreateCommand(L"load", ChatCommands::CmdLoad);
	GW::Chat::CreateCommand(L"transmo", ChatCommands::CmdTransmo);
	GW::Chat::CreateCommand(L"resize", ChatCommands::CmdResize);
}

bool ChatCommands::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!GW::CameraMgr::GetCameraUnlock()) return false;
	if (GW::Chat::GetIsTyping()) return false;
	if (ImGui::GetIO().WantTextInput) return false;

	switch (Message) {
		case WM_KEYDOWN:
		case WM_KEYUP:
			switch (wParam) {
				case VK_A:
				case VK_D:
				case VK_E:
				case VK_Q:
				case VK_R:
				case VK_S:
				case VK_W:
				case VK_X:
				case VK_Z:
				
				case VK_ESCAPE:
				case VK_UP:
				case VK_DOWN:
				case VK_LEFT:
				case VK_RIGHT:
					return true;
			}
	}
	return false;
}

void ChatCommands::Update(float delta) {
	static bool keep_forward;

	if (delta == 0.f) return;
	float dist_dist = delta * cam_speed;
	float dist_rot  = delta * ROTATION_SPEED;

	if (GW::CameraMgr::GetCameraUnlock() 
		&& !GW::Chat::GetIsTyping() 
		&& !ImGui::GetIO().WantTextInput) {

		float forward = 0;
		float vertical = 0;
		float rotate = 0;
		float side = 0;
		if (ImGui::IsKeyDown(VK_W) || ImGui::IsKeyDown(VK_UP) || keep_forward) forward += 1.0f;
		if (ImGui::IsKeyDown(VK_S) || ImGui::IsKeyDown(VK_DOWN)) forward -= 1.0f;
		if (ImGui::IsKeyDown(VK_Q)) side += 1.0f;
		if (ImGui::IsKeyDown(VK_E)) side -= 1.0f;
		if (ImGui::IsKeyDown(VK_Z)) vertical -= 1.0f;
		if (ImGui::IsKeyDown(VK_X)) vertical += 1.0f;
		if (ImGui::IsKeyDown(VK_A) || ImGui::IsKeyDown(VK_LEFT)) rotate += 1.0f;
		if (ImGui::IsKeyDown(VK_D) || ImGui::IsKeyDown(VK_RIGHT)) rotate -= 1.0f;
		if (ImGui::IsKeyDown(VK_R)) keep_forward = true;

		if (ImGui::IsKeyDown(VK_W) || ImGui::IsKeyDown(VK_UP) ||
			ImGui::IsKeyDown(VK_S) || ImGui::IsKeyDown(VK_DOWN) ||
			ImGui::IsKeyDown(VK_ESCAPE))
		{
			keep_forward = false;
		}

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
			GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
			if (skillbar && skillbar->IsValid()) {
				GW::SkillbarSkill skill = skillbar->skills[slot];
				GW::Skill& skilldata = GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
				if ((skilldata.adrenaline == 0 && skill.GetRecharge() == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a == skilldata.adrenaline)) {
					GW::GameThread::Enqueue([slot] {
						GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId());
					});

					skill_usage_delay = std::max(skilldata.activation + skilldata.aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
					skill_timer = clock();
				}
			}
		}
	}
}

bool ChatCommands::ReadTemplateFile(std::wstring path, char *buff, size_t buffSize) {
	HANDLE fileHandle = CreateFileW(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		// We don't print that error, because we can /load [template]
		// Log::Error("Failed openning file '%S'", path.c_str());
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

void ChatCommands::CmdAge2(const wchar_t *message, int argc, LPWSTR *argv) {
	DWORD second = GW::Map::GetInstanceTime() / 1000;
	Log::Info("%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
}

void ChatCommands::CmdDialog(const wchar_t *message, int argc, LPWSTR *argv) {
	if (argc <= 1) {
		Log::Error("Please provide an integer or hex argument");
	} else {
		int id;
		if (GuiUtils::ParseInt(argv[1], &id)) {
			GW::Agents::SendDialog(id);
			Log::Info("Sent Dialog 0x%X", id);
		} else {
			Log::Error("Invalid argument '%ls', please use an integer or hex value", argv[0]);
		}
	}
}

void ChatCommands::CmdChest(const wchar_t *message, int argc, LPWSTR *argv) {
	switch (GW::Map::GetInstanceType()) {
	case GW::Constants::InstanceType::Outpost:
		GW::Items::OpenXunlaiWindow();
		break;
	case GW::Constants::InstanceType::Explorable: {
		GW::Agent* target = GW::Agents::GetTarget();
		if (target && target->type == 0x200) {
			GW::Agents::GoSignpost(target);
			GW::Items::OpenLockedChest();
		}
	}
		break;
	default:
		break;
	}
}

void ChatCommands::CmdTB(const wchar_t *message, int argc, LPWSTR *argv) {
	if (argc <= 1) {
		MainWindow::Instance().visible ^= 1;
	} else {
		std::wstring arg = GuiUtils::ToLower(argv[1]);
		if (arg == L"age") {
			CmdAge2(message, 0, argv);
		} else if (arg == L"hide") {
			MainWindow::Instance().visible = false;
		} else if (arg == L"show") {
			MainWindow::Instance().visible = true;
		} else if (arg == L"reset") {
			ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
			ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
			MainWindow::Instance().visible = false;
			SettingsWindow::Instance().visible = true;
		} else if (arg == L"settings") {
			SettingsWindow::Instance().visible = true;
		} else if (arg == L"mini" || arg == L"minimize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::Instance().StartSelfDestruct();
		} else if (arg == L"ignore") {

		} else {
            auto windows = MatchingWindows(message, argc, argv);
            for (ToolboxUIElement* window : windows) {
                window->visible ^= 1;
            }
		}
	}
}

std::vector<ToolboxUIElement*> ChatCommands::MatchingWindows(const wchar_t *message, int argc, LPWSTR *argv) {
	std::vector<ToolboxUIElement*> ret;
	if (argc <= 1) {
		ret.push_back(&MainWindow::Instance());
	} else {
		const wchar_t *tail = next_word(message);
		std::wstring arg = GuiUtils::ToLower(tail);
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

void ChatCommands::CmdShow(const wchar_t *message, int argc, LPWSTR *argv) {
	auto windows = MatchingWindows(message, argc, argv);
	if (windows.empty()) {
		if (argc == 2 && !wcscmp(argv[1], L"settings")) {
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

void ChatCommands::CmdHide(const wchar_t *message, int argc, LPWSTR *argv) {
	auto windows = MatchingWindows(message, argc, argv);
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
	} else if (s == L"ei" || s == L"it") {
		district = GW::Constants::District::EuropeItalian;
	} else if (s == L"es") {
		district = GW::Constants::District::EuropeSpanish;
	} else if (s == L"ep" || s == L"pl") {
		district = GW::Constants::District::EuropePolish;
	} else if (s == L"er" || s == L"ru") {
		district = GW::Constants::District::EuropeRussian;
	} else if (s == L"ak" || s == L"kr") {
		district = GW::Constants::District::AsiaKorean;
	} else if (s == L"ac" || s == L"atc" || s == L"ch") {
		district = GW::Constants::District::AsiaChinese;
	} else if (s == L"aj" || s == L"jp") {
		district = GW::Constants::District::AsiaJapanese;
	}
}

void ChatCommands::CmdTP(const wchar_t *message, int argc, LPWSTR *argv) {
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
		Log::Error("[Error] Did not recognize guild '%ls'", argv[2]);
		return;
	}

	// current outpost - different district
	GW::Constants::District district;
	int district_number;
	ParseDistrict(arg1, district, district_number);
	if (district != GW::Constants::District::Current) {
		// we have a match
		GW::Constants::MapID current = GW::Map::GetMapID();
		TravelWindow::Travel(current, district, district_number);
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
			int fav_num;
			if (GuiUtils::ParseInt(fav_s_num.c_str(), &fav_num)) {
				TravelWindow::Instance().TravelFavorite(fav_num - 1);
			}
		}
	} else if (town == L"toa") {
		TravelWindow::Travel(GW::Constants::MapID::Temple_of_the_Ages, district, district_number);
	} else if (town == L"doa") {
		TravelWindow::Travel(GW::Constants::MapID::Domain_of_Anguish, district, district_number);
	} else if (town == L"kamadan" || town == L"kama") {
		TravelWindow::Travel(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost, district, district_number);
	} else if (town == L"embark") {
		TravelWindow::Travel(GW::Constants::MapID::Embark_Beach, district, district_number);
	} else if (town == L"eee") {
		TravelWindow::Travel(GW::Constants::MapID::Embark_Beach, GW::Constants::District::EuropeEnglish, 0);
	} else if (town == L"vlox" || town == L"vloxs") {
		TravelWindow::Travel(GW::Constants::MapID::Vloxs_Falls, district, district_number);
	} else if (town == L"gadd" || town == L"gadds") {
		TravelWindow::Travel(GW::Constants::MapID::Gadds_Encampment_outpost, district, district_number);
	} else if (town == L"urgoz") {
		TravelWindow::Travel(GW::Constants::MapID::Urgozs_Warren, district, district_number);
	} else if (town == L"deep") {
		TravelWindow::Travel(GW::Constants::MapID::The_Deep, district, district_number);
	} else if (town == L"gtob") {
		TravelWindow::Travel(GW::Constants::MapID::Great_Temple_of_Balthazar_outpost, district, district_number);
	} else if (town == L"la") {
		TravelWindow::Travel(GW::Constants::MapID::Lions_Arch_outpost, district, district_number);
	} else if (town == L"kaineng") {
		TravelWindow::Travel(GW::Constants::MapID::Kaineng_Center_outpost, district, district_number);
	} else if (town == L"eotn") {
		TravelWindow::Travel(GW::Constants::MapID::Eye_of_the_North_outpost, district, district_number);
	} else if (town == L"sif" || town == L"sifhalla") {
		TravelWindow::Travel(GW::Constants::MapID::Sifhalla_outpost, district, district_number);
	} else if (town == L"doom" || town == L"doomlore") {
		TravelWindow::Travel(GW::Constants::MapID::Doomlore_Shrine_outpost, district, district_number);
	} else {
		int map_id_integer;
		if (GuiUtils::ParseInt(town.c_str(), &map_id_integer) && (map_id_integer != 0)) {
			TravelWindow::Travel((GW::Constants::MapID)map_id_integer, district, district_number);
		} else {
			GW::Constants::MapID map_id = MatchMapPrefix(town.c_str());
			if (map_id != GW::Constants::MapID::None) {
				TravelWindow::Travel(map_id, district, district_number);
			}
		}

	}
}

void ChatCommands::CmdZoom(const wchar_t *message, int argc, LPWSTR *argv) {
	if (argc <= 1) {
		GW::CameraMgr::SetMaxDist();
	} else {
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
	}
}

void ChatCommands::CmdCamera(const wchar_t *message, int argc, LPWSTR *argv) {
	if (argc == 1) {
		GW::CameraMgr::UnlockCam(false);
	} else {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"lock") {
			GW::CameraMgr::UnlockCam(false);
		} else if (arg1 == L"unlock") {
			GW::CameraMgr::UnlockCam(true);
			Log::Info("Use Q/E, A/D, W/S, X/Z, R and arrows for camera movement");
		} else if (arg1 == L"fog") {
			if (argc == 3) {
				std::wstring arg2 = GuiUtils::ToLower(argv[2]);
				if (arg2 == L"on") {
					GW::CameraMgr::SetFog(true);
				} else if (arg2 == L"off") {
					GW::CameraMgr::SetFog(false);
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

void ChatCommands::CmdDamage(const wchar_t *message, int argc, LPWSTR *argv) {
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
			int idx;
			if (GuiUtils::ParseInt(argv[1], &idx)) {
				PartyDamage::Instance().WriteDamageOf(idx - 1);
			}
		}
	}
}

void ChatCommands::CmdAfk(const wchar_t *message, int argc, LPWSTR *argv) {
	GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
    GameSettings& settings = GameSettings::Instance();
	if (argc > 1) {
		const wchar_t *afk_msg = next_word(message);
		settings.SetAfkMessage(afk_msg);
	} else {
        settings.afk_message.clear();
    }
}

void ChatCommands::CmdTarget(const wchar_t *message, int argc, LPWSTR *argv) {
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
				if (agent->player_number != me->player_number) {
					float newDistance = GW::GetSquareDistance(me->pos, agents[i]->pos);
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
				Log::Info("Target model id (PlayerNumber) is %d", target->player_number);
			}
		} else if (arg1 == L"getpos") {
			GW::Agent* target = GW::Agents::GetTarget();
			if (target == nullptr) {
				Log::Error("No target selected!");
			} else {
				Log::Info("Target coordinates are (%f, %f)", target->pos.x, target->pos.y);
			}
		} else {
			const wchar_t *name = next_word(message);
			GW::Player *target = GW::PlayerMgr::GetPlayerByName(name);
			if (target != NULL) {
				GW::Agent *agent = GW::Agents::GetAgentByID(target->agent_id);
				if (agent) {
					GW::Agents::ChangeTarget(agent);
				}
			}
		}
	}
}

void ChatCommands::AddSkillToUse(int skill) {
	if (skill <= 0 || skill > 8) return;
	auto i = std::find(skills_to_use.begin(), skills_to_use.end(), skill - 1);
	if (i == skills_to_use.end()) {
		skills_to_use.push_front(skill - 1);
	}
}

void ChatCommands::CmdUseSkill(const wchar_t *message, int argc, LPWSTR *argv) {
	Instance().skills_to_use.clear();

	if (argc > 1) {
		std::wstring arg1 = GuiUtils::ToLower(argv[1]);
		if (arg1 == L"stop" || arg1 == L"off" || arg1 == L"0") {
			// do nothing, already cleared skills_to_use
		} else {
			for (int i = argc - 1; i > 0; --i) {
				try {
					int num = std::stoi(argv[i]);
					if (num >= 0) {
						// note: num can be one or more skills
						while (num > 10) {
							Instance().AddSkillToUse(num % 10);
							num = num / 10;
						}
						Instance().AddSkillToUse(num);
					}
				} catch (...) {
					Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
				}
			}
		}
	}
}

void ChatCommands::CmdSCWiki(const wchar_t *message, int argc, LPWSTR *argv) {
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

void ChatCommands::CmdLoad(const wchar_t *message, int argc, LPWSTR *argv) {
	// We will & should move that to GWCA.
	static int(__cdecl *GetPersonalDir)(size_t size, wchar_t *dir) = 0;
	*(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
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
		if (len >= 64) return;
		for (size_t i = 0; i < len; i++)
			temp[i] = (char)arg1[i];
		temp[len] = 0;
	}
	if (argc == 2)
		GW::SkillbarMgr::LoadSkillTemplate(temp);	
	else if (argc == 3) {
		int hero_number;
		if (GuiUtils::ParseInt(argv[2], &hero_number)) {
			// @Robustness:
			// Check that the number is actually valid or make sure LoadSkillTemplate is safe
			if (0 < hero_number && hero_number <= 8) {
				GW::SkillbarMgr::LoadSkillTemplate(temp, hero_number);
			}
		}
	}
}

void ChatCommands::CmdTransmo(const wchar_t *message, int argc, LPWSTR *argv) {
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
		npc_id = target->transmog_npc_id & 0x0FFFFFFF;
	} else {
		npc_id = target->player_number;
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
		GW::Packet::StoC::NpcGeneralStats packet;
		packet.header = 79;
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
		GW::Packet::StoC::P080 packet;
		packet.header = 80;
		packet.npc_id = npc_id;I 

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
		GW::Packet::StoC::AgentScale packet;
		packet.agent_id = GW::Agents::GetPlayerId();
		packet.scale = (DWORD)scale << 24;

		GW::StoC::EmulatePacket(&packet);
	});

	GW::GameThread::Enqueue([npc_id]()
	{
		GW::Packet::StoC::AgentModel packet;
		packet.agent_id = GW::Agents::GetPlayerId();
		packet.model_id = npc_id;
		GW::StoC::EmulatePacket(&packet);
	});
}

void ChatCommands::CmdResize(const wchar_t *message, int argc, LPWSTR *argv) {
	if (argc != 3) {
		Log::Error("The syntax is /resize width height");
		return;
	}
	int width, height;
	if (!(GuiUtils::ParseInt(argv[1], &width) && GuiUtils::ParseInt(argv[2], &height))) {
		Log::Error("The syntax is /resize width height");
		return;
	}
	HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
	RECT rect;
	GetWindowRect(hwnd, &rect);
	MoveWindow(hwnd, rect.left, rect.top, width, height, TRUE);
}


static struct {
    const char *name;
	const wchar_t 		   *sname;
	GW::Constants::MapID 	map_id;
} teleport_search_map[] = {
	"Abaddon's Gate", L"abaddonsgate", GW::Constants::MapID::Abaddons_Gate,
	"Abaddon's Mouth", L"abaddonsmouth", GW::Constants::MapID::Abaddons_Mouth,
	"Altrumm Ruins", L"altrummruins", GW::Constants::MapID::Altrumm_Ruins,
	"Amatz Basin", L"amatzbasin", GW::Constants::MapID::Amatz_Basin,
	"Amnoon Oasis, The", L"amnoonoasisthe", GW::Constants::MapID::The_Amnoon_Oasis_outpost,
	"Arborstone", L"arborstone", GW::Constants::MapID::Arborstone_outpost_mission,
	"Ascalon City", L"ascaloncity", GW::Constants::MapID::Ascalon_City_outpost,
	"Aspenwood Gate (Kurzick)", L"aspenwoodgatekurzick", GW::Constants::MapID::Aspenwood_Gate_Kurzick_outpost,
	"Aspenwood Gate (Luxon)", L"aspenwoodgateluxon", GW::Constants::MapID::Aspenwood_Gate_Luxon_outpost,
	"Astralarium, The", L"astralariumthe", GW::Constants::MapID::The_Astralarium_outpost,
	"Augury Rock", L"auguryrock", GW::Constants::MapID::Augury_Rock_outpost,
	"Aurios Mines, The", L"auriosminesthe", GW::Constants::MapID::The_Aurios_Mines,
	"Aurora Glade", L"auroraglade", GW::Constants::MapID::Aurora_Glade,
	"Bai Paasu Reach", L"baipaasureach", GW::Constants::MapID::Bai_Paasu_Reach_outpost,
	"Basalt Grotto", L"basaltgrotto", GW::Constants::MapID::Basalt_Grotto_outpost,
	"Beacon's Perch", L"beaconsperch", GW::Constants::MapID::Beacons_Perch_outpost,
	"Beetletun", L"beetletun", GW::Constants::MapID::Beetletun_outpost,
	"Beknur Harbor", L"beknurharbor", GW::Constants::MapID::Beknur_Harbor,
	"Bergen Hot Springs", L"bergenhotsprings", GW::Constants::MapID::Bergen_Hot_Springs_outpost,
	"Blacktide Den", L"blacktideden", GW::Constants::MapID::Blacktide_Den,
	"Bloodstone Fen", L"bloodstonefen", GW::Constants::MapID::Bloodstone_Fen,
	"Bone Palace", L"bonepalace", GW::Constants::MapID::Bone_Palace_outpost,
	"Boreal Station", L"borealstation", GW::Constants::MapID::Boreal_Station_outpost,
	"Boreas Seabed", L"boreasseabed", GW::Constants::MapID::Boreas_Seabed_outpost_mission,
	"Borlis Pass", L"borlispass", GW::Constants::MapID::Borlis_Pass,
	"Brauer Academy", L"braueracademy", GW::Constants::MapID::Brauer_Academy_outpost,
	"Breaker Hollow", L"breakerhollow", GW::Constants::MapID::Breaker_Hollow_outpost,
	"Camp Hojanu", L"camphojanu", GW::Constants::MapID::Camp_Hojanu_outpost,
	"Camp Rankor", L"camprankor", GW::Constants::MapID::Camp_Rankor_outpost,
	"Cavalon", L"cavalon", GW::Constants::MapID::Cavalon_outpost,
	"Central Transfer Chamber", L"centraltransferchamber", GW::Constants::MapID::Central_Transfer_Chamber_outpost,
	"Chahbek Village", L"chahbekvillage", GW::Constants::MapID::Chahbek_Village,
	"Champion's Dawn", L"championsdawn", GW::Constants::MapID::Champions_Dawn_outpost,
	"Chantry of Secrets", L"chantryofsecrets", GW::Constants::MapID::Chantry_of_Secrets_outpost,
	"Codex Arena", L"codexarena", GW::Constants::MapID::Codex_Arena_outpost,
	"Consulate Docks", L"consulatedocks", GW::Constants::MapID::Consulate_Docks,
	"Copperhammer Mines", L"copperhammermines", GW::Constants::MapID::Copperhammer_Mines_outpost,
	"D'Alessio Seaboard", L"dajkahinlet", GW::Constants::MapID::Dajkah_Inlet,
	"Dajkah Inlet", L"dalessioseaboard", GW::Constants::MapID::DAlessio_Seaboard,
	"Dasha Vestibule", L"dashavestibule", GW::Constants::MapID::Dasha_Vestibule,
	"Deep, The", L"deepthe", GW::Constants::MapID::The_Deep,
	"Deldrimor War Camp", L"deldrimorwarcamp", GW::Constants::MapID::Deldrimor_War_Camp_outpost,
	"Destiny's Gorge", L"destinysgorge", GW::Constants::MapID::Destinys_Gorge_outpost,
	"Divinity Coast", L"divinitycoast", GW::Constants::MapID::Divinity_Coast,
	"Doomlore Shrine", L"doomloreshrine", GW::Constants::MapID::Doomlore_Shrine_outpost,
	"Dragon's Lair, The", L"dragonslairthe", GW::Constants::MapID::The_Dragons_Lair,
	"Dragon's Throat", L"dragonsthroat", GW::Constants::MapID::Dragons_Throat,
	"Droknar's Forge", L"droknarsforge", GW::Constants::MapID::Droknars_Forge_outpost,
	"Druid's Overlook", L"druidsoverlook", GW::Constants::MapID::Druids_Overlook_outpost,
	"Dunes of Despair", L"dunesofdespair", GW::Constants::MapID::Dunes_of_Despair,
	"Durheim Archives", L"durheimarchives", GW::Constants::MapID::Durheim_Archives_outpost,
	"Dzagonur Bastion", L"dzagonurbastion", GW::Constants::MapID::Dzagonur_Bastion,
	"Elona Reach", L"elonareach", GW::Constants::MapID::Elona_Reach,
	"Embark Beach", L"embarkbeach", GW::Constants::MapID::Embark_Beach,
	"Ember Light Camp", L"emberlightcamp", GW::Constants::MapID::Ember_Light_Camp_outpost,
	"Eredon Terrace", L"eredonterrace", GW::Constants::MapID::Eredon_Terrace_outpost,
	"Eternal Grove, The", L"eternalgrovethe", GW::Constants::MapID::The_Eternal_Grove_outpost_mission,
	"Eye of the North", L"eyeofthenorth", GW::Constants::MapID::Eye_of_the_North_outpost,
	"Fishermen's Haven", L"fishermenshaven", GW::Constants::MapID::Fishermens_Haven_outpost,
	"Fort Aspenwood (Kurzick)", L"fortaspenwoodkurzick", GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost,
	"Fort Aspenwood (Luxon)", L"fortaspenwoodluxon", GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost,
	"Fort Ranik", L"fortranik", GW::Constants::MapID::Fort_Ranik,
	"Frontier Gate", L"frontiergate", GW::Constants::MapID::Frontier_Gate_outpost,
	"Frost Gate, The", L"frostgatethe", GW::Constants::MapID::The_Frost_Gate,
	"Gadd's Encampment", L"gaddsencampment", GW::Constants::MapID::Gadds_Encampment_outpost,
	"Gate of Anguish", L"gateofanguish", GW::Constants::MapID::Domain_of_Anguish,
	"Gate of Desolation", L"gateofdesolation", GW::Constants::MapID::Gate_of_Desolation,
	"Gate of Fear", L"gateoffear", GW::Constants::MapID::Gate_of_Fear_outpost,
	"Gate of Madness", L"gateofmadness", GW::Constants::MapID::Gate_of_Madness,
	"Gate of Pain", L"gateofpain", GW::Constants::MapID::Gate_of_Pain,
	"Gate of Secrets", L"gateofsecrets", GW::Constants::MapID::Gate_of_Secrets_outpost,
	"Gate of the Nightfallen Lands", L"gateofthenightfallenlands", GW::Constants::MapID::Gate_of_the_Nightfallen_Lands_outpost,
	"Gate of Torment", L"gateoftorment", GW::Constants::MapID::Gate_of_Torment_outpost,
	"Gates of Kryta", L"gatesofkryta", GW::Constants::MapID::Gates_of_Kryta,
	"Grand Court of Sebelkeh", L"grandcourtofsebelkeh", GW::Constants::MapID::Grand_Court_of_Sebelkeh,
	"Granite Citadel, The", L"granitecitadelthe", GW::Constants::MapID::The_Granite_Citadel_outpost,
	"Great Northern Wall, The", L"greatnorthernwallthe", GW::Constants::MapID::The_Great_Northern_Wall,
	"Great Temple of Balthazar", L"greattempleofbalthazar", GW::Constants::MapID::Great_Temple_of_Balthazar_outpost,
	"Grendich Courthouse", L"grendichcourthouse", GW::Constants::MapID::Grendich_Courthouse_outpost,
	"Gunnar's Hold", L"gunnarshold", GW::Constants::MapID::Gunnars_Hold_outpost,
	"Gyala Hatchery", L"gyalahatchery", GW::Constants::MapID::Gyala_Hatchery_outpost_mission,
	"Harvest Temple", L"harvesttemple", GW::Constants::MapID::Harvest_Temple_outpost,
	"Hell's Precipice", L"hellsprecipice", GW::Constants::MapID::Hells_Precipice,
	"Henge of Denravi", L"hengeofdenravi", GW::Constants::MapID::Henge_of_Denravi_outpost,
	"Heroes' Ascent", L"heroesascent", GW::Constants::MapID::Heroes_Ascent_outpost,
	"Heroes' Audience", L"heroesaudience", GW::Constants::MapID::Heroes_Audience_outpost,
	"Honur Hill", L"honurhill", GW::Constants::MapID::Honur_Hill_outpost,
	"House zu Heltzer", L"housezuheltzer", GW::Constants::MapID::House_zu_Heltzer_outpost,
	"Ice Caves of Sorrow", L"icecavesofsorrow", GW::Constants::MapID::Ice_Caves_of_Sorrow,
	"Ice Tooth Cave", L"icetoothcave", GW::Constants::MapID::Ice_Tooth_Cave_outpost,
	"Imperial Sanctum", L"imperialsanctum", GW::Constants::MapID::Imperial_Sanctum_outpost_mission,
	"Iron Mines of Moladune", L"ironminesofmoladune", GW::Constants::MapID::Iron_Mines_of_Moladune,
	"Jade Flats (Kurzick)", L"jadeflatskurzick", GW::Constants::MapID::Jade_Flats_Kurzick_outpost,
	"Jade Flats (Luxon)", L"jadeflatsluxon", GW::Constants::MapID::Jade_Flats_Luxon_outpost,
	"Jade Quarry (Kurzick), The", L"jadequarrykurzickthe", GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost,
	"Jade Quarry (Luxon), The", L"jadequarryluxonthe", GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost,
	"Jennur's Horde", L"jennurshorde", GW::Constants::MapID::Jennurs_Horde,
	"Jokanur Diggings", L"jokanurdiggings", GW::Constants::MapID::Jokanur_Diggings,
	"Kaineng Center", L"kainengcenter", GW::Constants::MapID::Kaineng_Center_outpost,
	"Kamadan, Jewel of Istan", L"kamadan, Jewel of Istan", GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost,
	"Kodash Bazaar, The", L"kodashbazaarthe", GW::Constants::MapID::The_Kodash_Bazaar_outpost,
	"Kodlonu Hamlet", L"kodlonuhamlet", GW::Constants::MapID::Kodlonu_Hamlet_outpost,
	"Kodonur Crossroads", L"kodonurcrossroads", GW::Constants::MapID::Kodonur_Crossroads,
	"Lair of the Forgotten", L"lairoftheforgotten", GW::Constants::MapID::Lair_of_the_Forgotten_outpost,
	"Leviathan Pits", L"leviathanpits", GW::Constants::MapID::Leviathan_Pits_outpost,
	"Lion's Arch", L"lionsarch", GW::Constants::MapID::Lions_Arch_outpost,
	"Longeye's Ledge", L"longeyesledge", GW::Constants::MapID::Longeyes_Ledge_outpost,
	"Lutgardis Conservatory", L"lutgardisconservatory", GW::Constants::MapID::Lutgardis_Conservatory_outpost,
	"Maatu Keep", L"maatukeep", GW::Constants::MapID::Maatu_Keep_outpost,
	"Maguuma Stade", L"maguumastade", GW::Constants::MapID::Maguuma_Stade_outpost,
	"Marhan's Grotto", L"marhansgrotto", GW::Constants::MapID::Marhans_Grotto_outpost,
	"Marketplace, The", L"marketplacethe", GW::Constants::MapID::The_Marketplace_outpost,
	"Mihanu Township", L"mihanutownship", GW::Constants::MapID::Mihanu_Township_outpost,
	"Minister Cho's Estate", L"ministerchosestate", GW::Constants::MapID::Minister_Chos_Estate_outpost_mission,
	"Moddok Crevice", L"moddokcrevice", GW::Constants::MapID::Moddok_Crevice,
	"Mouth of Torment, The", L"mouthoftormentthe", GW::Constants::MapID::The_Mouth_of_Torment_outpost,
	"Nahpui Quarter", L"nahpuiquarter", GW::Constants::MapID::Nahpui_Quarter_outpost_mission,
	"Nolani Academy", L"nolaniacademy", GW::Constants::MapID::Nolani_Academy,
	"Nundu Bay", L"nundubay", GW::Constants::MapID::Nundu_Bay,
	"Olafstead", L"olafstead", GW::Constants::MapID::Olafstead_outpost,
	"Piken Square", L"pikensquare", GW::Constants::MapID::Piken_Square_outpost,
	"Pogahn Passage", L"pogahnpassage", GW::Constants::MapID::Pogahn_Passage,
	"Port Sledge", L"portsledge", GW::Constants::MapID::Port_Sledge_outpost,
	"Quarrel Falls", L"quarrelfalls", GW::Constants::MapID::Quarrel_Falls_outpost,
	"Raisu Palace", L"raisupalace", GW::Constants::MapID::Raisu_Palace_outpost_mission,
	"Ran Musu Gardens", L"ranmusugardens", GW::Constants::MapID::Ran_Musu_Gardens_outpost,
	"Random Arenas", L"randomarenas", GW::Constants::MapID::Random_Arenas_outpost,
	"Rata Sum", L"ratasum", GW::Constants::MapID::Rata_Sum_outpost,
	"Remains of Sahlahja", L"remainsofsahlahja", GW::Constants::MapID::Remains_of_Sahlahja,
	"Rilohn Refuge", L"rilohnrefuge", GW::Constants::MapID::Rilohn_Refuge,
	"Ring of Fire", L"ringoffire", GW::Constants::MapID::Ring_of_Fire,
	"Riverside Province", L"riversideprovince", GW::Constants::MapID::Riverside_Province,
	"Ruins of Morah", L"ruinsofmorah", GW::Constants::MapID::Ruins_of_Morah,
	"Ruins of Surmia", L"ruinsofsurmia", GW::Constants::MapID::Ruins_of_Surmia,
	"Saint Anjeka's Shrine", L"saintanjekasshrine", GW::Constants::MapID::Saint_Anjekas_Shrine_outpost,
	"Sanctum Cay", L"sanctumcay", GW::Constants::MapID::Sanctum_Cay,
	"Sardelac Sanitarium", L"sardelacsanitarium", GW::Constants::MapID::Sardelac_Sanitarium_outpost,
	"Seafarer's Rest", L"seafarersrest", GW::Constants::MapID::Seafarers_Rest_outpost,
	"Seeker's Passage", L"seekerspassage", GW::Constants::MapID::Seekers_Passage_outpost,
	"Seitung Harbor", L"seitungharbor", GW::Constants::MapID::Seitung_Harbor_outpost,
	"Senji's Corner", L"senjiscorner", GW::Constants::MapID::Senjis_Corner_outpost,
	"Serenity Temple", L"serenitytemple", GW::Constants::MapID::Serenity_Temple_outpost,
	"Shadow Nexus, The", L"shadownexusthe", GW::Constants::MapID::The_Shadow_Nexus,
	"Shing Jea Arena", L"shingjeaarena", GW::Constants::MapID::Shing_Jea_Arena,
	"Shing Jea Monastery", L"shingjeamonastery", GW::Constants::MapID::Shing_Jea_Monastery_outpost,
	"Sifhalla", L"sifhalla", GW::Constants::MapID::Sifhalla_outpost,
	"Sunjiang District", L"sunjiangdistrict", GW::Constants::MapID::Sunjiang_District_outpost_mission,
	"Sunspear Arena", L"sunspeararena", GW::Constants::MapID::Sunspear_Arena,
	"Sunspear Great Hall", L"sunspeargreathall", GW::Constants::MapID::Sunspear_Great_Hall_outpost,
	"Sunspear Sanctuary", L"sunspearsanctuary", GW::Constants::MapID::Sunspear_Sanctuary_outpost,
	"Tahnnakai Temple", L"tahnnakaitemple", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission,
	"Tanglewood Copse", L"tanglewoodcopse", GW::Constants::MapID::Tanglewood_Copse_outpost,
	"Tarnished Haven", L"tarnishedhaven", GW::Constants::MapID::Tarnished_Haven_outpost,
	"Temple of the Ages", L"templeoftheages", GW::Constants::MapID::Temple_of_the_Ages,
	"Thirsty River", L"thirstyriver", GW::Constants::MapID::Thirsty_River,
	"Thunderhead Keep", L"thunderheadkeep", GW::Constants::MapID::Thunderhead_Keep,
	"Tihark Orchard", L"tiharkorchard", GW::Constants::MapID::Tihark_Orchard,
	"Tomb of the Primeval Kings", L"tomboftheprimevalkings", GW::Constants::MapID::Tomb_of_the_Primeval_Kings,
	"Tsumei Village", L"tsumeivillage", GW::Constants::MapID::Tsumei_Village_outpost,
	"Umbral Grotto", L"umbralgrotto", GW::Constants::MapID::Umbral_Grotto_outpost,
	"Unwaking Waters (Kurzick)", L"unwakingwaterskurzick", GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
	"Unwaking Waters (Luxon)", L"unwakingwatersluxon", GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
	"Urgoz's Warren", L"urgozswarren", GW::Constants::MapID::Urgozs_Warren,
	"Vasburg Armory", L"vasburgarmory", GW::Constants::MapID::Vasburg_Armory_outpost,
	"Venta Cemetery", L"ventacemetery", GW::Constants::MapID::Venta_Cemetery,
	"Ventari's Refuge", L"ventarisrefuge", GW::Constants::MapID::Ventaris_Refuge_outpost,
	"Vizunah Square (Foreign)", L"vizunahsquareforeign", GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
	"Vizunah Square (Local)", L"vizunahsquarelocal", GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
	"Vlox's Falls", L"vloxsfalls", GW::Constants::MapID::Vloxs_Falls,
	"Wehhan Terraces", L"wehhanterraces", GW::Constants::MapID::Wehhan_Terraces_outpost,
	"Wilds, The", L"wildsthe", GW::Constants::MapID::The_Wilds,
	"Yahnur Market", L"yahnurmarket", GW::Constants::MapID::Yahnur_Market_outpost,
	"Yak's Bend", L"yaksbend", GW::Constants::MapID::Yaks_Bend_outpost,
	"Yohlon Haven", L"yohlonhaven", GW::Constants::MapID::Yohlon_Haven_outpost,
	"Zaishen Challenge", L"zaishenchallenge", GW::Constants::MapID::Zaishen_Challenge_outpost,
	"Zaishen Elite", L"zaishenelite", GW::Constants::MapID::Zaishen_Elite_outpost,
	"Zaishen Menagerie", L"zaishenmenagerie", GW::Constants::MapID::Zaishen_Menagerie_outpost,
	"Zen Daijun", L"zendaijun", GW::Constants::MapID::Zen_Daijun_outpost_mission,
	"Zin Ku Corridor", L"zinkucorridor", GW::Constants::MapID::Zin_Ku_Corridor_outpost,
	"Zos Shivros Channel", L"zosshivroschannel", GW::Constants::MapID::Zos_Shivros_Channel,
};

static bool string_starts_with(const wchar_t *str, const wchar_t *pre)
{
	for (;;) {
		if (*pre == 0)
			break;
		if (*str == 0)
			return false;
		if (*pre == L' ' || *pre == L'\'') {
			++pre;
			continue;
		}
		if (towlower(*pre) != *str)
			return false;
		++pre;
		++str;
	}
	return true;
}

GW::Constants::MapID ChatCommands::MatchMapPrefix(const wchar_t *map_name)
{
	size_t array_size = sizeof(teleport_search_map) / sizeof(teleport_search_map[0]);
	for (size_t i = 0; i < array_size; i++) {
		if (string_starts_with(teleport_search_map[i].sname, map_name)) {
			size_t count = 1;
			for (size_t j = i + 1; j < array_size; j++) {
				if (string_starts_with(teleport_search_map[j].sname, map_name))
					count++;
				else
					break;
			}

			if (count == 1) {
				return teleport_search_map[i].map_id;
			} else {
				std::stringstream ss;
				size_t max_count = std::min<size_t>(4, count);
				for (size_t j = i; j < i + max_count - 1; j++)
					ss << teleport_search_map[j].name << ", ";
				if (max_count < count)
					ss << teleport_search_map[i + max_count - 1].name << ", ...";
				else
					ss << teleport_search_map[i + max_count - 1].name << ".";
				std::string s = ss.str();
				Log::Info("Ambiguous prefix because of %s", s.c_str());
				return GW::Constants::MapID::None;
			}
			break;
		}
	}

	Log::Error("No map were found with the prefix '%S'", map_name);
	return GW::Constants::MapID::None;
}
