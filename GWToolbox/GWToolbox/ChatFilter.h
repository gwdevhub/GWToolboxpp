#pragma once

#include <GWCA\StoCPackets.h>

class ChatFilter {
public:
	ChatFilter();

	void SetSuppressMessages(bool active) { suppress_messages_active = active; }

private:
	const wchar_t* Get1stSegment(GWCA::StoC_Pak::P081* pak) const;
	const wchar_t* Get2ndSegment(GWCA::StoC_Pak::P081* pak) const;
	

	DWORD GetNumericSegment(GWCA::StoC_Pak::P081* pak) const;
	bool ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const;
	bool ShouldIgnoreItem(const wchar_t* item_segment) const;
	bool ShouldIgnore(GWCA::StoC_Pak::P081* pak) const;

	bool suppress_messages_active;
	bool suppress_next_message;
};
