#include "stdafx.h"

#include "GuiUtils.h"

#include <imgui.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>

#include <Modules/Resources.h>

namespace {
	ImFont* font16 = nullptr;
	ImFont* font18 = nullptr;
	ImFont* font20 = nullptr;
	ImFont* font24 = nullptr;
	ImFont* font42 = nullptr;
	ImFont* font48 = nullptr;
}

void GuiUtils::LoadFonts() {
	utf8::string fontfile = Resources::GetPathUtf8(L"Font.ttf");
	ImGuiIO& io = ImGui::GetIO();
	font16 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 16.0f);
	font18 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 18.0f);
	font20 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 20.0f);
	font24 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 24.0f);
	font42 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 42.0f);
	font48 = io.Fonts->AddFontFromFileTTF(fontfile.bytes, 48.0f);
	free(fontfile);
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
	GW::Constants::InterfaceSize interfacesize =
		static_cast<GW::Constants::InterfaceSize>(GW::UI::GetPreference(GW::UI::Preference_InterfaceSize));

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
bool GuiUtils::ParseInt(const char *str, int *val, int base) {
	char *end;
	*val = strtol(str, &end, base);
	if (*end != 0 || errno == ERANGE)
		return false;
	else
		return true;
}
bool GuiUtils::ParseInt(const wchar_t *str, int *val, int base) {
	wchar_t *end;
	*val = wcstol(str, &end, base);
	if (*end != 0 || errno == ERANGE)
		return false;
	else
		return true;
}
bool GuiUtils::ParseFloat(const char *str, float *val) {
	char *end;
	*val = strtof(str, &end);
	return str != end && errno != ERANGE;
}
bool GuiUtils::ParseFloat(const wchar_t *str, float *val) {
	wchar_t *end;
	*val = wcstof(str, &end);
	return str != end && errno != ERANGE;
}
bool GuiUtils::ParseUInt(const char *str, unsigned int *val, int base) {
	char *end;
	*val = strtoul(str, &end, base);
	if (str == end || errno == ERANGE)
		return false;
	else
		return true;
}
bool GuiUtils::ParseUInt(const wchar_t *str, unsigned int *val, int base) {
	wchar_t *end;
	*val = wcstoul(str, &end, base);
	if (str == end || errno == ERANGE)
		return false;
	else
		return true;
}
std::wstring GuiUtils::ToWstr(std::string &s) {
	std::wstring result;
	result.reserve(s.size() + 1);
	for (char c : s) result.push_back(c);
	return result;
}
size_t GuiUtils::wcstostr(char *dest, const wchar_t *src, size_t n) {
	size_t i;
    unsigned char *d = (unsigned char *)dest;
    for (i = 0; i < n; i++) {
        if (src[i] & ~0x7f)
            return 0;
        d[i] = src[i] & 0x7f;
        if (src[i] == 0) break;
    }
    return i;
}
char *GuiUtils::StrCopy(char *dest, const char *src, size_t dest_size) {
	strncpy(dest, src, dest_size - 1);
	dest[dest_size - 1] = 0;
	return dest;
}
