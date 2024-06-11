#include "Color.h"
#include "stdafx.h"
#include "Timer.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Widgets/PartyDamage.h>

constexpr const wchar_t* INI_FILENAME = L"healthlog.ini";
constexpr const char* IniSection = "health";

namespace {

    // damage values
    struct PlayerDamage {
        uint32_t damage = 0;
        uint32_t recent_damage = 0;
        clock_t last_damage = 0;
        uint32_t agent_id = 0;
        GW::Constants::Profession primary = GW::Constants::Profession::None;
        GW::Constants::Profession secondary = GW::Constants::Profession::None;

        void Reset()
        {
            damage = 0;
            recent_damage = 0;
            agent_id = 0;
            primary = GW::Constants::Profession::None;
            secondary = GW::Constants::Profession::None;
        }
    };

    uint32_t total = 0;
    std::vector<PlayerDamage> damage;
    std::vector<GuiUtils::EncString*> party_names_by_index;
    std::map<DWORD, uint32_t> hp_map{};
    std::map<DWORD, size_t> party_index{}; // Mapping of agent_id to party index
    

    // main routine variables
    bool in_explorable = false;
    clock_t send_timer = 0;
    std::queue<std::wstring> send_queue{};

    // ini
    ToolboxIni* inifile = nullptr;
    Color color_background = 0;
    Color color_damage = 0;
    Color color_recent = 0;
    float width = 0.f;
    bool bars_left = true;
    int recent_max_time = 0;
    int row_height = 0;
    bool hide_in_outpost = false;
    bool print_by_click = false;

    bool snap_to_party_window = true;
    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 0;

    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry MapLoaded_Entry;

    std::pair<ImVec2, ImVec2> party_frame_position;
    std::pair<ImVec2, ImVec2> party_health_bars_position;
    std::unordered_map<uint32_t, std::pair<ImVec2, ImVec2>> agent_health_bar_positions;

    PlayerDamage* GetDamageByAgentId(uint32_t agent_id, uint32_t* party_index_out = nullptr) {
        const auto found = party_index.find(agent_id);
        if (found == party_index.end())
            return nullptr;
        const auto party_idx = found->second;
        if (party_idx >= damage.size())
            return nullptr;
        if (party_index_out)
            *party_index_out = party_idx;
        return &damage[party_idx];
    }

    const wchar_t* GetDamageName(uint32_t index) {
        return party_names_by_index[index]->wstring().c_str();
    }

    bool GetFramePosition(GW::UI::Frame* frame, GW::UI::Frame* relative_to, ImVec2* top_left, ImVec2* bottom_right) {
        if (!(frame && relative_to))
            return false;
        const auto& rp = relative_to->position;
        const auto viewport_scale = rp.GetViewportScale();
        const auto& p = frame->position;

        if (top_left) {
            *top_left = {
                p.screen_left * viewport_scale.x,
                (rp.viewport_height - p.screen_top) * viewport_scale.y
            };
        }
        if (bottom_right) {
            *bottom_right = {
                p.screen_right * viewport_scale.x,
                (rp.viewport_height - p.screen_bottom) * viewport_scale.y
            };
        }
        return true;
    }

    void DrawFramePosition(GW::UI::Frame* frame) {
        const auto relative_to = GW::UI::GetFrameByLabel(L"Game");
        ImVec2 top_left;
        ImVec2 bottom_right;
        if (GetFramePosition(frame, relative_to, &top_left, &bottom_right))
            ImGui::GetForegroundDrawList()->AddRect(top_left, bottom_right, IM_COL32_WHITE);
    }

    float GetPartOfTotal(uint32_t dmg) {
        if (total == 0) {
            return 0;
        }
        return static_cast<float>(dmg) / total;
    }
    float GetPercentageOfTotal(const uint32_t dmg) { 
        return GetPartOfTotal(dmg) * 100.0f; 
    }

