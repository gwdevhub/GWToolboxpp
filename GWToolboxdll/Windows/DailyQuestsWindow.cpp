#include "stdafx.h"

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Constants/DailyQuests.h>
#include <Windows/DailyQuestsWindow.h>


namespace {
    bool subscribed_zaishen_bounties[ZAISHEN_BOUNTY_COUNT] = { false };
    bool subscribed_zaishen_combats[ZAISHEN_COMBAT_COUNT] = { false };
    bool subscribed_zaishen_missions[ZAISHEN_MISSION_COUNT] = { false };
    bool subscribed_zaishen_vanquishes[ZAISHEN_VANQUISH_COUNT] = { false };
    bool subscribed_wanted_quests[WANTED_COUNT] = { false };
    bool subscribed_weekly_bonus_pve[WEEKLY_BONUS_PVE_COUNT] = { false };
    bool subscribed_weekly_bonus_pvp[WEEKLY_BONUS_PVP_COUNT] = { false };

    bool show_zaishen_bounty_in_window = true;
    bool show_zaishen_combat_in_window = true;
    bool show_zaishen_missions_in_window = true;
    bool show_zaishen_vanquishes_in_window = true;
    bool show_wanted_quests_in_window = true;
    bool show_nicholas_in_window = true;
    bool show_weekly_bonus_pve_in_window = true;
    bool show_weekly_bonus_pvp_in_window = true;

    uint32_t subscriptions_lookahead_days = 7;

    float text_width = 200.0f;
    int daily_quest_window_count = 90;

    class ZaishenMission {
        ZaishenMission(GW::Constants::QuestID _quest_id, GW::Constants::MapID _map_id);
    };

    bool subscriptions_changed = false;
    bool checked_subscriptions = false;
    time_t start_time;

    std::vector<std::pair< const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;


    bool GetIsPreSearing()
    {
        const GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
        return i && i->region == GW::Region::Region_Presearing;
    }


