#pragma once

#include <string>

#include <GWCA\GWCA.h>
#include <GWCA\ChatMgr.h>

class ChatLogger {
public:

	static void Init() {
		GWCA::Chat().RegisterChannel(L"GWToolbox++", 0x00CCFF);
	}

	inline static void Log(const wchar_t* msg) {
		GWCA::Chat().WriteChat(L"GWToolbox++", msg);
	}

	inline static void Log(const std::wstring msg) {
		GWCA::Chat().WriteChat(L"GWToolbox++", msg.c_str());
	}

	static void LogF(const wchar_t* format, ...) {
		va_list vl;
		va_start(vl, format);
		size_t szbuf = _vscwprintf(format, vl) + 1;
		wchar_t* chat = new wchar_t[szbuf];
		vswprintf_s(chat, szbuf, format, vl);
		va_end(vl);

		GWCA::Chat().WriteChat(L"GWToolbox++", chat);

		delete[] chat;
	}
};
