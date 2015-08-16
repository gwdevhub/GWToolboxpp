#include "ChatMgr.h"


void GWAPI::ChatMgr::SendChat(wchar_t* msg, wchar_t channel)
{
	static P5E_SendChat* chat = NULL;

	if (chat != NULL) delete chat;
	chat = new P5E_SendChat();

	chat->channel = channel;
	wcscpy_s(chat->msg, msg);

	parent->CtoS->SendPacket<P5E_SendChat>(chat);
}

void GWAPI::ChatMgr::WriteToChat(wchar_t* format, ...)
{
	static wchar_t* chat = NULL;
	static wchar_t* name = L"GWToolbox++";


	if (chat != NULL) delete chat;

	va_list vl;
	va_start(vl, format);
	size_t szbuf = _vscwprintf(format, vl);
	chat = new wchar_t[szbuf + 1];
	vwprintf_s(format, vl);
	va_end(vl);

	((void(__fastcall *)(DWORD, wchar_t*, wchar_t*))
		MemoryMgr::WriteChatFunction)
		(0, name, chat);
}
