#pragma once

#include <Shlobj.h>
#include <Shlwapi.h>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\GwConstants.h>

#include <string>
#include "logger.h"

using namespace OSHGui::Drawing;
using namespace std;

class GuiUtils {
public:
	static const int SPACE = 6; // same as OSHGui's Padding
	static const int HALFSPACE = SPACE / 2;
	static const int ROW_HEIGHT = 25; // toolbox standard button height
	static const int BUTTON_HEIGHT = 25;

	// Returns the default on hover color.
	static Color getMouseOverColor() {
		return OSHGui::Application::InstancePtr()->GetTheme().GetControlColorTheme("mouseover").ForeColor;
	}

	// Returns the settings folder as std::wstring
	static wstring getSettingsFolder() {
		WCHAR szPathW[MAX_PATH];
		szPathW[0] = L'\0';
		SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPathW);
		return wstring(szPathW) + L"\\GWToolboxpp";
	}

	// Returns the settings folder as std::string
	static string getSettingsFolderA() {
		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
		return string(szPath) + "\\GWToolboxpp";
	}

	// Returns the path of the given file in the settings folder as std::wstring
	static wstring getPath(wstring file) { 
		return getSettingsFolder() + L"\\" + file; 
	}

	// Returns the path of the given file in the settings folder as std::string
	static string getPathA(string file) { 
		return getSettingsFolderA() + "\\" + file; 
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as std::wstring
	static wstring getSubPath(wstring file, wstring subdir) {
		return getSettingsFolder() + L"\\" + subdir + L"\\" + file;
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as string
	static string getSubPathA(string file, string subdir) {
		return getSettingsFolderA() + "\\" + subdir + "\\" + file;
	}

	// Get the default toolbox font with given size and antialiased
	static FontPtr getTBFont(float size, bool antialiased) {
		FontPtr font;
		try {
			string path = GuiUtils::getPathA("Font.ttf");
			font = FontManager::LoadFontFromFile(path, size, antialiased);
		} catch (OSHGui::Misc::FileNotFoundException e) {
			LOG("ERROR - font file not found, falling back to Arial\n");
			font = FontManager::LoadFont("Arial", size, false);
		}

		if (font == NULL) {
			LOG("ERROR loading font, falling back to Arial\n");
			font = FontManager::LoadFont("Arial", size, false);
		}
		return font;
	}

	static int ComputeX(int container_width, int horizontal_amount, int grid_x) {
		float item_width = ComputeWidthF(container_width, horizontal_amount);
		return std::lroundf(SPACE + grid_x * (item_width + SPACE));
	}

	static int ComputeY(int grid_y) {
		return SPACE + grid_y * (ROW_HEIGHT + SPACE);
	}

	static float ComputeWidthF(int container_width, int horizontal_amount, int grid_width = 1) {
		float item_width = (float)(container_width - SPACE) / horizontal_amount - SPACE;
		return item_width * grid_width + (grid_width - 1) * SPACE;
	}

	static int ComputeWidth(int container_width, int horizontal_amount, int grid_width = 1) {
		float item_width = (float)(container_width - SPACE) / horizontal_amount - SPACE;
		return std::lroundf(std::floor(item_width * grid_width + (grid_width - 1) * SPACE));
	}

	static int GetPartyHealthbarHeight() {
		GwConstants::InterfaceSize interfacesize = 
			static_cast<GwConstants::InterfaceSize>(*(DWORD*)0x0A3FD08);

		switch (interfacesize) {
		case GwConstants::InterfaceSize::SMALL:
			return GwConstants::HealthbarHeight::Small;
		case GwConstants::InterfaceSize::NORMAL:
			return GwConstants::HealthbarHeight::Normal;
		case GwConstants::InterfaceSize::LARGE:
			return GwConstants::HealthbarHeight::Large;
		case GwConstants::InterfaceSize::VERYLARGE:
			return GwConstants::HealthbarHeight::VeryLarge;
		default:
			return GwConstants::HealthbarHeight::Normal;
		}
	}
};
