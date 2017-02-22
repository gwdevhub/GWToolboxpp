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

	static void Err(const char* format, ...) {
		char buf1[256];
		va_list vl;
		va_start(vl, format);
		vsprintf_s(buf1, format, vl);
		va_end(vl);
		
		char buf2a[256];
		char buf2b[256];
		sprintf_s(buf2a, "<c=#00ccff>GWToolbox++</c>: %s", buf1);
		sprintf_s(buf2b, "[Warning] %s", buf1);
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA5, buf2a);
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA4, buf2b);
	}

	static void Log(const char* format, ...) {
		char buf1[256];
		va_list vl;
		va_start(vl, format);
		vsprintf_s(buf1, format, vl);
		va_end(vl);
		
		char buf2[256];
		sprintf_s(buf2, "<c=#00ccff>GWToolbox++</c>: %s", buf1);
		GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA3, buf2);
	}
};