    const wchar_t* DateString(const time_t* unix)
    {
        const std::tm* now = std::localtime(unix);
        static wchar_t buf[12];
        swprintf(buf, sizeof(buf), L"%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
        return buf;
    }

    uint32_t GetZaishenBounty(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1244736000) / 86400 % 66);
    }

    uint32_t GetWeeklyBonusPvE(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % 9);
    }

    uint32_t GetWeeklyBonusPvP(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % 6);
    }

    uint32_t GetZaishenCombat(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1256227200) / 86400 % 28);
    }

    uint32_t GetZaishenMission(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % 69);
    }

    uint32_t GetZaishenVanquish(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % 136);
    }

    // Find the "week start" for this timestamp.
    time_t GetWeeklyRotationTime(const time_t* unix)
    {
        return static_cast<time_t>(floor((*unix - 1368457200) / 604800) * 604800) + 1368457200;
    }

    time_t GetNextWeeklyRotationTime()
    {
        const time_t unix = time(nullptr);
        return GetWeeklyRotationTime(&unix) + 604800;
    }

    const char* GetNicholasSandfordLocation(const time_t* unix)
    {
        const auto cycle_index = static_cast<uint32_t>((*unix - 1239260400) / 86400 % 52);
        return NICHOLAS_PRE_CYCLES[cycle_index];
    }

    uint32_t GetNicholasItemQuantity(const time_t* unix)
    {
        const auto cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
        return nicholas_post_cycles[cycle_index].quantity;
    }

    const char* GetNicholasLocation(const time_t* unix)
    {
        const auto cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
        return nicholas_post_cycles[cycle_index].english_map_name;
    }

    const char* GetNicholasItemName(const time_t* unix)
    {
        const auto cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
        return nicholas_post_cycles[cycle_index].english_name;
    }

    uint32_t GetWantedByShiningBlade(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1276012800) / 86400 % 21);
    }

    const char* GetVanguardQuest(const time_t* unix)
    {
        const auto cycle_index = static_cast<uint32_t>((*unix - 1299168000) / 86400 % 9);
        return VANGUARD_CYCLES[cycle_index];
    }

    
    void PrintDaily(const wchar_t* label, const char* value, const time_t unix, const bool as_wiki_link = true)
    {
        const bool show_date = unix != time(nullptr);
        wchar_t buf[139];
        if (show_date) {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s, %s: <a=1>\x200B%S</a>" : L"%s, %s: <a=1>%S</a>", label, DateString(&unix), value);
        }
        else {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s: <a=1>\x200B%S</a>" : L"%s: %S", label, value);
        }
        WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf, nullptr, true);
    }

    void CHAT_CMD_FUNC(CmdWeeklyBonus)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Weekly Bonus PvE", pve_weekly_bonus_cycles[GetWeeklyBonusPvE(&now)], now);
        PrintDaily(L"Weekly Bonus PvP", pvp_weekly_bonus_cycles[GetWeeklyBonusPvP(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdZaishenBounty)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Zaishen Bounty", zaishen_bounty_cycles[GetZaishenBounty(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdZaishenMission)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Zaishen Mission", zaishen_mission_cycles[GetZaishenMission(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdZaishenVanquish)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Zaishen Vanquish", zaishen_vanquish_cycles[GetZaishenVanquish(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdZaishenCombat)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Zaishen Combat", zaishen_combat_cycles[GetZaishenCombat(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdWantedByShiningBlade)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Wanted", wanted_by_shining_blade_cycles[GetWantedByShiningBlade(&now)], now);
    }

    void CHAT_CMD_FUNC(CmdVanguard)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        PrintDaily(L"Vanguard Quest", GetVanguardQuest(&now), now);
    }

    void CHAT_CMD_FUNC(CmdNicholas)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        char buf[128];
        if (GetIsPreSearing()) {
            snprintf(buf, _countof(buf), "5 %s", GetNicholasSandfordLocation(&now));
            PrintDaily(L"Nicholas Sandford", buf, now, false);
        }
        else {
            snprintf(buf, _countof(buf), "%d %s in %s", GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
            PrintDaily(L"Nicholas the Traveler", buf, now, false);
        }
    }

}

void DailyQuests::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }
    float offset = 0.0f;
    const float short_text_width = 120.0f * ImGui::GetIO().FontGlobalScale;
    const float long_text_width = text_width * ImGui::GetIO().FontGlobalScale;
    const float zm_width = 170.0f * ImGui::GetIO().FontGlobalScale;
    const float zb_width = 185.0f * ImGui::GetIO().FontGlobalScale;
    const float zc_width = 135.0f * ImGui::GetIO().FontGlobalScale;
    const float zv_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    const float ws_width = 180.0f * ImGui::GetIO().FontGlobalScale;
    const float nicholas_width = 180.0f * ImGui::GetIO().FontGlobalScale;
    const float wbe_width = 145.0f * ImGui::GetIO().FontGlobalScale;

    ImGui::Text("Date");
    ImGui::SameLine(offset += short_text_width);
    if (show_zaishen_missions_in_window) {
        ImGui::Text("Zaishen Mission");
        ImGui::SameLine(offset += zm_width);
    }
    if (show_zaishen_bounty_in_window) {
        ImGui::Text("Zaishen Bounty");
        ImGui::SameLine(offset += zb_width);
    }
    if (show_zaishen_combat_in_window) {
        ImGui::Text("Zaishen Combat");
        ImGui::SameLine(offset += zc_width);
    }
    if (show_zaishen_vanquishes_in_window) {
        ImGui::Text("Zaishen Vanquish");
        ImGui::SameLine(offset += zv_width);
    }
    if (show_wanted_quests_in_window) {
        ImGui::Text("Wanted");
        ImGui::SameLine(offset += ws_width);
    }
    if (show_nicholas_in_window) {
        ImGui::Text("Nicholas the Traveler");
        ImGui::SameLine(offset += nicholas_width);
    }
    if (show_weekly_bonus_pve_in_window) {
        ImGui::Text("Weekly Bonus PvE");
        ImGui::SameLine(offset += wbe_width);
    }
    if (show_weekly_bonus_pvp_in_window) {
        ImGui::Text("Weekly Bonus PvP");
        ImGui::SameLine(offset += long_text_width);
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::BeginChild("dailies_scroll", ImVec2(0, -1 * (20.0f * ImGui::GetIO().FontGlobalScale) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);
    uint32_t idx = 0;
    const ImColor sCol(102, 187, 238, 255);
    const ImColor wCol(255, 255, 255, 255);
    for (size_t i = 0; i < static_cast<size_t>(daily_quest_window_count); i++) {
        offset = 0.0f;
        switch (i) {
            case 0:
                ImGui::Text("Today");
                break;
            case 1:
                ImGui::Text("Tomorrow");
                break;
            default:
                char mbstr[100];
                std::strftime(mbstr, sizeof(mbstr), "%a %d %b", std::localtime(&unix));
                ImGui::Text(mbstr);
                break;
        }

        ImGui::SameLine(offset += short_text_width);
        if (show_zaishen_missions_in_window) {
            idx = GetZaishenMission(&unix);
            ImGui::TextColored(subscribed_zaishen_missions[idx] ? sCol : wCol, zaishen_mission_cycles[idx]);
            if (ImGui::IsItemClicked()) {
                subscribed_zaishen_missions[idx] = !subscribed_zaishen_missions[idx];
            }
            ImGui::SameLine(offset += zm_width);
        }
        if (show_zaishen_bounty_in_window) {
            idx = GetZaishenBounty(&unix);
            ImGui::TextColored(subscribed_zaishen_bounties[idx] ? sCol : wCol, zaishen_bounty_cycles[idx]);
            if (ImGui::IsItemClicked()) {
                subscribed_zaishen_bounties[idx] = !subscribed_zaishen_bounties[idx];
            }
            ImGui::SameLine(offset += zb_width);
        }
        if (show_zaishen_combat_in_window) {
            idx = GetZaishenCombat(&unix);
            ImGui::TextColored(subscribed_zaishen_combats[idx] ? sCol : wCol, zaishen_combat_cycles[idx]);
            if (ImGui::IsItemClicked()) {
                subscribed_zaishen_combats[idx] = !subscribed_zaishen_combats[idx];
            }
            ImGui::SameLine(offset += zc_width);
        }
        if (show_zaishen_vanquishes_in_window) {
            idx = GetZaishenVanquish(&unix);
            ImGui::TextColored(subscribed_zaishen_vanquishes[idx] ? sCol : wCol, zaishen_vanquish_cycles[idx]);
            if (ImGui::IsItemClicked()) {
                subscribed_zaishen_vanquishes[idx] = !subscribed_zaishen_vanquishes[idx];
            }
            ImGui::SameLine(offset += zv_width);
        }
        if (show_wanted_quests_in_window) {
            idx = GetWantedByShiningBlade(&unix);
            ImGui::TextColored(subscribed_wanted_quests[idx] ? sCol : wCol, wanted_by_shining_blade_cycles[idx]);
            if (ImGui::IsItemClicked()) {
                subscribed_wanted_quests[idx] = !subscribed_wanted_quests[idx];
            }
            ImGui::SameLine(offset += ws_width);
        }
        if (show_nicholas_in_window) {
            ImGui::Text("%d %s", GetNicholasItemQuantity(&unix), GetNicholasItemName(&unix));
            ImGui::SameLine(offset += nicholas_width);
        }
        if (show_weekly_bonus_pve_in_window) {
            idx = GetWeeklyBonusPvE(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pve[idx] ? sCol : wCol, pve_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(pve_weekly_bonus_descriptions[idx]);
            }
            if (ImGui::IsItemClicked()) {
                subscribed_weekly_bonus_pve[idx] = !subscribed_weekly_bonus_pve[idx];
            }
            ImGui::SameLine(offset += wbe_width);
        }
        if (show_weekly_bonus_pvp_in_window) {
            idx = GetWeeklyBonusPvP(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pvp[idx] ? sCol : wCol, pvp_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(pvp_weekly_bonus_descriptions[idx]);
            }
            if (ImGui::IsItemClicked()) {
                subscribed_weekly_bonus_pvp[idx] = !subscribed_weekly_bonus_pvp[idx];
            }
            ImGui::SameLine(offset += long_text_width);
        }
        ImGui::NewLine();
        unix += 86400;
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up. Subscribed quests are highlighted in ");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(sCol, "blue");
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(".");
    return ImGui::End();
}

void DailyQuests::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Daily Quest Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::Bullet();
    ImGui::Text("'/zb' prints current zaishen bounty.");
    ImGui::Bullet();
    ImGui::Text("'/zm' prints current zaishen mission.");
    ImGui::Bullet();
    ImGui::Text("'/zv' prints current zaishen vanquish.");
    ImGui::Bullet();
    ImGui::Text("'/zc' prints current zaishen combat.");
    ImGui::Bullet();
    ImGui::Text("'/vanguard' prints current pre-searing vanguard quest.");
    ImGui::Bullet();
    ImGui::Text("'/wanted' prints current shining blade bounty.");
    ImGui::Bullet();
    ImGui::Text("'/nicholas' prints current nicholas location.");
    ImGui::Bullet();
    ImGui::Text("'/weekly' prints current weekly bonus.");
    ImGui::Bullet();
    ImGui::Text("'/today' prints current daily activities.");
    ImGui::Bullet();
    ImGui::Text("'/tomorrow' prints tomorrow's daily activities.");
    ImGui::TreePop();
}

void DailyQuests::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::PushItemWidth(200.f * ImGui::FontScale());
    ImGui::InputInt("Show daily quests for the next N days", &daily_quest_window_count);
    ImGui::PopItemWidth();
    ImGui::Text("Quests to show in Daily Quests window:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Bounty", &show_zaishen_bounty_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Combat", &show_zaishen_combat_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Mission", &show_zaishen_missions_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Vanquish", &show_zaishen_vanquishes_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Wanted by Shining Blade", &show_wanted_quests_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Nicholas The Traveler", &show_nicholas_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvE)", &show_weekly_bonus_pve_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvP)", &show_weekly_bonus_pvp_in_window);

    ImGui::Unindent();
}

