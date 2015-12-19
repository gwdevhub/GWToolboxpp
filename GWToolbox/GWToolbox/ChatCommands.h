#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Windows.h>

class ChatCommands {
	typedef std::function<void(std::vector<std::wstring>)> Handler_t;

public:
	ChatCommands();

	bool ProcessMessage(LPMSG msg);

	void UpdateUI();

private:
	void AddCommand(std::wstring cmd, Handler_t);
	
	static std::wstring GetLowerCaseArg(std::vector<std::wstring>, int index);

	static void CmdAge2(std::vector<std::wstring>);
	static void CmdPcons(std::vector<std::wstring> args);
	static void CmdDialog(std::vector<std::wstring> args);
	static void CmdTB(std::vector<std::wstring> args);
	static void CmdTP(std::vector<std::wstring> args);

	static void CmdZoom(std::vector<std::wstring> args);
	static void CmdCamera(std::vector<std::wstring> args);

	bool unlocked_camera;
	int forward_movement;
	int side_movement;
	int vertical_movement;
	const float speed = 10.0f;
};