    void WriteDamageOf(size_t index, uint32_t rank = 0) {
        if (index >= damage.size()) {
            return;
        }
        if (damage[index].damage <= 0) {
            return;
        }

        if (rank == 0) {
            rank = 1; // start at 1, add 1 for each player with higher damage
            for (size_t i = 0; i < damage.size(); ++i) {
                if (i == index) {
                    continue;
                }
                if (damage[i].agent_id == 0) {
                    continue;
                }
                if (damage[i].damage > damage[index].damage) {
                    ++rank;
                }
            }
        }

        constexpr size_t buffer_size = 130;
        wchar_t buffer[buffer_size];
        swprintf_s(buffer, buffer_size, L"#%2d ~ %3.2f %% ~ %ls/%ls %ls ~ %d",
            rank,
            GetPercentageOfTotal(damage[index].damage),
            GetWProfessionAcronym(damage[index].primary),
            GetWProfessionAcronym(damage[index].secondary),
            GetDamageName(index),
            damage[index].damage);

        send_queue.push(buffer);
    }
    void WritePartyDamage() {
        std::vector<size_t> idx(damage.size());
        for (size_t i = 0; i < damage.size(); ++i) {
            idx[i] = i;
        }
        sort(idx.begin(), idx.end(), [](const size_t i1, const size_t i2) {
            return damage[i1].damage > damage[i2].damage;
            });

        for (size_t i = 0; i < idx.size(); ++i) {
            WriteDamageOf(idx[i], i + 1);
        }
        send_queue.push(L"Total ~ 100 % ~ " + std::to_wstring(total));
    }
    void WriteOwnDamage();
    void ResetDamage();

    void CreatePartyIndexMap()
    {
        if (!GW::PartyMgr::GetIsPartyLoaded()) {
            return;
        }
        party_index.clear();

        auto append_agent = [](uint32_t agent_id) {
            party_index[agent_id] = party_index.size();
            while (party_names_by_index.size() < party_index.size()) {
                party_names_by_index.push_back(new GuiUtils::EncString());
            }
            auto str = party_names_by_index[party_index.size() - 1];
            str->reset(GW::Agents::GetAgentEncName(agent_id))
                ->wstring(); // Trigger decode
            };
        const GW::PartyInfo* const info = GW::PartyMgr::GetPartyInfo();
        for (const GW::PlayerPartyMember& player : info->players) {
            const uint32_t id = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
            append_agent(id);
            for (const GW::HeroPartyMember& hero : info->heroes) {
                if (hero.owner_player_id == player.login_number) {
                    append_agent(hero.agent_id);
                }
            }
        }
        for (const GW::HenchmanPartyMember& hench : info->henchmen) {
            append_agent(hench.agent_id);
        }
        if (damage.size() < party_index.size()) {
            damage.resize(party_index.size());
        }
    }

