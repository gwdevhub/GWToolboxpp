#pragma once

#include <vector>

#include "OSHGui\OSHGui.hpp"

class EditBuild : public OSHGui::Panel {
public:
	const int WIDTH = 450;
	const int N_PLAYERS = 12;
	const int ITEM_HEIGHT = 26;
	const int NAME_LEFT = 30;
	const int NAME_WIDTH = 150;
	const int SEND_WIDTH = 45;

	EditBuild();
	
	void SetEditedBuild(int index, OSHGui::Button* button);
	void SetPanelPosition(bool left) { left_ = left; UpdateLocation(); }

private:
	bool up_; // true if aligned with the top buildpanel border, false if aligned with the bottom border
	bool left_; // true if showing to the left of buildpanel, false if showing on the right
	
	int editing_index;
	OSHGui::Button* editing_button;

	OSHGui::TextBox* name;
	std::vector<OSHGui::TextBox*> names;
	std::vector<OSHGui::TextBox*> templates;
	OSHGui::CheckBox* show_numbers;

	void UpdateLocation();
	void EnableTextInput(OSHGui::TextBox* tb);
	void SaveBuild();
	inline std::string ToString(std::wstring s) {
		return std::string(s.begin(), s.end());
	}
	inline std::wstring ToWString(std::string s) {
		return std::wstring(s.begin(), s.end());
	}
};
