#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Windows.h>

#include "Timer.h"
#include "ChatFilter.h"

class ChatCommands {
	const float DEFAULT_CAM_SPEED = 25.0f;

public:
	ChatCommands();

	bool ProcessMessage(LPMSG msg);

	// Update. Will always be called every frame.
	void Main();

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() {}

	void SetSuppressMessages(bool active) { chat_filter->SetSuppressMessages(active); }

private:
	bool IsTyping() { return false; /* (*(DWORD*)0xA377C8) != 0;*/ } // broken
	
	static std::wstring GetLowerCaseArg(std::vector<std::wstring>, size_t index);

	static bool CmdAge2(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdPcons(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdDialog(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdTB(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdTP(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdDamage(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdChest(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdAfk(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdTarget(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdUseSkill(std::wstring& cmd, std::vector<std::wstring>& args);

	static bool CmdZoom(std::wstring& cmd, std::vector<std::wstring>& args);
	static bool CmdCamera(std::wstring& cmd, std::vector<std::wstring>& args);

	ChatFilter* chat_filter;

	int move_forward;
	int move_side;
	int move_up;
	float cam_speed_;

	int skill_to_use;
	float skill_usage_delay;
	clock_t skill_timer;
};
