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



//using namespace std;

#include <cstdio>
#include <cstdarg>

#if __cplusplus < 201703L
#include <memory>
#endif



bool subscriptions_changed = false;


const char* DateString(time_t* unix) {
    std::tm* now = std::localtime(unix);
    static char buf[12];
    snprintf(buf, sizeof(buf), "%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    return buf;
}
uint32_t GetZaishenBounty(time_t* unix) {
    return (*unix - 1244736000) / 86400 % 66;
}
uint32_t GetWeeklyBonusPvE(time_t* unix) {
    return (*unix - 1368457200) / 604800 % 9;
}
uint32_t GetWeeklyBonusPvP(time_t* unix) {
    return (*unix - 1368457200) / 604800 % 6;
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
// Find the "week start" for this timestamp.
time_t GetWeeklyRotationTime(time_t* unix) {
    return (time_t)(floor((*unix - 1368457200) / 604800) * 604800) + 1368457200;
}
time_t GetNextWeeklyRotationTime() {
    time_t unix = time(nullptr);
    return GetWeeklyRotationTime(&unix) + 604800;
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
    if(show_zaishen_bounty_in_window) {
        ImGui::Text("Zaishen Bounty");
        ImGui::SameLine(offset += zb_width);
    }
    if(show_zaishen_combat_in_window) {
        ImGui::Text("Zaishen Combat");
        ImGui::SameLine(offset += zc_width);
    }
    if(show_zaishen_vanquishes_in_window) {
        ImGui::Text("Zaishen Vanquish");
        ImGui::SameLine(offset += zv_width);
    }
    if(show_wanted_quests_in_window) {
        ImGui::Text("Wanted");
        ImGui::SameLine(offset += ws_width);
    }
    if(show_nicholas_in_window) {
        ImGui::Text("Nicholas the Traveler");
        ImGui::SameLine(offset += nicholas_width);
    }
    if(show_weekly_bonus_pve_in_window) {
        ImGui::Text("Weekly Bonus PvE");
        ImGui::SameLine(offset += wbe_width);
    }
    if(show_weekly_bonus_pvp_in_window) {
        ImGui::Text("Weekly Bonus PvP");
        ImGui::SameLine(offset += long_text_width);
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::BeginChild("dailies_scroll", ImVec2(0, (-1 * (20.0f * ImGui::GetIO().FontGlobalScale)) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);
    uint32_t idx = 0;
    ImColor sCol(102, 187, 238, 255);
    ImColor wCol(255, 255, 255, 255);
    for (size_t i = 0; i < (size_t)daily_quest_window_count; i++) {
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
            ImGui::TextColored(subscribed_zaishen_missions[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_mission_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_missions[idx] = !subscribed_zaishen_missions[idx];
            ImGui::SameLine(offset += zm_width);
        }
        if (show_zaishen_bounty_in_window) {
            idx = GetZaishenBounty(&unix);
            ImGui::TextColored(subscribed_zaishen_bounties[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_bounty_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_bounties[idx] = !subscribed_zaishen_bounties[idx];
            ImGui::SameLine(offset += zb_width);
        }
        if (show_zaishen_combat_in_window) {
            idx = GetZaishenCombat(&unix);
            ImGui::TextColored(subscribed_zaishen_combats[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_combat_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_combats[idx] = !subscribed_zaishen_combats[idx];
            ImGui::SameLine(offset += zc_width);
        }
        if (show_zaishen_vanquishes_in_window) {
            idx = GetZaishenVanquish(&unix);
            ImGui::TextColored(subscribed_zaishen_vanquishes[idx] ? sCol : wCol, DailyQuests::Instance().zaishen_vanquish_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_vanquishes[idx] = !subscribed_zaishen_vanquishes[idx];
            ImGui::SameLine(offset += zv_width);
        }
        if (show_wanted_quests_in_window) {
            idx = GetWantedByShiningBlade(&unix);
            ImGui::TextColored(subscribed_wanted_quests[idx] ? sCol : wCol, DailyQuests::Instance().wanted_by_shining_blade_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_wanted_quests[idx] = !subscribed_wanted_quests[idx];
            ImGui::SameLine(offset += ws_width);
        }
        if (show_nicholas_in_window) {
            ImGui::Text("%d %s", GetNicholasItemQuantity(&unix), GetNicholasItemName(&unix));
            ImGui::SameLine(offset += nicholas_width);
        }
        if (show_weekly_bonus_pve_in_window) {
            idx = GetWeeklyBonusPvE(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pve[idx] ? sCol : wCol, DailyQuests::Instance().pve_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(DailyQuests::Instance().pve_weekly_bonus_descriptions[idx]);
            if(ImGui::IsItemClicked())
                subscribed_weekly_bonus_pve[idx] = !subscribed_weekly_bonus_pve[idx];
            ImGui::SameLine(offset += wbe_width);
        }
        if (show_weekly_bonus_pvp_in_window) {
            idx = GetWeeklyBonusPvP(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pvp[idx] ? sCol : wCol, DailyQuests::Instance().pvp_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(DailyQuests::Instance().pvp_weekly_bonus_descriptions[idx]);
            if (ImGui::IsItemClicked())
                subscribed_weekly_bonus_pvp[idx] = !subscribed_weekly_bonus_pvp[idx];
            ImGui::SameLine(offset += long_text_width);
        }
        ImGui::NewLine();
        unix += 86400;
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up. Subscribed quests are highlighted in ");
    ImGui::SameLine(0,0);
    ImGui::TextColored(sCol, "blue");
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(".");
    return ImGui::End();
}
void DailyQuests::DrawHelp() {
	if (!ImGui::TreeNode("Daily Quest Chat Commands"))
		return;
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::Bullet(); ImGui::Text("'/zb' prints current zaishen bounty.");
    ImGui::Bullet(); ImGui::Text("'/zm' prints current zaishen mission.");
    ImGui::Bullet(); ImGui::Text("'/zv' prints current zaishen vanquish.");
	ImGui::Bullet(); ImGui::Text("'/zc' prints current zaishen combat.");
	ImGui::Bullet(); ImGui::Text("'/vanguard' prints current pre-searing vanguard quest.");
	ImGui::Bullet(); ImGui::Text("'/wanted' prints current shining blade bounty.");
    ImGui::Bullet(); ImGui::Text("'/nicholas' prints current nicholas location.");
	ImGui::Bullet(); ImGui::Text("'/weekly' prints current weekly bonus.");
    ImGui::Bullet(); ImGui::Text("'/today' prints current daily activities.");
    ImGui::Bullet(); ImGui::Text("'/tomorrow' prints tomorrow's daily activities.");
	ImGui::TreePop();
}
void DailyQuests::DrawSettingInternal() {
    ToolboxWindow::DrawSettingInternal();
	float width = ImGui::GetContentRegionAvailWidth() / 2;
	ImGui::PushItemWidth(width);
    ImGui::InputInt("Show daily quests for the next ", &daily_quest_window_count);
	ImGui::PopItemWidth();
    ImGui::SameLine(0, 0);
    ImGui::Text("days");
    ImGui::Text("Quests to show in Daily Quests window:");
    ImGui::Indent();
	ImGui::PushItemWidth(width);
    ImGui::Checkbox("Zaishen Bounty", &show_zaishen_bounty_in_window);
    ImGui::SameLine();
    ImGui::Checkbox("Zaishen Combat", &show_zaishen_combat_in_window);
    ImGui::Checkbox("Zaishen Mission", &show_zaishen_missions_in_window);
    ImGui::SameLine();
    ImGui::Checkbox("Zaishen Vanquish", &show_zaishen_vanquishes_in_window);
    ImGui::Checkbox("Wanted by Shining Blade", &show_wanted_quests_in_window);
    ImGui::SameLine();
    ImGui::Checkbox("Nicholas The Traveler", &show_nicholas_in_window);
    ImGui::Checkbox("Weekly Bonus (PvE)", &show_weekly_bonus_pve_in_window);
    ImGui::SameLine();
    ImGui::Checkbox("Weekly Bonus (PvP)", &show_weekly_bonus_pvp_in_window);
    ImGui::InputFloat("Text spacing", &text_width);
	ImGui::PopItemWidth();
	ImGui::Unindent();
}
void DailyQuests::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);

    show_zaishen_bounty_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_bounty_in_window), show_zaishen_bounty_in_window);
    show_zaishen_combat_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_combat_in_window), show_zaishen_combat_in_window);
    show_zaishen_missions_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_missions_in_window), show_zaishen_missions_in_window);
    show_zaishen_vanquishes_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_vanquishes_in_window), show_zaishen_vanquishes_in_window);
    show_wanted_quests_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_wanted_quests_in_window), show_wanted_quests_in_window);
    show_nicholas_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_nicholas_in_window), show_nicholas_in_window);
    show_weekly_bonus_pve_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pve_in_window), show_weekly_bonus_pve_in_window);
    show_weekly_bonus_pvp_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pvp_in_window), show_weekly_bonus_pvp_in_window);

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

    const char* wbes = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), "0");
    std::bitset<wbe_cnt> wbeb(wbes);
    for (unsigned int i = 0; i < wbeb.size(); ++i) {
        subscribed_weekly_bonus_pve[i] = wbeb[i] == 1;
    }

    const char* wbps = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), "0");
    std::bitset<wbp_cnt> wbpb(wbps);
    for (unsigned int i = 0; i < wbpb.size(); ++i) {
        subscribed_weekly_bonus_pvp[i] = wbpb[i] == 1;
    }
}
void DailyQuests::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_bounty_in_window), show_zaishen_bounty_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_combat_in_window), show_zaishen_combat_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_missions_in_window), show_zaishen_missions_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_vanquishes_in_window), show_zaishen_vanquishes_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_wanted_quests_in_window), show_wanted_quests_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_nicholas_in_window), show_nicholas_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pve_in_window), show_weekly_bonus_pve_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pvp_in_window), show_weekly_bonus_pvp_in_window);
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

    std::bitset<wbe_cnt> wbeb;
    for (unsigned int i = 0; i < wbeb.size(); ++i) {
        wbeb[i] = subscribed_weekly_bonus_pve[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), wbeb.to_string().c_str());

    std::bitset<wbp_cnt> wbpb;
    for (unsigned int i = 0; i < wbpb.size(); ++i) {
        wbpb[i] = subscribed_weekly_bonus_pvp[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), wbpb.to_string().c_str());
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
    GW::Chat::CreateCommand(L"weekly", DailyQuests::CmdWeeklyBonus);
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
        GW::Chat::SendChat('/', "weekly");
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

    
}
void DailyQuests::Terminate() {
    ToolboxModule::Terminate();
    
}
bool checked_subscriptions = false;
time_t start_time;
void DailyQuests::Update(float delta) {
    if (subscriptions_changed)
        checked_subscriptions = false;
    if (checked_subscriptions)
        return;
    subscriptions_changed = false;
    if (!start_time)
        start_time = time(nullptr);
    if (GW::Map::GetIsMapLoaded() && (time(nullptr) - start_time) > 1) {
        checked_subscriptions = true;
        // Check daily quests for the next 6 days, and send a message if found. Only runs once when TB is opened.
        time_t now = time(nullptr);
        time_t unix = now + 0;
        uint32_t quest_idx;
        for (unsigned int i = 0; i < subscriptions_lookahead_days; i++) {
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

        // Check weekly bonuses / special events
        unix = GetWeeklyRotationTime(&now);
        for (unsigned int i = 0; i < 2; i++) {
            char date_str[32];
            switch (i) {
                case 0: std::strftime(date_str, 32, "until %R on %A", std::localtime(&unix)); break;
                default: std::strftime(date_str, 32, "on %A at %R", std::localtime(&unix)); break;
            }
            if (subscribed_weekly_bonus_pve[quest_idx = GetWeeklyBonusPvE(&unix)])
                Log::Info("%s is the Weekly PvE Bonus %s", Instance().pve_weekly_bonus_cycles[quest_idx], date_str);
            if (subscribed_weekly_bonus_pvp[quest_idx = GetWeeklyBonusPvP(&unix)])
                Log::Info("%s is the Weekly PvP Bonus %s", Instance().pvp_weekly_bonus_cycles[quest_idx], date_str);
            unix += 604800;
        }
    }
}

void DailyQuests::CmdWeeklyBonus(const wchar_t* message, int argc, LPWSTR* argv) {
    time_t now = time(nullptr);
    time_t nowOriginal = time(&now);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    const int buf_size = 139;
    char buf[buf_size];
    char buf2[buf_size];
    if (nowOriginal == now) { // Today - dont put timestamp in.
        snprintf(buf, buf_size, "Weekly Bonus PvE: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().pve_weekly_bonus_cycles[GetWeeklyBonusPvE(&now)]);
        snprintf(buf2, buf_size, "Weekly Bonus PvP: <a=1>%s%s</a>", "https://wiki.guildwars.com/wiki/", Instance().pvp_weekly_bonus_cycles[GetWeeklyBonusPvP(&now)]);
    }
    else {
        snprintf(buf, buf_size, "Weekly Bonus PvE, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().pve_weekly_bonus_cycles[GetWeeklyBonusPvE(&now)]);
        snprintf(buf2, buf_size, "Weekly Bonus PvP, %s: <a=1>%s%s</a>", DateString(&now), "https://wiki.guildwars.com/wiki/", Instance().pvp_weekly_bonus_cycles[GetWeeklyBonusPvP(&now)]);
    }
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf2);
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