    bool RecalculatePartyPositions() {
        agent_health_bar_positions.clear();
        ImVec2 top_left;
        ImVec2 bottom_right;
        const auto party = GW::PartyMgr::GetPartyInfo();
        const auto party_frame = party ? GW::UI::GetFrameByLabel(L"Party") : nullptr;
        if(!(party_frame && party_frame->IsVisible()))
            return false;

        GW::UI::Frame* party_window_health_bars = nullptr;

        // Traverse to health bars
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            auto sub_frame = GW::UI::GetChildFrame(party_frame, 1);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 8);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
            party_window_health_bars = GW::UI::GetChildFrame(sub_frame, 0);
        }
        else {
            auto sub_frame = GW::UI::GetChildFrame(party_frame, 0);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
            party_window_health_bars = GW::UI::GetChildFrame(sub_frame, 0);
        }

        const auto player_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 0);
        if(!player_health_bars) // Child frames by player id (includes heroes)
            return false;


        const auto game_frame = GW::UI::GetFrameByLabel(L"Game");
        GetFramePosition(player_health_bars, game_frame, &party_health_bars_position.first, &party_health_bars_position.second);
        GetFramePosition(party_frame, game_frame, &party_frame_position.first, &party_frame_position.second);

        for (auto& player : party->players) {
            const auto player_container = GW::UI::GetChildFrame(player_health_bars, player.login_number);
            const auto player_health_bar = GW::UI::GetChildFrame(player_container, 0);
            if (!player_health_bar)
                return false;
            const auto player_agent = GW::Agents::GetPlayerByID(player.login_number);
            if (!player_agent)
                return false;
            GetFramePosition(player_health_bar, game_frame, &top_left, &bottom_right);
            agent_health_bar_positions[player_agent->agent_id] = { top_left, bottom_right };
            for (auto& hero : party->heroes) {
                if (hero.owner_player_id != player.login_number)
                    continue;
                const auto agent_health_bar = GW::UI::GetChildFrame(player_container, 4 + hero.agent_id);
                if (!agent_health_bar)
                    continue;
                GetFramePosition(agent_health_bar, game_frame, &top_left, &bottom_right);
                agent_health_bar_positions[hero.agent_id] = { top_left, bottom_right };
                party_health_bars_position.second = bottom_right;
            }
        }
        const auto henchmen_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 1); // Find matching henchmen frames by agent_id
        for (auto& henchman : party->henchmen) {
            if (!henchmen_health_bars)
                return false;
            const auto agent_health_bar = GW::UI::GetChildFrame(henchmen_health_bars, henchman.agent_id);
            if (!agent_health_bar)
                continue;
            GetFramePosition(agent_health_bar, game_frame, &top_left, &bottom_right);
            agent_health_bar_positions[henchman.agent_id] = { top_left, bottom_right };
            party_health_bars_position.second = bottom_right;
        }
        const auto other_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 2); // Find matching agent frames frames by agent_id
        for (auto& agent_id : party->others) {
            if (!henchmen_health_bars)
                return false;
            const auto agent_health_bar = GW::UI::GetChildFrame(other_health_bars, agent_id);
            if (!agent_health_bar)
                continue;
            GetFramePosition(agent_health_bar, game_frame, &top_left, &bottom_right);
            agent_health_bar_positions[agent_id] = { top_left, bottom_right };
            party_health_bars_position.second = bottom_right;
        }
        return true;
    }
    void MapLoadedCallback(GW::HookStatus*, const GW::Packet::StoC::MapLoaded*)
    {
        switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Outpost:
            in_explorable = false;
            break;
        case GW::Constants::InstanceType::Explorable:
            party_index.clear();
            if (!in_explorable) {
                in_explorable = true;
                ResetDamage();
            }
            break;
        case GW::Constants::InstanceType::Loading:
        default:
            break;
        }
    }

    void DamagePacketCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet)
    {
        // ignore non-damage packets
        switch (packet->type) {
        case GW::Packet::StoC::P156_Type::damage:
        case GW::Packet::StoC::P156_Type::critical:
        case GW::Packet::StoC::P156_Type::armorignoring:
            break;
        default:
            return;
        }

        // ignore heals
        if (packet->value >= 0) {
            return;
        }
        const auto cause = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->cause_id));
        if (!(cause && cause->GetIsLivingType()))
            return;// ignore player-inflicted damage
        if (cause->allegiance != GW::Constants::Allegiance::Ally_NonAttackable)
            return;
        const auto entry = GetDamageByAgentId(cause->agent_id);
        if (!entry)
            return;
        const auto target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->target_id));
        if (!(target && target->GetIsLivingType()))
            return; // ignore player-inflicted damage
        if (target->login_number != 0)
            return; // ignore player-inflicted damage
        // such as Life bond or sacrifice
        switch (target->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
        case GW::Constants::Allegiance::Spirit_Pet:
        case GW::Constants::Allegiance::Minion:
            return; // ignore damage inflicted to allies in general
        }

        long ldmg;
        if (target->max_hp > 0 && target->max_hp < 100000) {
            ldmg = std::lround(-packet->value * target->max_hp);
            hp_map[target->player_number] = target->max_hp;
        }
        else {
            const auto it = hp_map.find(target->player_number);
            if (it == hp_map.end()) {
                // max hp not found, approximate with hp/lvl formula
                ldmg = std::lround(-packet->value * (target->level * 20 + 100));
            }
            else {
                // size_t maxhp = it->second;
                ldmg = std::lround(-packet->value * it->second);
            }
        }

        const uint32_t dmg = static_cast<uint32_t>(ldmg);

        if (entry->damage == 0) {
            entry->agent_id = packet->cause_id;
            entry->primary = static_cast<GW::Constants::Profession>(cause->primary);
            entry->secondary = static_cast<GW::Constants::Profession>(cause->secondary);
        }

        entry->damage += dmg;
        total += dmg;

        entry->recent_damage += dmg;
        entry->last_damage = TIMER_INIT();
    }

    void ResetDamage()
    {
        total = 0;
        for (auto& entry : damage) {
            entry.Reset();
        }
    }
    void WriteOwnDamage() {
        uint32_t my_index = 0;
        const auto entry = GetDamageByAgentId(GW::Agents::GetPlayerId(), &my_index);
        if(entry)
            WriteDamageOf(my_index);
    }

    void CHAT_CMD_FUNC(CmdDamage)
    {
        if (argc <= 1) {
            WritePartyDamage();
        }
        else {
            const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
            if (arg1 == L"print" || arg1 == L"report") {
                WritePartyDamage();
            }
            else if (arg1 == L"me") {
                WriteOwnDamage();
            }
            else if (arg1 == L"reset") {
                ResetDamage();
            }
            else {
                uint32_t idx;
                if (GuiUtils::ParseUInt(argv[1], &idx)) {
                    WriteDamageOf(idx - 1);
                }
            }
        }
    }
}

