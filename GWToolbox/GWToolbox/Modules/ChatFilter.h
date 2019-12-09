#pragma once

#include <GWCA\Utilities\Hook.h>
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

	bool self_drop_rare = false;
	bool self_drop_common = false;
	bool ally_drop_rare = false;
	bool ally_drop_common = false;
	bool ally_pickup_rare = false;
	bool ally_pickup_common = false;
	bool skill_points = false;
	bool pvp_messages = true;
	bool hoh = false;
	bool favor = false;
	bool ninerings = true;
	bool noonehearsyou = true;
	bool lunars = true;
	bool away = false;
	bool you_have_been_playing_for = false;
	bool player_has_achieved_title = false;

	bool messagebycontent = false;

	static const size_t FILTER_BUF_SIZE = 1024*16;

	// Chat filter
	std::vector<std::string> bycontent_words;
	char bycontent_word_buf[FILTER_BUF_SIZE] = "";
	bool bycontent_filedirty = false;

	std::vector<std::regex> bycontent_regex;
	char bycontent_regex_buf[FILTER_BUF_SIZE] = "";

#ifdef EXTENDED_IGNORE_LIST
	bool messagebyauthor = false;
	std::set<std::string> byauthor_words;
	char byauthor_buf[FILTER_BUF_SIZE] = "";
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

	GW::HookEntry LocalMessageCallback_Entry;

	GW::HookEntry MessageServer_Entry;
	GW::HookEntry MessageGlobal_Entry;
	GW::HookEntry MessageLocal_Entry;
};