void DailyQuests::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    LOAD_BOOL(show_zaishen_bounty_in_window);
    LOAD_BOOL(show_zaishen_combat_in_window);
    LOAD_BOOL(show_zaishen_missions_in_window);
    LOAD_BOOL(show_zaishen_vanquishes_in_window);
    LOAD_BOOL(show_wanted_quests_in_window);
    LOAD_BOOL(show_nicholas_in_window);
    LOAD_BOOL(show_weekly_bonus_pve_in_window);
    LOAD_BOOL(show_weekly_bonus_pvp_in_window);

    const char* zms = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_missions), "0");
    const std::bitset<ZAISHEN_MISSION_COUNT> zmb(zms);
    for (auto i = 0u; i < zmb.size(); i++) {
        subscribed_zaishen_missions[i] = zmb[i] == 1;
    }

    const char* zbs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), "0");
    const std::bitset<ZAISHEN_BOUNTY_COUNT> zbb(zbs);
    for (auto i = 0u; i < zbb.size(); i++) {
        subscribed_zaishen_bounties[i] = zbb[i] == 1;
    }

    const char* zcs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_combats), "0");
    const std::bitset<ZAISHEN_COMBAT_COUNT> zcb(zcs);
    for (auto i = 0u; i < zcb.size(); i++) {
        subscribed_zaishen_combats[i] = zcb[i] == 1;
    }

    const char* zvs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), "0");
    const std::bitset<ZAISHEN_VANQUISH_COUNT> zvb(zvs);
    for (auto i = 0u; i < zvb.size(); i++) {
        subscribed_zaishen_vanquishes[i] = zvb[i] == 1;
    }

    const char* wss = ini->GetValue(Name(), VAR_NAME(subscribed_wanted_quests), "0");
    const std::bitset<WANTED_COUNT> wsb(wss);
    for (auto i = 0u; i < wsb.size(); i++) {
        subscribed_wanted_quests[i] = wsb[i] == 1;
    }

    const char* wbes = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), "0");
    const std::bitset<WEEKLY_BONUS_PVE_COUNT> wbeb(wbes);
    for (auto i = 0u; i < wbeb.size(); i++) {
        subscribed_weekly_bonus_pve[i] = wbeb[i] == 1;
    }

    const char* wbps = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), "0");
    const std::bitset<WEEKLY_BONUS_PVP_COUNT> wbpb(wbps);
    for (auto i = 0u; i < wbpb.size(); i++) {
        subscribed_weekly_bonus_pvp[i] = wbpb[i] == 1;
    }
}

