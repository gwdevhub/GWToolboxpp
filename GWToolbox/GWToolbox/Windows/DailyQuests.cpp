#include "stdafx.h"
#include <ctype.h>
#include <ctime>
#include <stdint.h>
#include <bitset>

#include <algorithm>
#include <functional>

#include <ShellApi.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Map.h>

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include <GuiUtils.h>
#include "GWToolbox.h"
#include <logger.h>
#include "DailyQuests.h"

// Implementation
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

using namespace std;

#include <cstdio>
#include <cstdarg>

#if __cplusplus < 201703L
#include <memory>
#endif

typedef void(__fastcall *OnStartWhisper_pt)(uint32_t unk1, wchar_t* name, wchar_t* name2);
OnStartWhisper_pt OnStartWhisper_Func;
OnStartWhisper_pt OnStartWhisperRet;
void __fastcall OnStartWhisper(uint32_t unk1, wchar_t* name, wchar_t* name2) {
    GW::HookBase::EnterHook();
    //Log::LogW(L"[OnStartWhisper] %08X %ls", unk1,name);
    if (name && (!wcsncmp(name, L"http://", 7) || !wcsncmp(name, L"https://", 8))) {
        ShellExecuteW(NULL, L"open", name, NULL, NULL, SW_SHOWNORMAL);
    } else {
        OnStartWhisperRet(unk1, name, name2);
    }
    return GW::HookBase::LeaveHook();
}
const char* DateString(time_t* unix) {
    std::tm* now = std::localtime(unix);
        
    char* dow = "Sun";
    char* suf = "th";
    char* mon = "Jan";

    switch (now->tm_wday) {
        case 1:dow="Mon"; break;
        case 2:dow="Tue"; break;
        case 3:dow="Wed"; break;
        case 4:dow="Thur"; break;
        case 5:dow="Fri"; break;
        case 6:dow="Sat"; break;
    }
    switch (now->tm_mday) {
        case 1:
        case 21:
        case 31:suf = "st";break;
        case 2:
        case 22:suf= "nd";break;
        case 3:
        case 23: suf= "rd";break;
    }
    switch (now->tm_mon) {
        case 1:mon= "Feb"; break;
        case 2:mon = "Mar"; break;
        case 3:mon = "Apr"; break;
        case 4:mon = "May"; break;
        case 5:mon = "Jun"; break;
        case 6:mon = "Jul"; break;
        case 7:mon = "Aug"; break;
        case 8:mon = "Sep"; break;
        case 9:mon = "Oct"; break;
        case 10:mon = "Nov"; break;
        case 11:mon = "Dec"; break;
    }
    const int buf_size = 12;
    static char buf[buf_size];
    //snprintf(buf, buf_size, "%s %d%s %s", dow, now->tm_mday, suf, mon);
    snprintf(buf, buf_size, "%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    return buf;
}
uint32_t GetZaishenBounty(time_t* unix) {
    return (*unix - 1244736000) / 86400 % 66;
}
uint32_t GetZaishenCombat(time_t* unix) {
    return (*unix - 1256227200) / 86400 % 28;
}
uint32_t GetZaishenMission(time_t* unix) {
    return (*unix - 1299168000) / 86400 % 69;
}
uint32_t GetZaishenVanquish(time_t* unix) {
    return (*unix - 1299168000) / 86400 % 136;
}
const char* GetNicholasSandfordLocation(time_t* unix) {
    uint32_t cycle_index = (*unix - 1239260400) / 86400 % 52;
    return DailyQuests::Instance().nicholas_sandford_cycles[cycle_index];
}
uint32_t GetNicholasItemQuantity(time_t* unix) {
    uint32_t cycle_index = (*unix - 1323097200) / 604800 % 137;
    return DailyQuests::Instance().nicholas_quantity_cycles[cycle_index];
}
const char* GetNicholasLocation(time_t* unix) {
    uint32_t cycle_index = (*unix - 1323097200) / 604800 % 137;
    return DailyQuests::Instance().nicholas_location_cycles[cycle_index];
}
const char* GetNicholasItemName(time_t* unix) {
    uint32_t cycle_index = (*unix - 1323097200) / 604800 % 137;
    return DailyQuests::Instance().nicholas_item_cycles[cycle_index];
}
uint32_t GetWantedByShiningBlade(time_t* unix) {
    return (*unix - 1276012800) / 86400 % 21;
}
const char* GetVanguardQuest(time_t* unix) {
    uint32_t cycle_index = (*unix - 1299168000) / 86400 % 9;
    return DailyQuests::Instance().vanguard_cycles[cycle_index];
}
bool GetIsPreSearing() {
    GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
    return i && i->region == GW::Region::Region_Presearing;
}
void DailyQuests::Draw(IDirect3DDevice9* pDevice) {
    if (!visible)
        return;
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    float offset = 0.0f;
    const float short_text_width = 120.0f * ImGui::GetIO().FontGlobalScale;
    const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Date");
    ImGui::SameLine(offset += short_text_width);
    ImGui::Text("Zaishen Mission");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Zaishen Bounty");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Zaishen Combat");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Zaishen Vanquish");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Wanted");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Nicholas the Traveler");
    ImGui::Separator();
    ImGui::BeginChild("dailies_scroll", ImVec2(0, (-1 * (20.0f * ImGui::GetIO().FontGlobalScale)) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);
    uint32_t idx = 0;
    ImColor sCol(102, 187, 238, 255);
    ImColor wCol(255, 255, 255, 255);
    char btn_id_buff[7];
    for (size_t i = 0; i < 136; i++) {
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
        idx = GetZaishenMission(&unix);
        sprintf(btn_id_buff, "zm_%03d", i);
        if (ImGui::InvisibleButton(btn_id_buff, ImVec2(long_text_width, ImGui::GetFontSize()))) {
            subscribed_zaishen_missions[idx] = !subscribed_zaishen_missions[idx];
        }
        ImGui::SameLine(offset);
        ImGui::TextColored(subscribed_zaishen_missions[idx] ? sCol: wCol, DailyQuests::Instance().zaishen_mission_cycles[idx]);
        ImGui::SameLine(offset += long_text_width);

        idx = GetZaishenBounty(&unix);
        sprintf(btn_id_buff, "zb_%03d", i);
        if (ImGui::InvisibleButton(btn_id_buff, ImVec2(long_text_width,ImGui::GetFontSize()))) {
            subscribed_zaishen_bounties[idx] = !subscribed_zaishen_bounties[idx];
        }
        ImGui::SameLine(offset);
        ImGui::TextColored(subscribed_zaishen_bounties[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_bounty_cycles[idx]);
        ImGui::SameLine(offset += long_text_width);

        idx = GetZaishenCombat(&unix);
        sprintf(btn_id_buff, "zc_%03d", i);
        if (ImGui::InvisibleButton(btn_id_buff, ImVec2(long_text_width, ImGui::GetFontSize()))) {
            subscribed_zaishen_combats[idx] = !subscribed_zaishen_combats[idx];
        }
        ImGui::SameLine(offset);
        ImGui::TextColored(subscribed_zaishen_combats[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_combat_cycles[idx]);
        ImGui::SameLine(offset += long_text_width);

        idx = GetZaishenVanquish(&unix);
        sprintf(btn_id_buff, "zv_%03d", i);
        if (ImGui::InvisibleButton(btn_id_buff, ImVec2(long_text_width, ImGui::GetFontSize()))) {
            subscribed_zaishen_vanquishes[idx] = !subscribed_zaishen_vanquishes[idx];
        }
        ImGui::SameLine(offset);
        ImGui::TextColored(subscribed_zaishen_vanquishes[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_vanquish_cycles[idx]);
        ImGui::SameLine(offset += long_text_width);

        idx = GetWantedByShiningBlade(&unix);
        sprintf(btn_id_buff, "sb_%03d", i);
        if (ImGui::InvisibleButton(btn_id_buff, ImVec2(long_text_width, ImGui::GetFontSize()))) {
            subscribed_wanted_quests[idx] = !subscribed_wanted_quests[idx];
        }
        ImGui::SameLine(offset);
        ImGui::TextColored(subscribed_wanted_quests[idx] ? sCol : wCol, DailyQuests::Instance().wanted_by_shining_blade_cycles[idx]);
        ImGui::SameLine(offset += long_text_width);

        ImGui::Text("%d %s, %s",GetNicholasItemQuantity(&unix), GetNicholasItemName(&unix), GetNicholasLocation(&unix));
        unix += 86400;
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up.Subscribed quests are highlighted in ");
    ImGui::SameLine(0,0);
    ImGui::TextColored(sCol, "blue");
    return ImGui::End();
}
void DailyQuests::DrawHelp() {
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::Bullet(); ImGui::Text("'/zb' prints current zaishen bounty.");
    ImGui::Bullet(); ImGui::Text("'/zm' prints current zaishen mission.");
    ImGui::Bullet(); ImGui::Text("'/zc' prints current zaishen mission.");
    ImGui::Bullet(); ImGui::Text("'/nicholas' prints current nicholas location.");
    ImGui::Bullet(); ImGui::Text("'/today' prints current daily activities.");
    ImGui::Bullet(); ImGui::Text("'/tomorrow' prints tomorrow's daily activities.");
}
void DailyQuests::DrawSettingInternal() {

}

void DailyQuests::LoadSettings(CSimpleIni* ini) {

    const char* zms = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_missions), "0");
    std::bitset<zm_cnt> zmb(zms);
    for (unsigned int i = 0; i < zmb.size(); ++i) {
        subscribed_zaishen_missions[i] = zmb[i] == 1;
    }

    const char* zbs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), "0");
    std::bitset<zb_cnt> zbb(zbs);
    for (unsigned int i = 0; i < zbb.size(); ++i) {
        subscribed_zaishen_bounties[i] = zbb[i] == 1;
    }

    const char* zcs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_combats), "0");
    std::bitset<zc_cnt> zcb(zcs);
    for (unsigned int i = 0; i < zcb.size(); ++i) {
        subscribed_zaishen_combats[i] = zcb[i] == 1;
    }

    const char* zvs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), "0");
    std::bitset<zv_cnt> zvb(zvs);
    for (unsigned int i = 0; i < zvb.size(); ++i) {
        subscribed_zaishen_vanquishes[i] = zvb[i] == 1;
    }

    const char* wss = ini->GetValue(Name(), VAR_NAME(subscribed_wanted_quests), "0");
    std::bitset<ws_cnt> wsb(wss);
    for (unsigned int i = 0; i < wsb.size(); ++i) {
        subscribed_wanted_quests[i] = wsb[i] == 1;
    }
}
void DailyQuests::SaveSettings(CSimpleIni* ini) {
    std::bitset<zm_cnt> zmb;
    for (unsigned int i = 0; i < zmb.size(); ++i) {
        zmb[i] = subscribed_zaishen_missions[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_missions), zmb.to_string().c_str());

    std::bitset<zb_cnt> zbb;
    for (unsigned int i = 0; i < zbb.size(); ++i) {
        zbb[i] = subscribed_zaishen_bounties[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), zbb.to_string().c_str());

    std::bitset<zc_cnt> zcb;
    for (unsigned int i = 0; i < zcb.size(); ++i) {
        zcb[i] = subscribed_zaishen_combats[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_combats), zcb.to_string().c_str());

    std::bitset<zv_cnt> zvb;
    for (unsigned int i = 0; i < zvb.size(); ++i) {
        zvb[i] = subscribed_zaishen_vanquishes[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), zvb.to_string().c_str());

    std::bitset<ws_cnt> wsb;
    for (unsigned int i = 0; i < wsb.size(); ++i) {
        wsb[i] = subscribed_wanted_quests[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_wanted_quests), wsb.to_string().c_str());
}

