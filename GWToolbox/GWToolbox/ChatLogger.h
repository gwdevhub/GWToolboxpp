#pragma once

#include <string>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ChatMgr.h>

class ChatLogger {
public:

	static void Init() {
		GW::Chat().SetMessageColor(GW::Channel::CHANNEL_GWCA3, 0xFFFFFFFF);
		GW::Chat().SetMessageColor(GW::Channel::CHANNEL_GWCA5, 0xFFFF4444);
	}

	inline static void Err(const std::wstring msg) {
		std::wstring s1 = std::wstring(L"<c=#00ccff>GWToolbox++</c>: ") + msg;
		std::wstring s2 = std::wstring(L"[Warning]: ") + msg;
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA5, s1.c_str());
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA4, s2.c_str());
	}

	static void ErrF(const wchar_t* format, ...) {
		va_list vl;
		va_start(vl, format);
		size_t szbuf = _vscwprintf(format, vl) + 1;
		wchar_t* chat = new wchar_t[szbuf];
		vswprintf_s(chat, szbuf, format, vl);
		va_end(vl);

		Err(std::wstring(chat));

		delete[] chat;
	}

	inline static void Log(const std::wstring msg) {
		std::wstring s = std::wstring(L"<c=#00ccff>GWToolbox++</c>: ") + msg;
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA3, s.c_str());
	}
	inline static void Log(const std::string msg) {
		Log(std::wstring(msg.begin(), msg.end()));
	}

	inline static void Log(const wchar_t* msg) {
		Log(std::wstring(msg));
	}

	static void LogF(const wchar_t* format, ...) {
		va_list vl;
		va_start(vl, format);
		size_t szbuf = _vscwprintf(format, vl) + 1;
		wchar_t* chat = new wchar_t[szbuf];
		vswprintf_s(chat, szbuf, format, vl);
		va_end(vl);

		Log(std::wstring(chat));
		
		delete[] chat;
	}
};
