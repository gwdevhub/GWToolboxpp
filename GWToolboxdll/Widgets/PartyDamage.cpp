#include "Color.h"
#include "stdafx.h"
#include "Timer.h"

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Widgets/PartyDamage.h>
#include <Utils/TextUtils.h>

constexpr const wchar_t* INI_FILENAME = L"healthlog.ini";
constexpr const char* IniSection = "health";

namespace {

    GW::HookEntry ChatCmd_HookEntry;

    // damage values


    uint32_t total = 0;
    uint32_t total_healing = 0;

    std::map<DWORD, uint32_t> hp_map{};
    

    // main routine variables
    bool in_explorable = false;
    clock_t send_timer = 0;
    std::queue<std::wstring> send_queue{};

    // ini
    ToolboxIni* inifile = nullptr;
    Color color_background = Colors::ARGB(76, 0, 0, 0);
    Color color_damage = Colors::ARGB(76, 0, 0, 0);
    Color color_recent = Colors::ARGB(205, 102, 153, 230);
    Color color_healing = Colors::ARGB(205, 102, 230, 102);
    float width = 100.0f;
    bool bars_left = true;
    int recent_max_time = 7000;
    bool hide_in_outpost = false;
    bool print_by_click = false;
    bool overlay_party_window = false;

    bool show_damage = true;
    bool show_healing = false;

    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 0;

    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry MapLoaded_Entry;

    float GetPartOfTotal(uint32_t dmg) {
        if (total == 0) {
            return 0;
        }
        return static_cast<float>(dmg) / total;
    }
    float GetPercentageOfTotal(const uint32_t dmg) {
        return GetPartOfTotal(dmg) * 100.0f;
    }
    float GetPartOfTotalHealing(uint32_t heal) {
        if (total_healing == 0) {
            return 0;
        }
        return static_cast<float>(heal) / total_healing;
    }
    float GetPercentageOfTotalHealing(const uint32_t heal) {
        return GetPartOfTotalHealing(heal) * 100.0f;
    }
}

struct PartyDamage::PlayerDamage {
    uint32_t damage = 0;
    uint32_t recent_damage = 0;
    clock_t last_damage = 0;
    uint32_t healing = 0;
    uint32_t recent_healing = 0;
    clock_t last_healing = 0;
    uint32_t agent_id = 0;
    GW::Constants::Profession primary = GW::Constants::Profession::None;
    GW::Constants::Profession secondary = GW::Constants::Profession::None;
    std::wstring name;

    void Reset()
    {
        damage = 0;
        recent_damage = 0;
        healing = 0;
        recent_healing = 0;
        agent_id = 0;
        primary = GW::Constants::Profession::None;
        secondary = GW::Constants::Profession::None;
        name.clear();
    }
};

std::vector<PartyDamage::PlayerDamage> PartyDamage::damage;
std::vector<PartyDamage::PlayerDamage> PartyDamage::departed_damage;
std::vector<uint32_t> PartyDamage::prev_party_agent_ids;