void DailyQuests::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_BOOL(show_zaishen_bounty_in_window);
    SAVE_BOOL(show_zaishen_combat_in_window);
    SAVE_BOOL(show_zaishen_missions_in_window);
    SAVE_BOOL(show_zaishen_vanquishes_in_window);
    SAVE_BOOL(show_wanted_quests_in_window);
    SAVE_BOOL(show_nicholas_in_window);
    SAVE_BOOL(show_weekly_bonus_pve_in_window);
    SAVE_BOOL(show_weekly_bonus_pvp_in_window);
    std::bitset<ZAISHEN_MISSION_COUNT> zmb;
    for (auto i = 0u; i < zmb.size(); i++) {
        zmb[i] = subscribed_zaishen_missions[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_missions), zmb.to_string().c_str());

    std::bitset<ZAISHEN_BOUNTY_COUNT> zbb;
    for (auto i = 0u; i < zbb.size(); i++) {
        zbb[i] = subscribed_zaishen_bounties[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), zbb.to_string().c_str());

    std::bitset<ZAISHEN_COMBAT_COUNT> zcb;
    for (auto i = 0u; i < zcb.size(); i++) {
        zcb[i] = subscribed_zaishen_combats[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_combats), zcb.to_string().c_str());

    std::bitset<ZAISHEN_VANQUISH_COUNT> zvb;
    for (auto i = 0u; i < zvb.size(); i++) {
        zvb[i] = subscribed_zaishen_vanquishes[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), zvb.to_string().c_str());

    std::bitset<WANTED_COUNT> wsb;
    for (auto i = 0u; i < wsb.size(); i++) {
        wsb[i] = subscribed_wanted_quests[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_wanted_quests), wsb.to_string().c_str());

    std::bitset<WEEKLY_BONUS_PVE_COUNT> wbeb;
    for (auto i = 0u; i < wbeb.size(); i++) {
        wbeb[i] = subscribed_weekly_bonus_pve[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), wbeb.to_string().c_str());

    std::bitset<WEEKLY_BONUS_PVP_COUNT> wbpb;
    for (auto i = 0u; i < wbpb.size(); i++) {
        wbpb[i] = subscribed_weekly_bonus_pvp[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), wbpb.to_string().c_str());
}

