#pragma once

#include <initializer_list>

#include <GWCA\Packets\StoC.h>

class ChatFilter {
public:
	ChatFilter();

	void SetSuppressMessages(bool active) { suppress_messages_active = active; }

private:
	const wchar_t* Get1stSegment(GW::Packet::StoC::P081* pak) const;
	const wchar_t* Get2ndSegment(GW::Packet::StoC::P081* pak) const;
	bool FullMatch(GW::Packet::StoC::P081* pak, 
		const std::initializer_list<wchar_t>& msg) const;


	DWORD GetNumericSegment(GW::Packet::StoC::P081* pak) const;
	bool ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const;
	bool ShouldIgnoreItem(const wchar_t* item_segment) const;
	bool ShouldIgnore(GW::Packet::StoC::P081* pak) const;

	bool suppress_messages_active;
	bool suppress_next_message;
};
