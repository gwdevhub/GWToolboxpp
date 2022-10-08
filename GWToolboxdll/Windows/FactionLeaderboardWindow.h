#pragma once

#include <GWCA/Utilities/Hook.h>

#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>

class FactionLeaderboardWindow : public ToolboxWindow {
    FactionLeaderboardWindow() = default;
    ~FactionLeaderboardWindow() = default;

private:
    struct LeaderboardEntry {
        LeaderboardEntry() = default;
        LeaderboardEntry(uint32_t m, uint32_t r, uint32_t a, uint32_t f, const wchar_t* n, const wchar_t* t) : map_id(m), rank(r), allegiance(a), faction(f) {
            wcscpy(guild_wstr, n); // Copy the string to avoid read errors later.
            wcscpy(tag_wstr, t); // Copy the string to avoid read errors later.
            map_name[0] = 0;
            strcpy(guild_str,GuiUtils::WStringToString(guild_wstr).c_str());
            strcpy(tag_str, GuiUtils::WStringToString(tag_wstr).c_str());
            guild_wiki_url = guild_wstr;
            std::ranges::transform(guild_wiki_url, guild_wiki_url.begin(),
                                   [](wchar_t ch) -> wchar_t {
                                       return ch == ' ' ? L'_' : ch;
                                   });
            guild_wiki_url = L"https://wiki.guildwars.com/wiki/Guild:" + guild_wiki_url;
            initialised = true;
        }
        uint32_t map_id=0;
        uint32_t rank=0;
        uint32_t allegiance=0;
        uint32_t faction=0;
        wchar_t guild_wstr[32];
        wchar_t tag_wstr[5];
        char guild_str[128]{}; // unicode char can be up to 4 bytes
        char tag_str[20]{}; // unicode char can be up to 4 bytes
        wchar_t map_name_enc[16]{};
        char map_name[256]{};
        std::wstring guild_wiki_url;
        bool initialised = false;
    };

public:
    static FactionLeaderboardWindow& Instance() {
        static FactionLeaderboardWindow instance;
        return instance;
    }

    const char* Name() const override { return "Faction Leaderboard"; }
    const char* Icon() const override { return ICON_FA_GLOBE; }

    void Initialize() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
private:
    std::vector<LeaderboardEntry> leaderboard;
    std::vector<LeaderboardEntry>::iterator lit;

    GW::HookEntry TownAlliance_Entry;
};
