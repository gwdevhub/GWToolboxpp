#pragma once

#include <set>
#include <string>
#include <initializer_list>

#include <GWCA\Packets\StoC.h>

#include "ToolboxModule.h"

#include "Settings.h"

class ChatFilter : public ToolboxModule {
public:
	const char* Name() override { return "Chat Filter"; }

	ChatFilter();

	void DrawSettings() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	const wchar_t* Get1stSegment(GW::Packet::StoC::P081* pak) const;
	const wchar_t* Get2ndSegment(GW::Packet::StoC::P081* pak) const;
	bool FullMatch(GW::Packet::StoC::P081* pak, 
		const std::initializer_list<wchar_t>& msg) const;


	DWORD GetNumericSegment(GW::Packet::StoC::P081* pak) const;
	bool ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const;
	bool ShouldIgnoreItem(const wchar_t* item_segment) const;
	bool ShouldIgnore(GW::Packet::StoC::P081* pak);

	bool suppress_next_message;
	bool suppress_next_p081;

	bool self_common_drops;
	bool ally_common_drops;
	bool self_rare_drops;
	bool ally_rare_drops;
	bool skill_points;
	bool pvp_messages;
	bool hoh;
	bool favor;
	bool ninerings;
	bool noonehearsyou;
	bool lunars;
	bool playeraway;

	bool messagebycontent;
#define FILTER_BUF_SIZE 1024*16
	char bycontent_buf[FILTER_BUF_SIZE];
	std::set<std::string> bycontent_words;

	bool messagebyauthor;
	char byauthor_buf[FILTER_BUF_SIZE];
	std::set<std::string> byauthor_words;

	void ParseBuffer(const char* buf, std::set<std::string>& words);
	void ByContent_ParseBuf() {
		ParseBuffer(bycontent_buf, bycontent_words);
	}
	void ByAuthor_ParseBuf() {
		ParseBuffer(byauthor_buf, byauthor_words);
	}
};
