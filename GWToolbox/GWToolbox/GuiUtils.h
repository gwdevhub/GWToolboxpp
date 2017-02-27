#pragma once

#include <string>

namespace GuiUtils {
	// Returns the settings folder as std::string
	std::string getSettingsFolder();

	// Returns the path of the given file in the settings folder as std::string
	std::string getPath(std::string file);

	// Returns the path of the given file in the subdirectory 
	// in the settings folder as string
	std::string getSubPath(std::string file, std::string subdir);

	enum FontSize {
		f10,
		f11,
		f12,
		f16,
		f26,
		f30
	};
	void LoadFonts();

	

	int GetPartyHealthbarHeight();
};

namespace ImGui {
	// Shows '(?)' and the helptext when hovered
	void ShowHelp(const char* help);
}
