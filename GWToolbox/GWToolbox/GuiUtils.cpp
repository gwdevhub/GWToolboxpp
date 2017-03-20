#include "GuiUtils.h"

#include <Shlobj.h>
#include <Shlwapi.h>

#include <imgui.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\Utilities\Scanner.h>

std::string GuiUtils::getSettingsFolder() {
	CHAR szPath[MAX_PATH];
	szPath[0] = '\0';
	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
	return std::string(szPath) + "\\GWToolboxpp";
}

std::string GuiUtils::getPath(std::string file) {
	return getSettingsFolder() + "\\" + file;
}

std::string GuiUtils::getSubPath(std::string file, std::string subdir) {
	return getSettingsFolder() + "\\" + subdir + "\\" + file;
}

void GuiUtils::LoadFonts() {
	std::string fontfile = getPath("Font.ttf");
	ImGuiIO& io = ImGui::GetIO();
	ImFont* f10 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 16.0f);
	ImFont* f11 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 18.0f);
	ImFont* f12 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 19.0f);
	ImFont* f16 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 24.0f);
	ImFont* f26 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 42.0f);
	ImFont* f30 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 48.0f);
}

int GuiUtils::GetPartyHealthbarHeight() {
	static DWORD* optionarray = nullptr;
	if (!optionarray) {
		optionarray = (DWORD*)GW::Scanner::Find("\x8B\x4D\x08\x85\xC9\x74\x0A", "xxxxxxx", -9);
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