void DailyQuests::Initialize() {
    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(L"zm", DailyQuests::CmdZaishenMission);
    GW::Chat::CreateCommand(L"zb", DailyQuests::CmdZaishenBounty);
    GW::Chat::CreateCommand(L"zc", DailyQuests::CmdZaishenCombat);
    GW::Chat::CreateCommand(L"zv", DailyQuests::CmdZaishenVanquish);
    GW::Chat::CreateCommand(L"vanguard", DailyQuests::CmdVanguard);
    GW::Chat::CreateCommand(L"wanted", DailyQuests::CmdWantedByShiningBlade);
    GW::Chat::CreateCommand(L"nicholas", DailyQuests::CmdNicholas);
    GW::Chat::CreateCommand(L"today", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
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
    });
    GW::Chat::CreateCommand(L"daily", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        GW::Chat::SendChat('/', "today");
    });
    GW::Chat::CreateCommand(L"dailies", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        GW::Chat::SendChat('/', "today");
    });
    GW::Chat::CreateCommand(L"tomorrow", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
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
    });

    // Hook for turning player name links into hyperlinks.
    OnStartWhisper_Func = (OnStartWhisper_pt)GW::Scanner::Find("\x55\x8B\xEC\x51\x53\x56\x8B\xF1\x57\xBA\x05\x00\x00\x00", "xxxxxxxxxxxxxx", 0);
    printf("[SCAN] OnStartWhisper = %p\n", OnStartWhisper_Func);
    if (OnStartWhisper_Func) {
        GW::HookBase::CreateHook(OnStartWhisper_Func, OnStartWhisper, (void **)&OnStartWhisperRet);
        GW::HookBase::EnableHooks(OnStartWhisper_Func);
    }
}
void DailyQuests::Terminate() {
    ToolboxModule::Terminate();
    if (OnStartWhisper_Func) {
        GW::HookBase::DisableHooks(OnStartWhisper_Func);
        GW::HookBase::RemoveHook(OnStartWhisper_Func);
    }
}
bool checked_subscriptions = false;
void DailyQuests::Update(float delta) {
    if (!checked_subscriptions) {
        checked_subscriptions = true;
        // Check daily quests for the next 6 days, and send a message if found. Only runs once when TB is opened.
        time_t unix = time(nullptr);
        uint32_t quest_idx;
        for (unsigned int i = 0; i < 6; i++) {
            char date_str[32];
            switch (i) {
            case 0:
                sprintf(date_str, "Today");
                break;
            case 1:
                sprintf(date_str, "Tomorrow");
                break;
            default:
                std::strftime(date_str, 32, "on %A", std::localtime(&unix));
                break;
            }
            if (subscribed_zaishen_missions[quest_idx = GetZaishenMission(&unix)])
                Log::Info("%s is the Zaishen Mission %s", Instance().zaishen_mission_cycles[quest_idx], date_str);
            if (subscribed_zaishen_bounties[quest_idx = GetZaishenBounty(&unix)])
                Log::Info("%s is the Zaishen Bounty %s", Instance().zaishen_bounty_cycles[quest_idx], date_str);
            if (subscribed_zaishen_combats[quest_idx = GetZaishenCombat(&unix)])
                Log::Info("%s is the Zaishen Combat %s", Instance().zaishen_combat_cycles[quest_idx], date_str);
            if (subscribed_zaishen_vanquishes[quest_idx = GetZaishenVanquish(&unix)])
                Log::Info("%s is the Zaishen Vanquish %s", Instance().zaishen_vanquish_cycles[quest_idx], date_str);
            if (subscribed_wanted_quests[quest_idx = GetWantedByShiningBlade(&unix)])
                Log::Info("%s is Wanted by the Shining Blade %s", Instance().wanted_by_shining_blade_cycles[quest_idx], date_str);
            unix += 86400;
        }
    }
}

