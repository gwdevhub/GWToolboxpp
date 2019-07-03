#pragma once

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

	bool ParseInt(const char *str, int *val, int base = 0);
	bool ParseInt(const wchar_t *str, int *val, int base = 0);

	bool ParseUInt(const char *str, unsigned int *val, int base = 0);
	bool ParseUInt(const wchar_t *str, unsigned int *val, int base = 0);

	bool ParseFloat(const char *str, float *val);
	bool ParseFloat(const wchar_t *str, float *val);

	std::wstring ToWstr(std::string &s);

	size_t wcstostr(char *dest, const wchar_t *src, size_t n);

	char *StrCopy(char *dest, const char *src, size_t dest_size);
};
