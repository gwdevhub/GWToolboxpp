#pragma once

#include <set>
#include <string>
#include <vector>
#include <initializer_list>
#include <GWCA\Packets\StoC.h>

#include "ToolboxModule.h"

class ChatFilter : public ToolboxModule {
	ChatFilter() {};
	~ChatFilter() {};

public:
	static ChatFilter& Instance() {
		static ChatFilter instance;
		return instance;
	}

	const char* Name() const override { return "Chat Filter"; }

	void Initialize() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	const wchar_t* Get1stSegment(const wchar_t *message) const;
	const wchar_t* Get2ndSegment(const wchar_t *message) const;
	bool FullMatch(const wchar_t* p, const std::initializer_list<wchar_t>& msg) const;

	DWORD GetNumericSegment(const wchar_t *message) const;
	bool ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const;
	bool IsRare(const wchar_t* item_segment) const;
	bool ShouldIgnore(const wchar_t *message);
	bool ShouldIgnoreByContent(const wchar_t *message, size_t size);
	bool ShouldIgnoreBySender(const wchar_t *sender, size_t size);

	bool self_drop_rare;
	bool self_drop_common;
	bool ally_drop_rare;
	bool ally_drop_common;
	bool ally_pickup_rare;
	bool ally_pickup_common;
	bool skill_points;
	bool pvp_messages;
	bool hoh;
	bool favor;
	bool ninerings;
	bool noonehearsyou;
	bool lunars;
	bool away;
	bool you_have_been_playing_for;
	bool player_has_achieved_title;

	bool messagebycontent;

	static const size_t FILTER_BUF_SIZE = 1024*16;

	// Chat filter
	std::vector<std::string> bycontent_words;
	char bycontent_buf[FILTER_BUF_SIZE];
	bool bycontent_filedirty = false;

#ifdef EXTENDED_IGNORE_LIST
	bool messagebyauthor;
	std::set<std::string> byauthor_words;
	char byauthor_buf[FILTER_BUF_SIZE];
	bool byauthor_filedirty = false;
#endif

	static void ParseBuffer(const char *text, std::vector<std::string> &words);
	static void ParseBuffer(const char *text, std::set<std::string>    &words);

	void ByContent_ParseBuf() {
		ParseBuffer(bycontent_buf, bycontent_words);
	}

#ifdef EXTENDED_IGNORE_LIST
	void ByAuthor_ParseBuf() {
		ParseBuffer(byauthor_buf, byauthor_words);
	}
#endif
};
