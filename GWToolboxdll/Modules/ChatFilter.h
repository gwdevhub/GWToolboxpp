#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>

namespace GW {
    namespace Packet {
        namespace StoC {
            struct PacketBase;
        }
    }
}
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
    const char* SettingsName() const override { return "Chat Settings"; }

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
    // Sets HookStatus to blocked if packet has content that hits the filter.
    static void BlockIfApplicable(GW::HookStatus* status, GW::Packet::StoC::PacketBase* packet);
    // Ensure the message buffer is cleared if this packet has been blocked
    static void ClearMessageBufferIfBlocked(GW::HookStatus* status, GW::Packet::StoC::PacketBase*);

    // Should this message for this channel be ignored either by encoded string or content?
    bool ShouldIgnore(const wchar_t* message, uint32_t channel);
    // Should this message be ignored by encoded string?
    bool ShouldIgnore(const wchar_t *message);
    // Should this message be ignored by content?
    bool ShouldIgnoreByContent(const wchar_t *message, size_t size = 1024);
    bool ShouldIgnoreBySender(const std::wstring& sender);
    // Should this channel be checked for ignored messages?
    bool ShouldFilterByChannel(uint32_t channel);
    // Should this channel be blocked altogether?
    bool ShouldBlockByChannel(uint32_t channel);


    bool guild_announcement = false;
    bool self_drop_rare = false;
    bool self_drop_common = false;
    bool ally_drop_rare = false;
    bool ally_drop_common = false;
    bool ally_pickup_rare = false;
    bool ally_pickup_common = false;
    bool player_pickup_rare = false;
    bool player_pickup_common = false;
    bool salvage_messages = false;
    bool skill_points = false;
    bool pvp_messages = true;
    bool hoh_messages = false;
    bool favor = false;
    bool ninerings = true;
    bool noonehearsyou = true;
    bool lunars = true;
    bool away = false;
    bool you_have_been_playing_for = false;
    bool player_has_achieved_title = false;
    bool faction_gain = false;
    bool challenge_mission_messages = false;
    bool block_messages_from_inactive_channels = false;


    // Error messages on-screen
    bool invalid_target = false; // Includes other error messages, see ChatFilter.cpp.
    bool opening_chest_messages = false;
    bool inventory_is_full = false;
    bool item_cannot_be_used = false; // Includes other error messages, see ChatFilter.cpp.
    bool not_enough_energy = false; // Includes other error messages, see ChatFilter.cpp.
    bool item_already_identified = false;

    bool messagebycontent = false;
    // Which channels to filter.
    bool filter_channel_local = true;
    bool filter_channel_guild = false;
    bool filter_channel_team = false;
    bool filter_channel_trade = true;
    bool filter_channel_alliance = false;
    bool filter_channel_emotes = false;

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
    //      ParseBuffer(bycontent_buf, bycontent_regex);
    //  } else {
    //      ParseBuffer(bycontent_buf, bycontent_words);
    //  }
    //}

#ifdef EXTENDED_IGNORE_LIST
    void ByAuthor_ParseBuf() {
        ParseBuffer(byauthor_buf, byauthor_words);
    }
#endif
    GW::HookEntry BlockIfApplicable_Entry;
    GW::HookEntry ClearIfApplicable_Entry;
};