void PartyDamage::Initialize()
{
    ToolboxWidget::Initialize();

    total = 0;
    send_timer = TIMER_INIT();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry, DamagePacketCallback,0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, MapLoadedCallback, 0x8000);

    GW::Chat::CreateCommand(L"dmg", CmdDamage);
    GW::Chat::CreateCommand(L"damage", CmdDamage);
    ResetDamage();
}

void PartyDamage::Terminate()
{
    ToolboxWidget::Terminate();
    GW::StoC::RemoveCallbacks(&GenericModifier_Entry);
    GW::StoC::RemoveCallbacks(&MapLoaded_Entry);
    GW::Chat::DeleteCommand(L"dmg");
    GW::Chat::DeleteCommand(L"damage");

    for (auto str : party_names_by_index) {
        delete str;
    }
    party_names_by_index.clear();

    if (inifile) {
        inifile->Reset();
        delete inifile;
        inifile = nullptr;
    }
}

void PartyDamage::Update(const float)
{
    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        send_timer = TIMER_INIT();
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
            && GW::Agents::GetPlayer()) {
            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
        }
    }

    if (party_index.empty()) {
        CreatePartyIndexMap();
    }

    // reset recent if needed
    for (auto& entry : damage) {
        if (TIMER_DIFF(entry.last_damage) > recent_max_time) {
            entry.recent_damage = 0;
        }
    }
}

