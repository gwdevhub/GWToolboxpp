#pragma once

#include <set>
#include <string>
#include <regex>
#include <vector>
#include <initializer_list>

#include <GWCA\Packets\StoC.h>

#include "ToolboxModule.h"

class ChatFilter : public ToolboxModule {
	ChatFilter() {};
	~ChatFilter() {};

	// @Remark:
	// The text buffer will only be parsed if there was no modification within this period of time.
	// It can be re-ajusted to be more enjoyable.
	static const uint32_t NOISE_REDUCTION_DELAY_MS = 1000;

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

	void Update(float delta) override;

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
	// Error messages on-screen
	bool invalid_target = true; // Includes other error messages, see ChatFilter.cpp.
	bool opening_chest_messages = true;
	bool inventory_is_full = false;
	bool item_cannot_be_used = true; // Includes other error messages, see ChatFilter.cpp.
	bool not_enough_energy = true; // Includes other error messages, see ChatFilter.cpp.
    bool item_already_identified = false;

	bool messagebycontent;

	static const size_t FILTER_BUF_SIZE = 1024*16;

	// Chat filter
	std::vector<std::string> bycontent_words;
	char bycontent_word_buf[FILTER_BUF_SIZE];
	bool bycontent_filedirty = false;

	std::vector<std::regex> bycontent_regex;
	char bycontent_regex_buf[FILTER_BUF_SIZE];

#ifdef EXTENDED_IGNORE_LIST
	bool messagebyauthor;
	std::set<std::string> byauthor_words;
	char byauthor_buf[FILTER_BUF_SIZE];
	bool byauthor_filedirty = false;
#endif

	uint32_t timer_parse_filters = 0;
	uint32_t timer_parse_regexes = 0;

	void ParseBuffer(const char *text, std::vector<std::string>  &words) const;
	void ParseBuffer(const char *text, std::vector<std::wstring> &words) const;
	void ParseBuffer(const char* text, std::vector<std::regex> &regex) const;

	//void ByContent_ParseBuf() {
	//		ParseBuffer(bycontent_buf, bycontent_regex);
	//	} else {
	//		ParseBuffer(bycontent_buf, bycontent_words);
	//	}
	//}

#ifdef EXTENDED_IGNORE_LIST
	void ByAuthor_ParseBuf() {
		ParseBuffer(byauthor_buf, byauthor_words);
	}
#endif
};
