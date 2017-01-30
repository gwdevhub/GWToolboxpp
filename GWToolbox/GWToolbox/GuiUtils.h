#pragma once

#include <Shlobj.h>
#include <Shlwapi.h>

#include <imgui.h>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\Constants\Constants.h>
#include <GWCA\Utilities\PatternScanner.h>

#include <string>
#include "logger.h"

using namespace OSHGui::Drawing;

class GuiUtils {
public:
	static const int SPACE = 6; // same as OSHGui's Padding
	static const int HALFSPACE = SPACE / 2;
	static const int ROW_HEIGHT = 25; // toolbox standard button height
	static const int BUTTON_HEIGHT = 25;

	enum FontSize {
		f10,
		f12,
		f16,
		f26,
		f30
	};
	static void LoadFonts() {
		std::string fontfile = GuiUtils::getPathA("Font.ttf");
		ImGuiIO& io = ImGui::GetIO();
		ImFont* f10 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 16.0f);
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

	// Returns the default on hover color.
	static Color getMouseOverColor() {
		return OSHGui::Application::InstancePtr()->GetTheme().GetControlColorTheme("mouseover").ForeColor;
	}

	// Returns the settings folder as std::wstring
	static std::wstring getSettingsFolder() {
		WCHAR szPathW[MAX_PATH];
		szPathW[0] = L'\0';
		SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPathW);
		return std::wstring(szPathW) + L"\\GWToolboxpp";
	}

	// Returns the settings folder as std::string
	static std::string getSettingsFolderA() {
		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
		return std::string(szPath) + "\\GWToolboxpp";
	}

	// Returns the path of the given file in the settings folder as std::wstring
	static std::wstring getPath(std::wstring file) {
		return getSettingsFolder() + L"\\" + file; 
	}

	// Returns the path of the given file in the settings folder as std::string
	static std::string getPathA(std::string file) {
		return getSettingsFolderA() + "\\" + file; 
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as std::wstring
	static std::wstring getSubPath(std::wstring file, std::wstring subdir) {
		return getSettingsFolder() + L"\\" + subdir + L"\\" + file;
	}

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as string
	static std::string getSubPathA(std::string file, std::string subdir) {
		return getSettingsFolderA() + "\\" + subdir + "\\" + file;
	}

	// Get the default toolbox font with given size and antialiased
	static FontPtr getTBFont(float size, bool antialiased) {
		FontPtr font;
		try {
			std::string path = GuiUtils::getPathA("Font.ttf");
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
