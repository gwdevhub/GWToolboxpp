#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Windows.h>

#include "Timer.h"

class ChatCommands {
	typedef std::function<void(std::vector<std::wstring>)> Handler_t;
	const float DEFAULT_CAM_SPEED = 25.0f;

public:
	ChatCommands();

	bool ProcessMessage(LPMSG msg);

	void UpdateUI();
	void MainRoutine();

	void SetSuppressMessages(bool active) { suppress_messages_active = active; }

private:
	void AddCommand(std::wstring cmd, Handler_t, bool override = true);
	bool IsTyping() { return (*(DWORD*)0xA377C8) != 0; }
	
	static std::wstring GetLowerCaseArg(std::vector<std::wstring>, size_t index);

	static void CmdAge2(std::vector<std::wstring>);
	static void CmdPcons(std::vector<std::wstring> args);
	static void CmdDialog(std::vector<std::wstring> args);
	static void CmdTB(std::vector<std::wstring> args);
	static void CmdTP(std::vector<std::wstring> args);
	static void CmdDamage(std::vector<std::wstring> args);
	static void CmdChest(std::vector<std::wstring> args);
	static void CmdAfk(std::vector<std::wstring> args);
	static void CmdTarget(std::vector<std::wstring> args);
	static void CmdUseSkill(std::vector<std::wstring> args);

	static void CmdZoom(std::vector<std::wstring> args);
	static void CmdCamera(std::vector<std::wstring> args);

	int move_forward;
	int move_side;
	int move_up;
	float cam_speed_;

	bool suppress_messages_active;
	bool suppress_next_message;

	int skill_to_use;
	float skill_usage_delay;
	clock_t skill_timer;
};
