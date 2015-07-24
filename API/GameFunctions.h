#pragma once

#include "GameLoop.h"

namespace GWAPI{

	void SendPacket(DWORD Size, DWORD Header, ...);

	void Dialog(DWORD ID);

	void GoNPC(DWORD AgentID, DWORD CallTarget);

	void UseItem(DWORD ItemID);

	void WriteChat(wchar_t* message, ...);

	void SendChat(wchar_t* Message, wchar_t Channel = '!');

}