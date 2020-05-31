#pragma once

#include <GWCA\Utilities\Hook.h>

#include <GWCA\GameEntities\Item.h>

#include <GWCA\Packets\StoC.h>

#include <GWCA\Managers\UIMgr.h>

#include <Defines.h>

#include "GuiUtils.h"
#include "ToolboxWindow.h"

class InfoWindow : public ToolboxWindow {
	InfoWindow() {};
	~InfoWindow() {
		ClearAvailableDialogs();
	};
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


	static void CmdResignLog(const wchar_t* cmd, int argc, wchar_t** argv);
	static void OnInstanceLoad(GW::HookStatus*, GW::Packet::StoC::InstanceLoadFile*);
	static void OnMessageCore(GW::HookStatus*, GW::Packet::StoC::MessageCore*);

private:
	enum Status {
		Unknown,
		NotYetConnected,
		Connected,
		Resigned,
		Left
	};

	struct AvailableDialog {
		AvailableDialog(wchar_t* _message, uint32_t dialog_id) {
			GW::UI::AsyncDecodeStr(_message, &msg_ws);
			snprintf(dialog_buf, sizeof(dialog_buf), "0x%X", dialog_id);
		};
		std::wstring msg_ws;
		std::string msg_s;
		char dialog_buf[11];
	};

	std::vector<AvailableDialog*> available_dialogs;


	static const char* GetStatusStr(Status status);

	void PrintResignStatus(wchar_t *buffer, size_t size, size_t index, const wchar_t *player_name);
	void DrawResignlog();

	void ClearAvailableDialogs() {
		for (auto dialog : available_dialogs) {
			delete dialog;
		}
		available_dialogs.clear();
	}

	struct ForDecode {
		std::wstring enc_ws;
		std::wstring dec_ws;
		std::string dec_s;
		inline void init(const wchar_t* enc) {
			if (enc_ws == enc)
				return;
			enc_ws = enc;
			enc_ws.clear();
			dec_ws.clear();
			dec_s.clear();
			GW::UI::AsyncDecodeStr(enc, &dec_ws);
		}
		inline char* str() {
			if (dec_s.empty() && !dec_ws.empty()) {
				static std::wregex repl(L"<[^>]+>");
				std::wstring ws_repl = std::regex_replace(dec_ws, repl, L"");
				dec_s = GuiUtils::WStringToString(ws_repl);
			}
			return (char*)dec_s.c_str();
		}
	};
	void DrawItemInfo(GW::Item* item, ForDecode* name);
	DWORD mapfile = 0;

	std::vector<Status> status;
	std::vector<unsigned long> timestamp;

	std::queue<std::wstring> send_queue;
	clock_t send_timer = 0;

	uint32_t quoted_item_id = 0;

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
	GW::HookEntry OnDialog_Entry;
};
