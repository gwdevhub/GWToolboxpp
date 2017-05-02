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
#include <Panels\SettingsPanel.h>
#include <Windows\PartyDamage.h>

#include "Panels\TravelPanel.h"
#include "Panels\BuildPanel.h"

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
}

void ChatCommands::Initialize() {
	ToolboxModule::Initialize();

	GW::Chat::RegisterCommand(L"age2", ChatCommands::CmdAge2);
	GW::Chat::RegisterCommand(L"dialog", ChatCommands::CmdDialog);
	GW::Chat::RegisterCommand(L"show", ChatCommands::CmdShow);
	GW::Chat::RegisterCommand(L"hide", ChatCommands::CmdHide);
	GW::Chat::RegisterCommand(L"tb", ChatCommands::CmdTB);
	GW::Chat::RegisterCommand(L"tp", ChatCommands::CmdTP);
	GW::Chat::RegisterCommand(L"to", ChatCommands::CmdTP);
	GW::Chat::RegisterCommand(L"travel", ChatCommands::CmdTP);
	GW::Chat::RegisterCommand(L"zoom", ChatCommands::CmdZoom);
	GW::Chat::RegisterCommand(L"camera", ChatCommands::CmdCamera);
	GW::Chat::RegisterCommand(L"cam", ChatCommands::CmdCamera);
	GW::Chat::RegisterCommand(L"damage", ChatCommands::CmdDamage);
	GW::Chat::RegisterCommand(L"dmg", ChatCommands::CmdDamage);
	GW::Chat::RegisterCommand(L"chest", ChatCommands::CmdChest);
	GW::Chat::RegisterCommand(L"xunlai", ChatCommands::CmdChest);
	GW::Chat::RegisterCommand(L"afk", ChatCommands::CmdAfk);
	GW::Chat::RegisterCommand(L"target", ChatCommands::CmdTarget);
	GW::Chat::RegisterCommand(L"tgt", ChatCommands::CmdTarget);
	GW::Chat::RegisterCommand(L"useskill", ChatCommands::CmdUseSkill);
	GW::Chat::RegisterCommand(L"skilluse", ChatCommands::CmdUseSkill);

	GW::Chat::RegisterCommand(L"scwiki", 
		[](std::wstring& cmd, std::vector<std::wstring>& args) -> bool {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (args.size() == 0) {
			ShellExecuteW(NULL, L"open", L"http://wiki.fbgmguild.com/Main_Page", NULL, NULL, SW_SHOWNORMAL);
		} else {
			// the buffer is large enough, because you can type only 120 characters at once in the chat.
			wchar_t link[256] = L"http://wiki.fbgmguild.com/index.php?search=";
			size_t i;
			for (i = 0; i < args.size() - 1; i++) {
				wcscat_s(link, args[i].c_str());
				wcscat_s(link, L"+");
			}
			wcscat_s(link, args[i].c_str());
			ShellExecuteW(NULL, L"open", link, NULL, NULL, SW_SHOWNORMAL);
		}
		return true;
	});
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

namespace {
	std::wstring WStringToLower(std::wstring str) {
		std::wstring result = str;
		for (size_t i = 0; i < result.size(); i++)
			 result[i] = ::towlower(result[i]);
		return result;
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
	DWORD second = GW::Map::GetInstanceTime() / 1000;
	sprintf_s(buffer, "%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	Log::Info(buffer);
	return true;
}

bool ChatCommands::CmdDialog(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		Log::Error("Please provide an integer or hex argument");
	} else {
		try {
			long id = std::stol(args[0], 0, 0);
			GW::Agents::Dialog(id);
			Log::Info("Sent Dialog 0x%X", id);
		} catch (...) {
			Log::Error("Invalid argument '%ls', please use an integer or hex value", args[0].c_str());
		}
	}
	return true;
}

bool ChatCommands::CmdChest(std::wstring& cmd, std::vector<std::wstring>& args) {
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
	return true;
}

bool ChatCommands::CmdTB(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		MainWindow::Instance().visible ^= 1;
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
		} else if (arg == L"settings") {
			SettingsPanel::Instance().visible = true;
			ImGui::SetWindowPos(SettingsPanel::Instance().Name(), ImVec2(50.0f, 50.0f));
		} else if (arg == L"mini" || arg == L"minimize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
		} else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
			GWToolbox::Instance().StartSelfDestruct();
		} else {
			std::string name = std::string(arg.begin(), arg.end());
			for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
				if (name == GuiUtils::ToLower(window->Name())) {
					window->visible ^= 1;
				}
			}
		}
	}
	return true;
}

