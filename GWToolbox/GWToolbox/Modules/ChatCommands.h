#pragma once

#include <string>
#include <vector>
#include <functional>
#include <Windows.h>
#include <time.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

class ChatCommands : public ToolboxModule {
	const float DEFAULT_CAM_SPEED = 25.0f;

	ChatCommands() : move_forward(0), move_side(0), move_up(0),
		cam_speed_(DEFAULT_CAM_SPEED),
		skill_to_use(0), skill_usage_delay(1.0f), skill_timer(clock()) {};
	~ChatCommands() {};
public:
	static ChatCommands& Instance() {
		static ChatCommands instance;
		return instance;
	}

	const char* Name() const override { return "Chat Commands"; }

	void Initialize() override;

	void DrawSettings() override {};

	void DrawHelp();

	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	// Update. Will always be called every frame.
	void Update() override;

private:
	// 0xA37890 is a mirror of 0xA377C8.
	bool IsTyping() { return (*(DWORD*)0xA377C8) != 0; } 
	
	static bool ReadTemplateFile(std::wstring path, char *buff, size_t buffSize);

	static void CmdAge2(int argc, LPWSTR *argv);
	static void CmdDialog(int argc, LPWSTR *argv);
	static void CmdTB(int argc, LPWSTR *argv);
	static void CmdTP(int argc, LPWSTR *argv);
	static void CmdDamage(int argc, LPWSTR *argv);
	static void CmdChest(int argc, LPWSTR *argv);
	static void CmdAfk(int argc, LPWSTR *argv);
	static void CmdTarget(int argc, LPWSTR *argv);
	static void CmdUseSkill(int argc, LPWSTR *argv);
	static void CmdShow(int argc, LPWSTR *argv);
	static void CmdHide(int argc, LPWSTR *argv);
	static void CmdZoom(int argc, LPWSTR *argv);
	static void CmdCamera(int argc, LPWSTR *argv);
	static void CmdSCWiki(int argc, LPWSTR *argv);
	static void CmdLoad(int argc, LPWSTR *argv);

	static std::vector<ToolboxUIElement*> MatchingWindows(int argc, LPWSTR *argv);

	int move_forward;
	int move_side;
	int move_up;
	float cam_speed_;

	int skill_to_use;
	float skill_usage_delay;
	clock_t skill_timer;
};