void PartyDamage::ReconcileDamageIndices() {
    if (party_agent_ids_by_index == prev_party_agent_ids)
        return;
    prev_party_agent_ids = party_agent_ids_by_index;

    // Map old agent_id -> index in current damage array
    std::unordered_map<uint32_t, uint32_t> old_agent_to_idx;
    for (uint32_t i = 0; i < damage.size(); i++) {
        if (damage[i].agent_id != 0)
            old_agent_to_idx[damage[i].agent_id] = i;
    }

    // Also index departed entries by name for map-transition recovery
    std::unordered_map<std::wstring, size_t> departed_by_name;
    for (size_t i = 0; i < departed_damage.size(); i++) {
        if (!departed_damage[i].name.empty())
            departed_by_name[departed_damage[i].name] = i;
    }

    std::vector<PlayerDamage> new_damage(party_agent_ids_by_index.size());
    std::unordered_set<uint32_t> claimed_old_indices;

    for (const auto& [agent_id, new_idx] : party_indeces_by_agent_id) {
        if (new_idx >= new_damage.size()) continue;
        if (new_idx >= pets_start_idx) continue;

        // Try matching by agent_id first
        auto it = old_agent_to_idx.find(agent_id);
        if (it != old_agent_to_idx.end()) {
            new_damage[new_idx] = damage[it->second];
            new_damage[new_idx].agent_id = agent_id;
            claimed_old_indices.insert(it->second);
            continue;
        }

        // Agent_id not found (map transition gives new IDs). Try by name.
        if (new_idx < party_names_by_index.size()) {
            const auto& name = party_names_by_index[new_idx]->wstring();
            if (!name.empty()) {
                // Check departed entries
                auto dit = departed_by_name.find(name);
                if (dit != departed_by_name.end()) {
                    new_damage[new_idx] = departed_damage[dit->second];
                    new_damage[new_idx].agent_id = agent_id;
                    departed_damage[dit->second].Reset();
                    continue;
                }
                // Check old damage entries by name
                for (uint32_t i = 0; i < damage.size(); i++) {
                    if (!claimed_old_indices.contains(i) && damage[i].name == name && !damage[i].name.empty()) {
                        new_damage[new_idx] = damage[i];
                        new_damage[new_idx].agent_id = agent_id;
                        claimed_old_indices.insert(i);
                        break;
                    }
                }
            }
        }
    }

    // Move unclaimed entries with data to departed
    for (uint32_t i = 0; i < damage.size(); i++) {
        if (claimed_old_indices.contains(i)) continue;
        if (damage[i].damage > 0 || damage[i].healing > 0)
            departed_damage.push_back(std::move(damage[i]));
    }

    damage = std::move(new_damage);

    // Recompute totals
    total = 0;
    total_healing = 0;
    for (const auto& entry : damage) {
        total += entry.damage;
        total_healing += entry.healing;
    }
    for (const auto& entry : departed_damage) {
        total += entry.damage;
        total_healing += entry.healing;
    }
}

void PartyDamage::WriteDamageOf(size_t index, uint32_t rank) {
    if (index >= damage.size()) {
        return;
    }
    if (damage[index].damage <= 0 && damage[index].healing <= 0) {
        return;
    }
    if (damage[index].name.empty()) {
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

    constexpr size_t buffer_size = 200;
    wchar_t buffer[buffer_size];

    const bool has_damage = damage[index].damage > 0;
    const bool has_healing = show_healing && damage[index].healing > 0;

    if (has_damage && has_healing) {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls/%ls %ls ~ Dmg: %3.2f%% (%d) ~ Heal: %3.2f%% (%d)",
            rank,
            GetWProfessionAcronym(damage[index].primary),
            GetWProfessionAcronym(damage[index].secondary),
            damage[index].name.c_str(),
            GetPercentageOfTotal(damage[index].damage),
            damage[index].damage,
            GetPercentageOfTotalHealing(damage[index].healing),
            damage[index].healing);
    }
    else if (has_damage) {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls/%ls %ls ~ Dmg: %3.2f%% (%d)",
            rank,
            GetWProfessionAcronym(damage[index].primary),
            GetWProfessionAcronym(damage[index].secondary),
            damage[index].name.c_str(),
            GetPercentageOfTotal(damage[index].damage),
            damage[index].damage);
    }
    else if (has_healing) {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls/%ls %ls ~ Heal: %3.2f%% (%d)",
            rank,
            GetWProfessionAcronym(damage[index].primary),
            GetWProfessionAcronym(damage[index].secondary),
            damage[index].name.c_str(),
            GetPercentageOfTotalHealing(damage[index].healing),
            damage[index].healing);
    }
    else {
        return; // Nothing to report
    }

    send_queue.push(buffer);
}
void PartyDamage::WritePartyDamage() {
    // Temporarily append departed members so WriteDamageOf can index them
    const size_t base = damage.size();
    for (const auto& entry : departed_damage) {
        if (entry.damage > 0 || entry.healing > 0)
            damage.push_back(entry);
    }

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
    send_queue.push(L"Total ~ Dmg: " + std::to_wstring(total) + L" ~ Heal: " + std::to_wstring(total_healing));

    // Remove the temporarily appended entries
    damage.resize(base);
}

