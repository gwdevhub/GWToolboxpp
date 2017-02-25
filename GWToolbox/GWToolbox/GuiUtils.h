#pragma once

#include <Shlobj.h>
#include <Shlwapi.h>

#include <imgui.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Utilities\PatternScanner.h>

#include <string>
#include "logger.h"

class GuiUtils {
public:
	static const int SPACE = 6; // same as OSHGui's Padding
	static const int HALFSPACE = SPACE / 2;
	static const int ROW_HEIGHT = 25; // toolbox standard button height
	static const int BUTTON_HEIGHT = 25;

	enum FontSize {
		f10,
		f11,
		f12,
		f16,
		f26,
		f30
	};
	static void LoadFonts() {
		std::string fontfile = GuiUtils::getPath("Font.ttf");
		ImGuiIO& io = ImGui::GetIO();
		ImFont* f10 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 16.0f);
		ImFont* f11 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 18.0f);
		ImFont* f12 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 19.0f);
		ImFont* f16 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 24.0f);
		ImFont* f26 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 42.0f);
		ImFont* f30 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 48.0f);
	}

	static void ShowHelp(const char* help) {
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(help);
		}
	}

	// Returns the settings folder as std::string
	static std::string getSettingsFolder() {
		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
		return std::string(szPath) + "\\GWToolboxpp";
	}

	// Returns the path of the given file in the settings folder as std::string
	static std::string getPath(std::string file) {
		return getSettingsFolder() + "\\" + file; 
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as string
	static std::string getSubPath(std::string file, std::string subdir) {
		return getSettingsFolder() + "\\" + subdir + "\\" + file;
	}

	static int GetPartyHealthbarHeight() {
		static DWORD* optionarray = nullptr;
		if (!optionarray) {
			GW::PatternScanner scan(0x401000, 0x49a000);
			optionarray = (DWORD*)scan.FindPattern("\x8B\x4D\x08\x85\xC9\x74\x0A", "xxxxxxx", -9);
			if (optionarray)
				optionarray = *(DWORD**)optionarray;
		}
		GW::Constants::InterfaceSize interfacesize =
			static_cast<GW::Constants::InterfaceSize>(optionarray[6]);

		switch (interfacesize) {
		case GW::Constants::InterfaceSize::SMALL: return GW::Constants::HealthbarHeight::Small;
		case GW::Constants::InterfaceSize::NORMAL: return GW::Constants::HealthbarHeight::Normal;
		case GW::Constants::InterfaceSize::LARGE: return GW::Constants::HealthbarHeight::Large;
		case GW::Constants::InterfaceSize::LARGER: return GW::Constants::HealthbarHeight::Larger;
		default:
			return GW::Constants::HealthbarHeight::Normal;
		}
	}
};
