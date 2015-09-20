#pragma once

#include <vector>
#include "../include/OSHGui/OSHGui.hpp"

class EditBuild : public OSHGui::Panel {
public:
	const int WIDTH = 450;
	const int N_PLAYERS = 12;
	const int ITEM_HEIGHT = 24;
	const int NAME_LEFT = 30;
	const int NAME_WIDTH = 150;
	const int SEND_WIDTH = 45;

	EditBuild();
	
	void SetEditedBuild(int index, OSHGui::Button* button);

private:
	int editing_index;
	OSHGui::Button* editing_button;

	OSHGui::TextBox* name;
	std::vector<OSHGui::TextBox*> names;
	std::vector<OSHGui::TextBox*> templates;
	OSHGui::CheckBox* show_numbers;

	void EnableTextInput(OSHGui::TextBox* tb);
	void SaveBuild();
	inline std::string ToString(std::wstring s) {
		return std::string(s.begin(), s.end());
	}
	inline std::wstring ToWString(std::string s) {
		return std::wstring(s.begin(), s.end());
	}
};