void PartyDamage::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    //const float line_height = row_height > 0 && !snap_to_party_window ? row_height : GuiUtils::GetPartyHealthbarHeight();
    uint32_t size = GW::PartyMgr::GetPartySize();
    if (size != damage.size()) {
        CreatePartyIndexMap();
    }

    uint32_t max_recent = 0;
    uint32_t max = 0;
    for (const auto& i : damage) {
        if (max_recent < i.recent_damage) {
            max_recent = i.recent_damage;
        }

        if (max < i.damage) {
            max = i.damage;
        }
    }

    // @Cleanup: Only call RecalculatePartyPositions() when the party window has been moved or updated
    RecalculatePartyPositions();
    if (agent_health_bar_positions.empty())
        return;

    const Color damage_col_from = Colors::Add(color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_col_to = Colors::Sub(color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_from = Colors::Add(color_recent, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_to = Colors::Sub(color_recent, Colors::ARGB(0, 20, 20, 20));

    const int user_offset_x = abs(user_offset);
    float damage_x = party_frame_position.first.x - user_offset_x - width;
    if (damage_x < 0 || user_offset < 0) {
        // Right placement
        damage_x = party_frame_position.second.x + user_offset_x;
    }

    // Add a window to capture mouse clicks.
    ImGui::SetNextWindowPos({ damage_x,party_frame_position.first.y });
    ImGui::SetNextWindowSize({ width, ceil(party_frame_position.second.y - party_frame_position.first.y) });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    bool open = ImGui::Begin(Name(), nullptr, GetWinFlags());
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);
    if (!open)
        return;

    const auto draw_list = ImGui::GetForegroundDrawList();
    constexpr size_t buffer_size = 16;
    char buffer[buffer_size];

    const auto text_size = ImGui::CalcTextSize(" ");


    for (auto& [agent_id, health_bar_pos] : agent_health_bar_positions) {
        uint32_t this_agent_party_index = 0;
        const auto entry = GetDamageByAgentId(agent_id, &this_agent_party_index);
        if (!entry)
            continue;

        const ImVec2 damage_top_left = { damage_x, health_bar_pos.first.y };
        const ImVec2 damage_bottom_right = { damage_top_left.x + width, health_bar_pos.second.y };
        draw_list->AddRectFilled(damage_top_left, damage_bottom_right, color_background);

        const auto x = damage_top_left.x;

        const float& damage_float = static_cast<float>(entry->damage);
        // Total damage as percent of total team's total damage
        if (damage_float >= 0.f) {
            const float part_of_max = max > 0 ? damage_float / max : 0;
            const float bar_start_x = bars_left ? x + width * (1.0f - part_of_max) : x;
            const float bar_end_x = bars_left ? x + width : x + width * part_of_max;
            const auto bar_top_left = ImVec2(bar_start_x, damage_top_left.y);
            const auto bar_bottom_right = ImVec2(bar_end_x, damage_bottom_right.y);
            draw_list->AddRectFilledMultiColor(
                bar_top_left, bar_bottom_right, damage_col_from,
                damage_col_from, damage_col_to, damage_col_to
            );
        }

        // Recent damage as percent of total team's recent damage
        if (entry->recent_damage) {
            const float part_of_recent = max_recent > 0 ? static_cast<float>(entry->recent_damage) / max_recent : 0;
            const float recent_left = bars_left ? x + width * (1.0f - part_of_recent) : x;
            const float recent_right = bars_left ? x + width : x + width * part_of_recent;
            const auto recent_top_left = ImVec2(recent_left, damage_bottom_right.y - 6);
            const auto recent_top_right = ImVec2(recent_right, damage_bottom_right.y);
            draw_list->AddRectFilledMultiColor(
                recent_top_left, recent_top_right,
                damage_recent_from, damage_recent_from,
                damage_recent_to, damage_recent_to
            );
        }

        const auto text_y = damage_bottom_right.y - text_size.y;

        // Damage text - float
        if (damage_float < 1000.f) {
            snprintf(buffer, buffer_size, "%.0f", damage_float);
        }
        else if (damage_float < 1000.f * 10) {
            snprintf(buffer, buffer_size, "%.2f k", damage_float / 1000.f);
        }
        else if (damage_float < 1000.f * 1000.f) {
            snprintf(buffer, buffer_size, "%.1f k", damage_float / 1000.f);
        }
        else {
            snprintf(buffer, buffer_size, "%.2f m", damage_float / (1000.f * 1000.f));
        }

        draw_list->AddText(
            ImVec2(x + ImGui::GetStyle().ItemSpacing.x, text_y),
            IM_COL32(255, 255, 255, 255), buffer);

        // Damage text - percentage
        const float perc_of_total = GetPercentageOfTotal(entry->damage);
        snprintf(buffer, buffer_size, "%.1f %%", perc_of_total);
        draw_list->AddText(
            ImVec2(x + width / 2, text_y),
            IM_COL32(255, 255, 255, 255), buffer
        );

        if (print_by_click
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && ImGui::IsMouseInRect(damage_top_left, damage_bottom_right)
            && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            WriteDamageOf(this_agent_party_index, this_agent_party_index + 1);
        }
    }
}

void PartyDamage::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    width = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(width), 100.0f));
    LOAD_BOOL(bars_left);
    row_height = ini->GetLongValue(Name(), VAR_NAME(row_height), 0);
    recent_max_time = ini->GetLongValue(Name(), VAR_NAME(recent_max_time), 7000);
    color_background = Colors::Load(ini, Name(), VAR_NAME(color_background), Colors::ARGB(76, 0, 0, 0));
    color_damage = Colors::Load(ini, Name(), VAR_NAME(color_damage), Colors::ARGB(102, 205, 102, 51));
    color_recent = Colors::Load(ini, Name(), VAR_NAME(color_recent), Colors::ARGB(205, 102, 153, 230));
    LOAD_BOOL(hide_in_outpost);
    LOAD_BOOL(print_by_click);
    LOAD_BOOL(snap_to_party_window);
    LOAD_UINT(user_offset);

    if (inifile == nullptr) {
        inifile = new ToolboxIni(false, false, false);
    }
    inifile->LoadFile(Resources::GetSettingFile(INI_FILENAME).c_str());
    ToolboxIni::TNamesDepend keys;
    inifile->GetAllKeys(IniSection, keys);
    for (const ToolboxIni::Entry& key : keys) {
        int lkey;
        if (GuiUtils::ParseInt(key.pItem, &lkey)) {
            if (lkey <= 0) {
                continue;
            }
            const long lval = inifile->GetLongValue(IniSection, key.pItem, 0);
            if (lval <= 0) {
                continue;
            }
            hp_map[static_cast<size_t>(lkey)] = static_cast<uint32_t>(lval);
        }
    }

    is_movable = is_resizable = !snap_to_party_window;
}