void PartyDamage::MapLoadedCallback(GW::HookStatus*, const GW::Packet::StoC::MapLoaded*)
{
    switch (GW::Map::GetInstanceType()) {
    case GW::Constants::InstanceType::Outpost:
        in_explorable = false;
        break;
    case GW::Constants::InstanceType::Explorable:
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

void PartyDamage::DamagePacketCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet)
{
    // ignore non-damage/heal packets
    switch (packet->type) {
    case GW::Packet::StoC::P156_Type::damage:
    case GW::Packet::StoC::P156_Type::critical:
    case GW::Packet::StoC::P156_Type::armorignoring:
        break;
    default:
        return;
    }

    const bool is_heal = packet->value > 0;
    const bool is_damage = packet->value < 0;
    if (!is_heal && !is_damage)
        return;

    const auto cause = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->cause_id));
    if (!(cause && cause->GetIsLivingType()))
        return; // Ignore damage/heals caused by non-living agents
    if (cause->allegiance != GW::Constants::Allegiance::Ally_NonAttackable)
        return; // Ignore damage/heals caused by non-allied NPCs

    uint32_t party_idx = 0;
    auto entry = GetDamageByAgentId(cause->agent_id, &party_idx);
    if (!entry)
        return;

    const auto target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->target_id));
    if (!(target && target->GetIsLivingType()))
        return; // Ignore damage/heals on non-living agents

    if (is_damage) {
        // For damage: target must be enemy
        if (target->login_number != 0)
            return; // Ignore damage inflicted on other players such as Life bond or sacrifice
        switch (target->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
        case GW::Constants::Allegiance::Spirit_Pet:
        case GW::Constants::Allegiance::Minion:
            return; // ignore damage inflicted to allies in general
        }
    }
    else {
        // For healing: target must be ally
        switch (target->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
        case GW::Constants::Allegiance::Spirit_Pet:
        case GW::Constants::Allegiance::Minion:
            break; // allow healing to allies
        default:
            return; // ignore healing to enemies
        }
    }

    long lvalue;
    if (target->max_hp > 0 && target->max_hp < 100000) {
        lvalue = std::lround(std::abs(packet->value) * target->max_hp);
        hp_map[target->player_number] = target->max_hp;
    }
    else {
        const auto it = hp_map.find(target->player_number);
        if (it == hp_map.end()) {
            // max hp not found, approximate with hp/lvl formula
            lvalue = std::lround(std::abs(packet->value) * (target->level * 20 + 100));
        }
        else {
            lvalue = std::lround(std::abs(packet->value) * it->second);
        }
    }

    const uint32_t amount = static_cast<uint32_t>(lvalue);

    if (entry->damage == 0 && entry->healing == 0) {
        entry->agent_id = packet->cause_id;
        entry->primary = static_cast<GW::Constants::Profession>(cause->primary);
        entry->secondary = static_cast<GW::Constants::Profession>(cause->secondary);
    }

    if (entry->name.empty() && party_idx < party_names_by_index.size()) {
        const auto& decoded = party_names_by_index[party_idx]->wstring();
        if (!decoded.empty()) {
            entry->name = decoded;
        }
    }

    if (is_damage) {
        entry->damage += amount;
        total += amount;
        entry->recent_damage += amount;
        entry->last_damage = TIMER_INIT();
    }
    else {
        entry->healing += amount;
        total_healing += amount;
        entry->recent_healing += amount;
        entry->last_healing = TIMER_INIT();
    }
}

