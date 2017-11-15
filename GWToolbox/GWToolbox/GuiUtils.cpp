#include "GuiUtils.h"

#include <algorithm>
#include <Shlobj.h>
#include <Shlwapi.h>

#include <imgui.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\Utilities\Scanner.h>

#include <Modules\Resources.h>

namespace {
	ImFont* font16 = nullptr;
	ImFont* font18 = nullptr;
	ImFont* font20 = nullptr;
	ImFont* font24 = nullptr;
	ImFont* font42 = nullptr;
	ImFont* font48 = nullptr;
}

void GuiUtils::LoadFonts() {
	std::string fontfile = Resources::GetPath("Font.ttf");
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

std::string GuiUtils::ToLower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}
std::wstring GuiUtils::ToLower(std::wstring s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

bool GuiUtils::ParseInt(const char *str, int *val) {
	char *end;
	*val = strtol(str, &end, 0);
	return *end == 0;
}
bool GuiUtils::ParseInt(const wchar_t *str, int *val) {
	wchar_t *end;
	*val = wcstol(str, &end, 0);
	return *end == 0;
}

int GuiUtils::ConvertToUtf8(const wchar_t *str, char *output, size_t max_size) {
	size_t len = wcslen(str);
	if (len > max_size) return 0;
	int nbytes = WideCharToMultiByte(CP_UTF8, 0, str, -1, output, max_size, NULL, NULL);
	return nbytes;
}

int GuiUtils::ConvertToUtf8(const std::wstring& str, char *output, size_t max_size) {
	return ConvertToUtf8(str.c_str(), output, max_size);
}