#pragma once

#include <set>
#include <string>

#include <GWCA\Packets\StoC.h>

#include "Settings.h"

class ChatFilter {
	static const wchar_t* IniSection() { return L"Chat FIlter"; }

public:
	ChatFilter();

	void DrawSettings();

private:
	const wchar_t* Get1stSegment(GW::Packet::StoC::P081* pak) const;
	const wchar_t* Get2ndSegment(GW::Packet::StoC::P081* pak) const;

	DWORD GetNumericSegment(GW::Packet::StoC::P081* pak) const;
	bool ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const;
	bool ShouldIgnoreItem(const wchar_t* item_segment) const;
	bool ShouldIgnore(GW::Packet::StoC::P081* pak);

	bool suppress_next_message;
	bool suppress_next_p081;

	SettingBool ally_common_drops;
	SettingBool self_common_drops;
	SettingBool ally_rare_drops;
	SettingBool self_rare_drops;
	SettingBool skill_point;
	SettingBool pvp_messages;
	SettingBool hoh;
	SettingBool favor;
	SettingBool shingjeabroadwalk;
	SettingBool noonehearsyou;

	SettingBool messagebycontent;
	std::set<std::string> filter_words;
};
