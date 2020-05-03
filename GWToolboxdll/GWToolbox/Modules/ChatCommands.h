#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/Utilities/Hook.h>

class ChatCommands : public ToolboxModule {
	const float DEFAULT_CAM_SPEED = 1000.f; // 600 units per sec
	const float ROTATION_SPEED = (float)M_PI / 3.f; // 6 seconds for full rotation

	ChatCommands() {};
	~ChatCommands() {};
public:
	static ChatCommands& Instance() {
		static ChatCommands instance;
		return instance;
	}

	const char* Name() const override { return "Chat Commands"; }

	const char* SettingsName() const override { return "Game Settings"; }

	void Initialize() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void DrawHelp() override;

	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	// Update. Will always be called every frame.
	void Update(float delta) override;

    static void CmdReapplyTitle(const wchar_t* message, int argc, LPWSTR* argv);

private:
	static bool ReadTemplateFile(std::wstring path, char *buff, size_t buffSize);
	static bool ParseDistrict(const std::wstring s, GW::Constants::District& district, int& number);
	static bool ParseOutpost(const std::wstring s, GW::Constants::MapID& outpost, GW::Constants::District& district, int& number);
    static bool IsLuxon();

	static void CmdAge2(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdDialog(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdTB(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdTP(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdDamage(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdChest(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdAfk(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdTarget(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdUseSkill(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdShow(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdHide(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdZoom(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdCamera(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdSCWiki(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdLoad(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdTransmo(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdResize(const wchar_t *message, int argc, LPWSTR *argv);
	static void CmdPingEquipment(const wchar_t* message, int argc, LPWSTR* argv);
	static void CmdTransmoTarget(const wchar_t* message, int argc, LPWSTR* argv);
	static void CmdTransmoParty(const wchar_t* message, int argc, LPWSTR* argv);
    static void CmdTransmoAgent(const wchar_t* message, int argc, LPWSTR* argv);
    

	static std::vector<ToolboxUIElement*> MatchingWindows(const wchar_t *message, int argc, LPWSTR *argv);

	static GW::Constants::MapID MatchMapPrefix(const wchar_t *map_name);

	float cam_speed = DEFAULT_CAM_SPEED;
	bool forward_fix_z = true;

	void AddSkillToUse(int skill); // 1-8 range
	std::list<int> skills_to_use; // 0-7 range
	float skill_usage_delay = 1.0f;
	clock_t skill_timer = clock();
protected:
	const float SettingsWeighting() override { return  1.2f; };
};