std::vector<ToolboxUIElement*> ChatCommands::MatchingWindows(std::vector<std::wstring>& args) {
	std::vector<ToolboxUIElement*> ret;
	if (args.empty()) {
		ret.push_back(&MainWindow::Instance());
	} else {
		std::wstring arg = GetLowerCaseArg(args, 0);
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

bool ChatCommands::CmdShow(std::wstring& cmd, std::vector<std::wstring>& args) {
	auto windows = MatchingWindows(args);
	if (windows.empty()) {
		if (args.size() == 1 && args[0] == L"settings") {
			SettingsPanel::Instance().visible = true;
		} else {
			Log::Error("Cannot find window '%ls'", args.empty() ? L"" : args[0].c_str());
		}
	} else {
		for (ToolboxUIElement* window : windows) {
			window->visible = true;
		}
	}
	return true;
}

bool ChatCommands::CmdHide(std::wstring& cmd, std::vector<std::wstring>& args) {
	auto windows = MatchingWindows(args);
	if (windows.empty()) {
		Log::Error("Cannot find window '%ls'", args.empty() ? L"" : args[0].c_str());
	} else {
		for (ToolboxUIElement* window : windows) {
			window->visible = false;
		}
	}
	return true;
}

bool ChatCommands::CmdTP(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		Log::Error("[Error] Please provide an argument");
	} else {
		std::wstring town = GetLowerCaseArg(args, 0);

		GW::Constants::District district = GW::Constants::District::Current;
		int district_number = 0;
		if (args.size() >= 2 && town != L"gh") {
			std::wstring dis = GetLowerCaseArg(args, 1);
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
				TravelPanel::Instance().TravelFavorite(0);
			} else {
				int fav_num = wcstol(fav_s_num.c_str(), nullptr, 0);
				TravelPanel::Instance().TravelFavorite(fav_num - 1);
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
			if (args.size() < 2) {
				GW::GuildMgr::TravelGH();
			} else {
				std::wstring tag = GetLowerCaseArg(args, 1);
				GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
				for (auto guild : guilds) {
					if (guild && WStringToLower(guild->tag()) == tag) {
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
	return true;
}

bool ChatCommands::CmdZoom(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		GW::CameraMgr::SetMaxDist();
	} else {
		try {
			long distance = std::stol(args[0]);
			if (distance > 0) {
				GW::CameraMgr::SetMaxDist(static_cast<float>(distance));
			} else {
				Log::Error("Invalid argument '%ls', please use a positive integer value", args[0].c_str());
			}
			
		} catch (...) {
			Log::Error("Invalid argument '%ls', please use an integer value", args[0].c_str());
		}
	}
	return true;
}

bool ChatCommands::CmdCamera(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		GW::CameraMgr::UnlockCam(false);
	} else {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"lock") {
			GW::CameraMgr::UnlockCam(false);
		} else if (arg0 == L"unlock") {
			GW::CameraMgr::UnlockCam(true);
			Log::Info("Use W,A,S,D,X,Z for camera movement");
		} else if (arg0 == L"fog") {
			std::wstring arg1 = GetLowerCaseArg(args, 1);
			if (arg1 == L"on") {
				GW::CameraMgr::SetFog(true);
			} else if (arg1 == L"off") {
				GW::CameraMgr::SetFog(false);
			}
		} else if (arg0 == L"fov") {
			if (args.size() == 1) {
				GW::CameraMgr::SetFieldOfView(1.308997f);
			} else {
				std::wstring arg1 = GetLowerCaseArg(args, 1);
				if (arg1 == L"default") {
					GW::CameraMgr::SetFieldOfView(1.308997f);
				} else {
					try {
						float fovnew = std::stof(arg1);
						if (fovnew > 0) {
							GW::CameraMgr::SetFieldOfView(fovnew);
							Log::Info("Field of View is %f", fovnew);
						} else {
							Log::Error("Invalid argument '%ls', please use a positive value", args[1].c_str());
						}
					} catch (...) {
						Log::Error("Invalid argument '%ls', please use a float value", args[1].c_str());
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
					Log::Info("Camera speed is now %f", speed);
				} catch (...) {
					Log::Error("Invalid argument '%ls', please use a float value", args[1].c_str());
				}
			}
		} else {
			Log::Error("Invalid argument.");
		}
	}
	return true;
}

bool ChatCommands::CmdDamage(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (args.empty()) {
		PartyDamage::Instance().WritePartyDamage();
	} else {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"print" || arg0 == L"report") {
			PartyDamage::Instance().WritePartyDamage();
		} else if (arg0 == L"me") {
			PartyDamage::Instance().WriteOwnDamage();
		} else if (arg0 == L"reset") {
			PartyDamage::Instance().ResetDamage();
		} else {
			try {
				long idx = std::stol(arg0);
				PartyDamage::Instance().WriteDamageOf(idx - 1);
			} catch (...) {}
		}
	}
	return true;
}

bool ChatCommands::CmdAfk(std::wstring& cmd, std::vector<std::wstring>& args) {
	GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
	return false;
}

bool ChatCommands::CmdTarget(std::wstring& cmd, std::vector<std::wstring>& args) {
	if (!args.empty()) {
		std::wstring arg0 = GetLowerCaseArg(args, 0);
		if (arg0 == L"closest" || arg0 == L"nearest") {
			// target nearest agent
			GW::AgentArray agents = GW::Agents::GetAgentArray();
			if (!agents.valid()) return true;

			GW::Agent* me = GW::Agents::GetPlayer();
			if (me == nullptr) return true;

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

		} else if (arg0 == L"getid") {
			GW::Agent* target = GW::Agents::GetTarget();
			if (target == nullptr) {
				Log::Error("No target selected!");
			} else {
				Log::Info("Target model id (PlayerNumber) is %d", target->PlayerNumber);
			}
		} else if (arg0 == L"getpos") {
			GW::Agent* target = GW::Agents::GetTarget();
			if (target == nullptr) {
				Log::Error("No target selected!");
			} else {
				Log::Info("Target coordinates are (%f, %f)", target->X, target->Y);
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
				if (skill >= 0 && skill <= 8) {
					Instance().skill_to_use = skill;
					Log::Info("Using skill %d on recharge. Use /useskill to stop", skill);
				}
			} catch (...) {
				Log::Error("Invalid argument '%ls', please use an integer value", args[0].c_str());
			}
		}
	}
	return true;
}


