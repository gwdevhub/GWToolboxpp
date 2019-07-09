#include <ctype.h>
#include <ctime>
#include <stdint.h>

#include <algorithm>
#include <functional>

#include <TbWindows.h>
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
const char* GetZaishenBounty(time_t* unix) {
    uint32_t cycle_index = (*unix - 1244736000) / 86400 % 66;
    return DailyQuests::Instance().zaishen_bounty_cycles[cycle_index];
}
const char* GetZaishenCombat(time_t* unix) {
    uint32_t cycle_index = (*unix - 1256227200) / 86400 % 28;
    return DailyQuests::Instance().zaishen_combat_cycles[cycle_index];
}
const char* GetZaishenMission(time_t* unix) {
    uint32_t cycle_index = (*unix - 1299168000) / 86400 % 69;
    return DailyQuests::Instance().zaishen_mission_cycles[cycle_index];
}
const char* GetZaishenVanquish(time_t* unix) {
    uint32_t cycle_index = (*unix - 1299168000) / 86400 % 136;
    return DailyQuests::Instance().zaishen_vanquish_cycles[cycle_index];
}
const char* GetNicholasSandfordLocation(time_t* unix) {
    uint32_t cycle_index = (*unix - 1239260400) / 604800 % 52;
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
const char* GetWantedByShiningBlade(time_t* unix) {
    uint32_t cycle_index = (*unix - 1276012800) / 604800 % 21;
    return DailyQuests::Instance().wanted_by_shining_blade_cycles[cycle_index];
}
const char* GetVanguardQuest(time_t* unix) {
    uint32_t cycle_index = (*unix - 1299168000) / 604800 % 9;
    return DailyQuests::Instance().vanguard_cycles[cycle_index];
}
bool GetIsPreSearing() {
    GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
    return i && i->region == GW::Region::Region_Presearing;
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
}

void DailyQuests::SaveSettings(CSimpleIni* ini) {
}

void DailyQuests::Initialize() {
    ToolboxModule::Initialize();

    GW::Chat::CreateCommand(L"zb", DailyQuests::CmdZaishenBounty);
    GW::Chat::CreateCommand(L"zm", DailyQuests::CmdZaishenMission);
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
        GW::Chat::SendChat('/', "zc");
        GW::Chat::SendChat('/', "zb");
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
        GW::Chat::SendChat('/', "zc tomorrow");
        GW::Chat::SendChat('/', "zb tomorrow");
        GW::Chat::SendChat('/', "zv tomorrow");
        GW::Chat::SendChat('/', "wanted tomorrow");
        GW::Chat::SendChat('/', "nicholas tomorrow");
    });

    // Hook for turning player name links into hyperlinks.
    /*OnStartWhisper_Func = (OnStartWhisper_pt)GW::Scanner::Find("\x55\x8B\xEC\x51\x53\x56\x8B\xF1\x57\xBA\x05\x00\x00\x00", "xxxxxxxxxxxxxx", 0);
    printf("[SCAN] OnStartWhisper = %p\n", OnStartWhisper_Func);*/
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
void DailyQuests::Update(float delta) {

}

void DailyQuests::CmdZaishenBounty(const wchar_t *message, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    if (nowOriginal == now) // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Zaishen Bounty: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetZaishenBounty(&now));
    else
        snprintf(buf, buf_size, "Zaishen Bounty, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetZaishenBounty(&now));
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
        snprintf(buf, buf_size, "Zaishen Mission: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetZaishenMission(&now));
    else
        snprintf(buf, buf_size, "Zaishen Mission, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetZaishenMission(&now));
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
        snprintf(buf, buf_size, "Zaishen Vanquish: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetZaishenVanquish(&now));
    else
        snprintf(buf, buf_size, "Zaishen Vanquish, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetZaishenVanquish(&now));
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
        snprintf(buf, buf_size, "Zaishen Combat: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetZaishenCombat(&now));
    else
        snprintf(buf, buf_size, "Zaishen Combat, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetZaishenCombat(&now));
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
        snprintf(buf, buf_size, "Wanted: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", GetWantedByShiningBlade(&now));
    else
        snprintf(buf, buf_size, "Wanted, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", GetWantedByShiningBlade(&now));
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
            snprintf(buf, buf_size, "Nicholas the Traveller: %d %s in %s", GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
        else
            snprintf(buf, buf_size, "Nicholas the Traveller, %s: %d %s in %s", DateString(&now), GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
    }
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
}