#pragma once

#include <string>
#include "ImGuiAddons.h"

namespace GuiUtils {
	// Returns the settings folder as std::string
	std::string getSettingsFolder();

	// Returns the path of the given file in the settings folder as std::string
	std::string getPath(std::string file);

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as string
	std::string getSubPath(std::string file, std::string subdir);

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
};