void PartyDamage::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    ini->SetDoubleValue(Name(), VAR_NAME(width), width);
    SAVE_BOOL(bars_left);
    SAVE_UINT(row_height);
    SAVE_UINT(recent_max_time);
    SAVE_COLOR(color_background);
    SAVE_COLOR(color_damage);
    SAVE_COLOR(color_recent);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(print_by_click);
    SAVE_BOOL(snap_to_party_window);
    SAVE_UINT(user_offset);

    for (const auto& [player_number, hp] : hp_map) {
        std::string key = std::to_string(player_number);
        inifile->SetLongValue(IniSection, key.c_str(), hp, nullptr, false, true);
    }
    ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
}

void PartyDamage::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::Checkbox("Print Player Damage by Ctrl + Click", &print_by_click);
    if (ImGui::Checkbox("Attach to party window", &snap_to_party_window)) {
        is_movable = is_resizable = !snap_to_party_window;
    }
    if (snap_to_party_window) {
        ImGui::InputInt("Party window offset", &user_offset);
        ImGui::ShowHelp("Distance away from the party window");
    }
    ImGui::Checkbox("Bars towards the left", &bars_left);
    ImGui::ShowHelp("If unchecked, they will expand to the right");
    ImGui::DragFloat("Width", &width, 1.0f, 50.0f, 0.0f, "%.0f");
    if (!snap_to_party_window) {
        ImGui::InputInt("Row Height", &row_height);
        ImGui::ShowHelp("Height of each row, leave 0 for default");
    }
    if (width <= 0) {
        width = 1.0f;
    }
    ImGui::DragInt("Timeout", &recent_max_time, 10.0f, 1000, 10 * 1000, "%d milliseconds");
    if (recent_max_time < 0) {
        recent_max_time = 0;
    }
    ImGui::ShowHelp("After this amount of time, each player recent damage (blue bar) will be reset");
    Colors::DrawSettingHueWheel("Background", &color_background);
    Colors::DrawSettingHueWheel("Damage", &color_damage);
    Colors::DrawSettingHueWheel("Recent", &color_recent);
}
