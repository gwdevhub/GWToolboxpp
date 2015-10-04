#include "ChatMgr.h"

#include "MemoryMgr.h"
#include "CtoSMgr.h"

void GWAPI::ChatMgr::SendChat(const wchar_t* msg, wchar_t channel)
{
	static P5E_SendChat* chat = new P5E_SendChat();

	chat->channel = channel;
	wcscpy_s(chat->msg, msg);

	parent_->CtoS()->SendPacket<P5E_SendChat>(chat);
}

void GWAPI::ChatMgr::WriteChatF(const wchar_t* format, ...)
{
	static wchar_t* chat = NULL;


	if (chat != NULL) delete[] chat;

	va_list vl;
	va_start(vl, format);
	size_t szbuf = _vscwprintf(format, vl) + 1;
	chat = new wchar_t[szbuf];
	vswprintf_s(chat, szbuf, format, vl);
	va_end(vl);

	WriteChat(chat);
}

void GWAPI::ChatMgr::WriteChat(const wchar_t* msg, const wchar_t* from) {

	((void(__fastcall *)(DWORD, const wchar_t*, const wchar_t*))
		MemoryMgr::WriteChatFunction)
		(0, from, msg);
}