void PartyDamage::ResetDamage()
{
    total = 0;
    total_healing = 0;
    for (auto& entry : damage) {
        entry.Reset();
    }
    departed_damage.clear();
    prev_party_agent_ids.clear();
}
void PartyDamage::WriteOwnDamage() {
    uint32_t my_index = 0;
    const auto entry = GetDamageByAgentId(GW::Agents::GetControlledCharacterId(), &my_index);
    if (entry)
        WriteDamageOf(my_index);
}

void CHAT_CMD_FUNC(PartyDamage::CmdDamage)
{
    if (argc <= 1) {
        WritePartyDamage();
    }
    else {
        const std::wstring arg1 = TextUtils::ToLower(argv[1]);
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
            if (TextUtils::ParseUInt(argv[1], &idx)) {
                WriteDamageOf(idx - 1);
            }
        }
    }
}

PartyDamage::PlayerDamage* PartyDamage::GetDamageByAgentId(uint32_t agent_id, uint32_t* party_index_out) {
    const auto found = party_indeces_by_agent_id.find(agent_id);
    if (found == party_indeces_by_agent_id.end())
        return nullptr;
    const auto party_idx = found->second;
    if (party_idx >= damage.size())
        return nullptr;
    if (party_idx >= pets_start_idx)
        return nullptr; // Don't log damage for allies or pets
    if (party_index_out)
        *party_index_out = party_idx;
    return &damage[party_idx];
}

void PartyDamage::Initialize()
{
    SnapsToPartyWindow::Initialize();

    total = 0;
    send_timer = TIMER_INIT();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry, DamagePacketCallback,0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, MapLoadedCallback, 0x8000);

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"dmg", CmdDamage);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"damage", CmdDamage);
    ResetDamage();
}

void PartyDamage::Terminate()
{
    SnapsToPartyWindow::Terminate();
    GW::StoC::RemoveCallbacks(&GenericModifier_Entry);
    GW::StoC::RemoveCallbacks(&MapLoaded_Entry);
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);

    for (auto str : party_names_by_index) {
        str->Release();
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
            && GW::Agents::GetControlledCharacter()) {
            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
        }
    }

    // reset recent if needed
    for (auto& entry : damage) {
        if (TIMER_DIFF(entry.last_damage) > recent_max_time) {
            entry.recent_damage = 0;
        }
        if (TIMER_DIFF(entry.last_healing) > recent_max_time) {
            entry.recent_healing = 0;
        }
    }
    FetchPartyInfo();
    ReconcileDamageIndices();

    // Update names for damage entries whose names weren't decoded yet,
    // and restore departed entries when names become available
    for (const auto& [agent_id, party_idx] : party_indeces_by_agent_id) {
        if (party_idx >= damage.size() || party_idx >= party_names_by_index.size())
            continue;
        const auto& decoded = party_names_by_index[party_idx]->wstring();
        if (decoded.empty()) continue;

        if (damage[party_idx].name.empty() && damage[party_idx].agent_id != 0) {
            damage[party_idx].name = decoded;
        }

        // Restore departed entry if this slot is empty but a departed member matches by name
        if (damage[party_idx].damage == 0 && damage[party_idx].healing == 0) {
            for (auto& dep : departed_damage) {
                if (dep.name == decoded && (dep.damage > 0 || dep.healing > 0)) {
                    damage[party_idx] = dep;
                    damage[party_idx].agent_id = agent_id;
                    dep.Reset();
                    // Recompute totals
                    total = 0;
                    total_healing = 0;
                    for (const auto& e : damage) { total += e.damage; total_healing += e.healing; }
                    for (const auto& e : departed_damage) { total += e.damage; total_healing += e.healing; }
                    break;
                }
            }
        }
    }
}

