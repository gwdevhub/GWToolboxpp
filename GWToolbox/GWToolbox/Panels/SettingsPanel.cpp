#include "SettingsPanel.h"

#include <Windows.h>

#include "Defines.h"
#include "GuiUtils.h" 
#include "GWToolbox.h"
#include <OtherModules\ToolboxSettings.h>
#include <OtherModules\GameSettings.h>
#include <OtherModules\Resources.h>

void SettingsPanel::Initialize() {
	ToolboxPanel::Initialize();
	Resources::Instance().LoadTextureAsync(&texture, "settings.png", "img");
}

void SettingsPanel::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImGui::PushTextWrapPos();
		ImGui::Text("GWToolbox++ version "
			GWTOOLBOX_VERSION
			" by Has and KAOS"
			BETA_VERSION);
		float w = (ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemSpacing.x) / 2;
		if (ImGui::Button("Open Settings Folder", ImVec2(w, 0))) {
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ShellExecute(NULL, "open", GuiUtils::getSettingsFolder().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}
		ImGui::SameLine();
		if (ImGui::Button("Open GWToolbox++ Website", ImVec2(w, 0))) {
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ShellExecute(NULL, "open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
		}

		ToolboxSettings::Instance().DrawFreezeSetting();
		ImGui::SameLine(ImGui::GetWindowWidth() / 2 + ImGui::GetStyle().ItemSpacing.x / 2);
		GameSettings::Instance().DrawBorderlessSetting();

		ImGui::Text("General:");
		if (ImGui::CollapsingHeader("Help")) {
			if (ImGui::TreeNode("General Interface")) {
				ImGui::Bullet(); ImGui::Text("Double-click on title bar to collapse a window.");
				ImGui::Bullet(); ImGui::Text("Click and drag on lower right corner to resize a window.");
				ImGui::Bullet(); ImGui::Text("Click and drag on any empty space to move a window.");
				ImGui::Bullet(); ImGui::Text("Mouse Wheel to scroll.");
				if (ImGui::GetIO().FontAllowUserScaling) {
					ImGui::Bullet(); ImGui::Text("CTRL+Mouse Wheel to zoom window contents.");
				}
				ImGui::Bullet(); ImGui::Text("TAB/SHIFT+TAB to cycle through keyboard editable fields.");
				ImGui::Bullet(); ImGui::Text("CTRL+Click on a slider or drag box to input text.");
				ImGui::Bullet(); ImGui::Text(
					"While editing text:\n"
					"- Hold SHIFT or use mouse to select text\n"
					"- CTRL+Left/Right to word jump\n"
					"- CTRL+A or double-click to select all\n"
					"- CTRL+X,CTRL+C,CTRL+V clipboard\n"
					"- CTRL+Z,CTRL+Y undo/redo\n"
					"- ESCAPE to revert\n"
					"- You can apply arithmetic operators +,*,/ on numerical values. Use +- to subtract.\n");
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Opening and closing windows")) {
				ImGui::Text("There are several ways to open and close toolbox windows and widgets:");
				ImGui::Bullet(); ImGui::Text("Buttons in the main window.");
				ImGui::Bullet(); ImGui::Text("Checkboxes in the Info window.");
				ImGui::Bullet(); ImGui::Text("Checkboxes on the right of each setting header below.");
				ImGui::Bullet(); ImGui::Text("Chat command '/hide <name>' to hide a window or widget.");
				ImGui::Bullet(); ImGui::Text("Chat command '/show <name>' to show a window or widget.");
				ImGui::Bullet(); ImGui::Text("Chat command '/tb <name>' to toggle a window or widget.");
				ImGui::Indent();
				ImGui::Text("In the commands above, <name> is the title of the window as shown in the title bar. For example, try '/hide settings' and '/show settings'.");
				ImGui::Text("Note: the names of the widgets without a visible title bar are the same as in the setting headers below.");
				ImGui::Unindent();
				ImGui::Bullet(); ImGui::Text("Send Chat hotkey to enter one of the commands above.");
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Chat Commands")) {
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
				ImGui::Bullet(); ImGui::Text("'/travel <town> [dis]', '/tp <town> [dis]' or '/to <arg> [dis]' to travel to a destination. \n"
					"<arg> can be any of: toa, doa, kamadan/kama, embark, vlox, gadds, urgoz, deep, fav1, fav2, fav3.\n"
					"[dis] can be any of: ae, ae1, ee, ee1, eg, eg1, int");
				ImGui::Bullet(); ImGui::Text("'/useskill <skill>' starts using the skill on recharge. "
					"Use the skill number instead of <skill> (e.g. '/useskill 5'). "
					"Use empty '/useskill', '/useskill 0' or '/useskill stop' to stop.");
				ImGui::Bullet(); ImGui::Text("'/zoom <value>' to change the maximum zoom to the value. "
					"use empty '/zoom' to reset to the default value of 750.");
				ImGui::TreePop();
			}
		}

		for (unsigned i = 0; i < GWToolbox::Instance().modules.size(); ++i) {
			if (i == 6) ImGui::Text("Main Window / Panels:");
			if (i == 15) ImGui::Text("Windows / Widgets:");
			GWToolbox::Instance().modules[i]->DrawSettings();
		}

		if (ImGui::Button("Save Now", ImVec2(w, 0))) {
			GWToolbox::Instance().SaveSettings();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally saves settings on exit.\nClick to save to disk now.");
		ImGui::SameLine();
		if (ImGui::Button("Load Now", ImVec2(w, 0))) {
			GWToolbox::Instance().LoadSettings();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally loads settings on launch.\nClick to re-load from disk now.");
		ImGui::PopTextWrapPos();
	}
	ImGui::End();
}