void DailyQuests::Initialize()
{
    ToolboxWindow::Initialize();

    chat_commands = {
        {L"zm", CmdZaishenMission},
        {L"zb", CmdZaishenBounty},
        {L"zc", CmdZaishenCombat},
        {L"zv", CmdZaishenVanquish},
        {L"vanguard", CmdVanguard},
        {L"wanted", CmdWantedByShiningBlade},
        {L"nicholas", CmdNicholas},
        {L"weekly", CmdWeeklyBonus},
        {L"today", [](const wchar_t*, const int, const LPWSTR*) -> void {
            if (GetIsPreSearing()) {
                GW::Chat::SendChat('/', "vanguard");
                GW::Chat::SendChat('/', "nicholas");
                return;
            }
            GW::Chat::SendChat('/', "zm");
            GW::Chat::SendChat('/', "zb");
            GW::Chat::SendChat('/', "zc");
            GW::Chat::SendChat('/', "zv");
            GW::Chat::SendChat('/', "wanted");
            GW::Chat::SendChat('/', "nicholas");
            GW::Chat::SendChat('/', "weekly");
        }},
        {L"daily", [](const wchar_t*, const int, const LPWSTR*) -> void {
            GW::Chat::SendChat('/', "today");
        }},
        {L"dailies", [](const wchar_t*, const int, const LPWSTR*) -> void {
            GW::Chat::SendChat('/', "today");
        }},
        {L"tomorrow", [](const wchar_t*, const int, const LPWSTR*) -> void {
            if (GetIsPreSearing()) {
                GW::Chat::SendChat('/', "vanguard tomorrow");
                GW::Chat::SendChat('/', "nicholas tomorrow");
                return;
            }
            GW::Chat::SendChat('/', "zm tomorrow");
            GW::Chat::SendChat('/', "zb tomorrow");
            GW::Chat::SendChat('/', "zc tomorrow");
            GW::Chat::SendChat('/', "zv tomorrow");
            GW::Chat::SendChat('/', "wanted tomorrow");
            GW::Chat::SendChat('/', "nicholas tomorrow");
        }}
    };
    for (auto& it : chat_commands) {
        GW::Chat::CreateCommand(it.first, it.second);
    }
}

