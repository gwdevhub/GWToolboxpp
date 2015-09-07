#pragma once

#include "Shlobj.h"
#include "Shlwapi.h"

#include "../include/OSHGui/OSHGui.hpp"
#include <string>
#include "logger.h"

using namespace OSHGui::Drawing;
using namespace std;

class GuiUtils {
public:

	// Returns the default on hover color.
	static Color getMouseOverColor() {
		return OSHGui::Application::InstancePtr()->GetTheme().GetControlColorTheme("mouseover").ForeColor;
	}

	// Returns the settings folder as std::wstring
	static wstring getSettingsFolderW() {
		WCHAR szPathW[MAX_PATH];
		szPathW[0] = L'\0';
		SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPathW);
		return wstring(szPathW) + L"\\GWToolbox";
	}

	// Returns the settings folder as std::string
	static string getSettingsFolderA() {
		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath);
		return string(szPath) + "\\GWToolbox";
	}

	// Returns the path of the given file in the settings folder as std::wstring
	static wstring getPathW(wstring file) { 
		return getSettingsFolderW() + L"\\" + file; 
	}

	// Returns the path of the given file in the settings folder as std::string
	static string getPathA(string file) { 
		return getSettingsFolderA() + "\\" + file; 
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as std::wstring
	static wstring getSubPathW(wstring file, wstring subdir) {
		return getSettingsFolderW() + L"\\" + subdir + L"\\" + file;
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
			string path = GuiUtils::getPathA("Friz_Quadrata_Regular.ttf");
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
};
