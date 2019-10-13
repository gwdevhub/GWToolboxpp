#pragma once

#include <GWCA\Utilities\Hook.h>

#include <Defines.h>

#include "ToolboxWindow.h"

class InfoWindow : public ToolboxWindow {
	InfoWindow() {};
	~InfoWindow() {};
public:
	static InfoWindow& Instance() {
		static InfoWindow instance;
		return instance;
	}

	const char* Name() const override { return "Info"; }

	void Initialize() override;

	void Draw(IDirect3DDevice9* pDevice) override;
	void Update(float delta) override;

	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	enum Status {
		Unknown,
		NotYetConnected,
		Connected,
		Resigned,
		Left
	};

	static const char* GetStatusStr(Status status);

	void PrintResignStatus(wchar_t *buffer, size_t size, size_t index, const wchar_t *player_name);
	void DrawResignlog();

	DWORD mapfile = 0;

	std::vector<Status> status;
	std::vector<unsigned long> timestamp;

	std::queue<std::wstring> send_queue;
	clock_t send_timer = 0;

	bool show_widgets = true;
	bool show_open_chest = true;
	bool show_player = true;
	bool show_target = true;
	bool show_map = true;
	bool show_dialog = true;
	bool show_item = true;
	bool show_mobcount = true;
	bool show_quest = true;
	bool show_resignlog = true;

	GW::HookEntry MessageCore_Entry;
	GW::HookEntry InstanceLoadFile_Entry;
};