void DailyQuests::CmdZaishenBounty(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Zaishen Bounty: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().zaishen_bounty_cycles[GetZaishenBounty(&now)]);
    else
        snprintf(buf, buf_size, "Zaishen Bounty, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().zaishen_bounty_cycles[GetZaishenBounty(&now)]);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL,buf);
}
void DailyQuests::CmdZaishenMission(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Zaishen Mission: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().zaishen_mission_cycles[GetZaishenMission(&now)]);
    else
        snprintf(buf, buf_size, "Zaishen Mission, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().zaishen_mission_cycles[GetZaishenMission(&now)]);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}
void DailyQuests::CmdZaishenVanquish(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Zaishen Vanquish: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().zaishen_vanquish_cycles[GetZaishenVanquish(&now)]);
    else
        snprintf(buf, buf_size, "Zaishen Vanquish, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().zaishen_vanquish_cycles[GetZaishenVanquish(&now)]);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}
void DailyQuests::CmdZaishenCombat(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Zaishen Combat: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().zaishen_combat_cycles[GetZaishenCombat(&now)]);
    else
        snprintf(buf, buf_size, "Zaishen Combat, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().zaishen_combat_cycles[GetZaishenCombat(&now)]);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}
void DailyQuests::CmdWantedByShiningBlade(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Wanted: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().wanted_by_shining_blade_cycles[GetWantedByShiningBlade(&now)]);
    else
        snprintf(buf, buf_size, "Wanted, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().wanted_by_shining_blade_cycles[GetWantedByShiningBlade(&now)]);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}
void DailyQuests::CmdVanguard(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Vanguard Quest: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetVanguardQuest(&now));
    else
        snprintf(buf, buf_size, "Vanguard Quest, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetVanguardQuest(&now));
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}
void DailyQuests::CmdNicholas(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (GetIsPreSearing()) {
        if (nowOriginal == now) // Today - dont put timestamp in.
            snprintf(buf, buf_size, "Nicholas Sandford: 5 %s", GetNicholasSandfordLocation(&now));
        else
            snprintf(buf, buf_size, "Nicholas Sandford, %s: 5 %s", DateString(&now), GetNicholasSandfordLocation(&now));
    } else {
        if (nowOriginal == now) // Today - dont put timestamp in.
            snprintf(buf, buf_size, "Nicholas the Traveler: %d %s in %s", GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
        else
            snprintf(buf, buf_size, "Nicholas the Traveler, %s: %d %s in %s", DateString(&now), GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
    }
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}