void DailyQuests::Terminate() {
    ToolboxWindow::Terminate();
    for (auto& it : chat_commands) {
        GW::Chat::DeleteCommand(it.first);
    }
}

void DailyQuests::Update(const float)
{
    if (subscriptions_changed) {
        checked_subscriptions = false;
    }
    if (checked_subscriptions) {
        return;
    }
    subscriptions_changed = false;
    if (!start_time) {
        start_time = time(nullptr);
    }
    if (GW::Map::GetIsMapLoaded() && time(nullptr) - start_time > 1) {
        checked_subscriptions = true;
        // Check daily quests for the next 6 days, and send a message if found. Only runs once when TB is opened.
        const time_t now = time(nullptr);
        time_t unix = now + 0;
        uint32_t quest_idx;
        for (auto i = 0u; i < subscriptions_lookahead_days; i++) {
            char date_str[32];
            switch (i) {
                case 0:
                    sprintf(date_str, "today");
                    break;
                case 1:
                    sprintf(date_str, "tomorrow");
                    break;
                default:
                    std::strftime(date_str, 32, "on %A", std::localtime(&unix));
                    break;
            }
            if (subscribed_zaishen_missions[quest_idx = GetZaishenMission(&unix)]) {
                Log::Info("%s is the Zaishen Mission %s", zaishen_mission_cycles[quest_idx], date_str);
            }
            if (subscribed_zaishen_bounties[quest_idx = GetZaishenBounty(&unix)]) {
                Log::Info("%s is the Zaishen Bounty %s", zaishen_bounty_cycles[quest_idx], date_str);
            }
            if (subscribed_zaishen_combats[quest_idx = GetZaishenCombat(&unix)]) {
                Log::Info("%s is the Zaishen Combat %s", zaishen_combat_cycles[quest_idx], date_str);
            }
            if (subscribed_zaishen_vanquishes[quest_idx = GetZaishenVanquish(&unix)]) {
                Log::Info("%s is the Zaishen Vanquish %s", zaishen_vanquish_cycles[quest_idx], date_str);
            }
            if (subscribed_wanted_quests[quest_idx = GetWantedByShiningBlade(&unix)]) {
                Log::Info("%s is Wanted by the Shining Blade %s", wanted_by_shining_blade_cycles[quest_idx], date_str);
            }
            unix += 86400;
        }

        // Check weekly bonuses / special events
        unix = GetWeeklyRotationTime(&now);
        for (auto i = 0u; i < 2; i++) {
            char date_str[32];
            switch (i) {
                case 0:
                    std::strftime(date_str, 32, "until %R on %A", std::localtime(&unix));
                    break;
                default:
                    std::strftime(date_str, 32, "on %A at %R", std::localtime(&unix));
                    break;
            }
            if (subscribed_weekly_bonus_pve[quest_idx = GetWeeklyBonusPvE(&unix)]) {
                Log::Info("%s is the Weekly PvE Bonus %s", pve_weekly_bonus_cycles[quest_idx], date_str);
            }
            if (subscribed_weekly_bonus_pvp[quest_idx = GetWeeklyBonusPvP(&unix)]) {
                Log::Info("%s is the Weekly PvP Bonus %s", pvp_weekly_bonus_cycles[quest_idx], date_str);
            }
            unix += 604800;
        }
    }
}