void PartyDamage::Draw(IDirect3DDevice9* )
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

    // @Cleanup: Only call when the party window has been moved or updated
    if (party_agent_ids_by_index.empty() || !RecalculatePartyPositions()) {
        return;
    }
    if (damage.size() < party_agent_ids_by_index.size()) {
        damage.resize(party_agent_ids_by_index.size());
    }

    uint32_t max_recent = 0;
    uint32_t max = 0;
    uint32_t max_recent_healing = 0;
    uint32_t max_healing = 0;
    for (const auto& i : damage) {
        if (max_recent < i.recent_damage) {
            max_recent = i.recent_damage;
        }
        if (max < i.damage) {
            max = i.damage;
        }
        if (max_recent_healing < i.recent_healing) {
            max_recent_healing = i.recent_healing;
        }
        if (max_healing < i.healing) {
            max_healing = i.healing;
        }
    }

    const Color damage_col_from = Colors::Add(color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_col_to = Colors::Sub(color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_from = Colors::Add(color_recent, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_to = Colors::Sub(color_recent, Colors::ARGB(0, 20, 20, 20));
    const Color healing_from = Colors::Add(color_healing, Colors::ARGB(0, 20, 20, 20));
    const Color healing_to = Colors::Sub(color_healing, Colors::ARGB(0, 20, 20, 20));

    const auto user_offset_x = abs(static_cast<float>(user_offset));
    float window_x = .0f;
    if (overlay_party_window) {
        window_x = party_health_bars_position.top_left.x + user_offset_x;
        if (user_offset < 0) {
            window_x = party_health_bars_position.bottom_right.x - user_offset_x - width;
        }

    }
    else {
        window_x = party_health_bars_position.top_left.x - user_offset_x - width;
        if (window_x < 0 || user_offset < 0) {
            // Right placement
            window_x = party_health_bars_position.bottom_right.x + user_offset_x;
        }
    }

    // Add a window to capture mouse clicks.
    ImGui::SetNextWindowPos({ window_x, party_health_bars_position.top_left.y });
    ImGui::SetNextWindowSize({ width, party_health_bars_position.bottom_right.y - party_health_bars_position.top_left.y });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        const auto draw_list = ImGui::GetWindowDrawList();
        constexpr size_t buffer_size = 16;
        char buffer[buffer_size];


        for (auto& [agent_id, party_slot] : party_indeces_by_agent_id) {
            uint32_t this_agent_party_index = 0;
            const auto entry = GetDamageByAgentId(agent_id, &this_agent_party_index);
            if (!entry)
                continue;
            const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
            if (!health_bar_pos)
                continue;

            const ImVec2 damage_top_left = { window_x, health_bar_pos->top_left.y };
            const ImVec2 damage_bottom_right = { damage_top_left.x + width, health_bar_pos->bottom_right.y };
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

            // Recent damage as percent of total team's recent damage (bottom bar)
            if (show_damage && entry->recent_damage) {
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

            // Recent healing as percent of total team's recent healing (top bar)
            if (show_healing && entry->recent_healing) {
                const float part_of_recent_heal = max_recent_healing > 0 ? static_cast<float>(entry->recent_healing) / max_recent_healing : 0;
                const float heal_left = bars_left ? x + width * (1.0f - part_of_recent_heal) : x;
                const float heal_right = bars_left ? x + width : x + width * part_of_recent_heal;
                const auto heal_top_left = ImVec2(heal_left, damage_top_left.y);
                const auto heal_bottom_right = ImVec2(heal_right, damage_top_left.y + 6);
                draw_list->AddRectFilledMultiColor(
                    heal_top_left, heal_bottom_right,
                    healing_from, healing_from,
                    healing_to, healing_to
                );
            }

            const auto row_height = damage_bottom_right.y - damage_top_left.y;
            const auto text_height = ImGui::GetTextLineHeight();
            const auto text_y = damage_top_left.y + (row_height - text_height) / 2;

            if (show_damage) {
                // Damage text
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

                draw_list->AddText(ImVec2(x + ImGui::GetStyle().ItemSpacing.x, text_y), IM_COL32(255, 255, 255, 255), buffer);
            }


            if (show_healing) {
                // Healing text
                const float healing_float = static_cast<float>(entry->healing);
                if (healing_float < 1000.f) {
                    snprintf(buffer, buffer_size, "%.0f", healing_float);
                }
                else if (healing_float < 1000.f * 10) {
                    snprintf(buffer, buffer_size, "%.2f k", healing_float / 1000.f);
                }
                else if (healing_float < 1000.f * 1000.f) {
                    snprintf(buffer, buffer_size, "%.1f k", healing_float / 1000.f);
                }
                else {
                    snprintf(buffer, buffer_size, "%.2f m", healing_float / (1000.f * 1000.f));
                }

                draw_list->AddText(ImVec2(x + width / 2, text_y), color_healing, buffer);
            }


            if (print_by_click
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && ImGui::IsMouseInRect(damage_top_left, damage_bottom_right)
                && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                WriteDamageOf(this_agent_party_index, this_agent_party_index + 1);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);

    
}

void PartyDamage::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    width = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(width), width));
    LOAD_BOOL(bars_left);
    recent_max_time = ini->GetLongValue(Name(), VAR_NAME(recent_max_time), recent_max_time);
    LOAD_COLOR(color_background);
    LOAD_COLOR(color_damage);
    LOAD_COLOR(color_recent);
    LOAD_COLOR(color_healing);
    LOAD_BOOL(hide_in_outpost);
    LOAD_BOOL(print_by_click);
    LOAD_UINT(user_offset);
    LOAD_BOOL(overlay_party_window);
    LOAD_BOOL(show_damage);
    LOAD_BOOL(show_healing);


    if (inifile == nullptr) {
        inifile = new ToolboxIni(false, false, false);
    }
    inifile->LoadFile(Resources::GetSettingFile(INI_FILENAME).c_str());
    ToolboxIni::TNamesDepend keys;
    inifile->GetAllKeys(IniSection, keys);
    for (const ToolboxIni::Entry& key : keys) {
        int lkey;
        if (TextUtils::ParseInt(key.pItem, &lkey)) {
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
}

void PartyDamage::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    ini->SetDoubleValue(Name(), VAR_NAME(width), width);
    SAVE_BOOL(bars_left);
    SAVE_UINT(recent_max_time);
    SAVE_COLOR(color_background);
    SAVE_COLOR(color_damage);
    SAVE_COLOR(color_recent);
    SAVE_COLOR(color_healing);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(print_by_click);
    SAVE_UINT(user_offset);
    SAVE_BOOL(overlay_party_window);
    SAVE_BOOL(show_damage);
    SAVE_BOOL(show_healing);

    for (const auto& [player_number, hp] : hp_map) {
        std::string key = std::to_string(player_number);
        inifile->SetLongValue(IniSection, key.c_str(), hp, nullptr, false, true);
    }
    ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
}

void PartyDamage::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Print Player Damage by Ctrl + Click", &print_by_click);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Bars towards the left", &bars_left);
    ImGui::ShowHelp("If unchecked, they will expand to the right");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show damage", &show_damage);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show healing", &show_healing);

    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show on top of health bars", &overlay_party_window);
    ImGui::ShowHelp("Untick to show this widget to the left (or right) of the party window.\nTick to show this widget over the top of the party health bars inside the party window");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("Party window offset", &user_offset);
    ImGui::PopItemWidth();
    ImGui::ShowHelp("Distance away from the party window");

    ImGui::DragFloat("Width", &width, 1.0f, 50.0f, 0.0f, "%.0f");
    if (width <= 0) {
        width = 1.0f;
    }
    ImGui::DragInt("Timeout", &recent_max_time, 10.0f, 1000, 10 * 1000, "%d milliseconds");
    if (recent_max_time < 0) {
        recent_max_time = 0;
    }
    ImGui::ShowHelp("After this amount of time, each player's recent damage/healing bars will be reset");
    Colors::DrawSettingHueWheel("Background", &color_background);
    Colors::DrawSettingHueWheel("Damage", &color_damage);
    Colors::DrawSettingHueWheel("Recent Damage", &color_recent);
    Colors::DrawSettingHueWheel("Healing", &color_healing);
}
