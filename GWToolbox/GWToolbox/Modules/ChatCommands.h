#pragma once

#include <string>
#include <vector>
#include <functional>
#include <Windows.h>
#include <time.h>

#include <GWCA\Constants\Constants.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

class ChatCommands : public ToolboxModule {
	const float DEFAULT_CAM_SPEED = 1000.f; // 600 units per sec
	const float ROTATION_SPEED = (float)M_PI / 3.f; // 6 seconds for full rotation

	ChatCommands() : cam_speed(DEFAULT_CAM_SPEED) {};
	~ChatCommands() {};
public:
	static ChatCommands& Instance() {
		static ChatCommands instance;
		return instance;
	}

	const char* Name() const override { return "Chat Commands"; }

	void Initialize() override;

	void DrawSettingInternal() override;

	void DrawHelp();

	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	// Update. Will always be called every frame.
	void Update(float delta) override;

private:
	static bool ReadTemplateFile(std::wstring path, char *buff, size_t buffSize);
	static void ParseDistrict(const std::wstring& s, GW::Constants::District& district, int& number);

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
	static void CmdTransmo(int argc, LPWSTR *argv);

	static std::vector<ToolboxUIElement*> MatchingWindows(int argc, LPWSTR *argv);

	float cam_speed;
	bool forward_fix_z;

	void ToggleSkill(int skill); // 1-8 range
	std::list<int> skills_to_use; // 0-7 range
	float skill_usage_delay = 1.0f;
	clock_t skill_timer = clock();
};
