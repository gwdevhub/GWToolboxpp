#pragma once
#include "APIMain.h"
#include <string>



namespace GWAPI {

	class ChatMgr{
		friend class GWAPIMgr;
		GWAPIMgr* parent;
		ChatMgr(GWAPIMgr* obj) : parent(obj) {}

		struct P5E_SendChat{
			const DWORD header = 0x5E;
			wchar_t channel;
			wchar_t msg[137];
		};

	public:

		// Sendchat, should be self explanatory. SendChat(L"I love gwtoolbox",L'!');
		void SendChat(const wchar_t* msg, wchar_t channel);

		// Write to chat as a PM with printf style arguments.
		void WriteChatF(const wchar_t* format, ...);

		// Simple write to chat as a PM
		void WriteChat(const wchar_t* msg, const wchar_t* from = L"GWToolbox++");

	};

}