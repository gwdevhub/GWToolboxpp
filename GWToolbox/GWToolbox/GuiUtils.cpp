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

namespace {
	ImFont* font16 = nullptr;
	ImFont* font18 = nullptr;
	ImFont* font20 = nullptr;
	ImFont* font24 = nullptr;
	ImFont* font42 = nullptr;
	ImFont* font48 = nullptr;
}

void GuiUtils::LoadFonts() {
	std::string fontfile = getPath("Font.ttf");
	ImGuiIO& io = ImGui::GetIO();
	font16 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 16.0f);
	font18 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 18.0f);
	font20 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 20.0f);
	font24 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 24.0f);
	font42 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 42.0f);
	font48 = io.Fonts->AddFontFromFileTTF(fontfile.c_str(), 48.0f);
}
ImFont* GuiUtils::GetFont(GuiUtils::FontSize size) {
	ImFont* font = [](FontSize size) -> ImFont* {
		switch (size) {
		case GuiUtils::f16: return font16;
		case GuiUtils::f18: return font18;
		case GuiUtils::f20:	return font20;
		case GuiUtils::f24:	return font24;
		case GuiUtils::f42:	return font42;
		case GuiUtils::f48:	return font48;
		default: return nullptr;
		}
	}(size);
	if (font && font->IsLoaded()) {
		return font;
	} else {
		return ImGui::GetIO().Fonts->Fonts[0];
	}
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
