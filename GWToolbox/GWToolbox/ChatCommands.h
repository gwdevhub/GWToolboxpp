#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Windows.h>
#include <time.h>

#include "ToolboxModule.h"

class ChatCommands : public ToolboxModule {
	const float DEFAULT_CAM_SPEED = 25.0f;

public:
	const char* Name() const override { return "Chat Commands"; }

	ChatCommands();

	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	// Update. Will always be called every frame.
	void Update() override;

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

	int move_forward;
	int move_side;
	int move_up;
	float cam_speed_;

	int skill_to_use;
	float skill_usage_delay;
	clock_t skill_timer;
};
