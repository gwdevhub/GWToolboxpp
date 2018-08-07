#pragma once

#include <string>
#include "ImGuiAddons.h"

namespace GuiUtils {
	enum FontSize {
		f16,
		f18,
		f20,
		f24,
		f42,
		f48
	};
	void LoadFonts();
	ImFont* GetFont(FontSize size);

	int GetPartyHealthbarHeight();

	std::string ToLower(std::string s);
	std::wstring ToLower(std::wstring s);

	bool ParseInt(const char *str, int *val);
	bool ParseInt(const wchar_t *str, int *val);

	std::wstring ToWstr(std::string &s);

	size_t wcstostr(char *dest, const wchar_t *src, size_t n);
};
