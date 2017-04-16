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
};
