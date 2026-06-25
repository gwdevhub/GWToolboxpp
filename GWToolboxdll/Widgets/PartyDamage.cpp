#include "Color.h"
#include "Timer.h"
#include "stdafx.h"

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/PartyDamage.h>

#include <cmath>

constexpr const wchar_t* INI_FILENAME = L"healthlog.ini";
constexpr const wchar_t* JSON_FILENAME = L"healthlog.json";

namespace {
    GW::HookEntry ChatCmd_HookEntry;

    // damage values


    uint32_t total = 0;
    uint32_t total_healing = 0;

    clock_t first_packet_time = 0;
    clock_t last_packet_time = 0;
    clock_t accumulated_combat_time_ms = 0;

    // Returns total combat time including the current battle segment,
    // up to the last damage packet time (not wall clock).
    clock_t GetEffectiveCombatTime() {
        if (first_packet_time == 0)
            return accumulated_combat_time_ms;
        return accumulated_combat_time_ms + (last_packet_time - first_packet_time);
    }

    // Condition DPS tracking
    enum class ConditionType : uint8_t { Bleeding, Poison, Disease, Burning, Count };
    constexpr uint32_t CONDITION_DPS_RATES[] = { 6, 8, 8, 14 };
    struct CondTracker {
        uint32_t agent_id = 0;
        clock_t apply_time[4] = {}; // bleeding, poison, disease, burning
    };
    CondTracker cond_trackers[64] = {};
    uint32_t cond_tracker_count = 0;
    double cond_damage[4] = {}; // accumulated damage per condition

    int CondIdxFromEffectId(uint32_t effect_id) {
        switch (effect_id) {
            case 23: return 0; // bleeding
            case 27: return 1; // poison
            case 26: return 2; // disease
            case 25: return 3; // burning
            default: return -1;
        }
    }

    int FindTrackerByAgentId(uint32_t agent_id) {
        for (uint32_t ti = 0; ti < cond_tracker_count; ti++) {
            if (cond_trackers[ti].agent_id == agent_id)
                return static_cast<int>(ti);
        }
        return -1;
    }

    // Finalize damage for all active conditions on a tracker, then remove it
    void FinalizeAndRemoveTracker(uint32_t ti) {
        if (ti >= cond_tracker_count) return;
        const clock_t now = TIMER_INIT();
        for (int ci = 0; ci < 4; ci++) {
            if (cond_trackers[ti].apply_time[ci] != 0) {
                const double elapsed = static_cast<double>(now - cond_trackers[ti].apply_time[ci]) / 1000.0;
                cond_damage[ci] += static_cast<double>(CONDITION_DPS_RATES[ci]) * elapsed;
            }
        }
        cond_tracker_count--;
        cond_trackers[ti] = cond_trackers[cond_tracker_count];
    }

    GW::HookEntry AgentRemove_Entry;
    GW::HookEntry AgentState_Entry;

    void ConditionAgentRemoveCallback(GW::HookStatus*, const GW::Packet::StoC::AgentRemove* packet) {
        const int ti = FindTrackerByAgentId(packet->agent_id);
        if (ti >= 0) FinalizeAndRemoveTracker(static_cast<uint32_t>(ti));
    }

    void ConditionAgentStateCallback(GW::HookStatus*, const GW::Packet::StoC::AgentState* packet) {
        if ((packet->state & 16) == 0) return;
        const int ti = FindTrackerByAgentId(packet->agent_id);
        if (ti >= 0) FinalizeAndRemoveTracker(static_cast<uint32_t>(ti));
    }

    std::map<DWORD, DWORD> hp_map_nm{};
    std::map<DWORD, DWORD> hp_map_hm{};
    const std::pair<const char*, std::map<DWORD, DWORD>*> section_maps[] = {{"health_nm", &hp_map_nm}, {"health_hm", &hp_map_hm}};

    // main routine variables
    bool in_explorable = false;
    clock_t send_timer = 0;
    std::queue<std::wstring> send_queue{};

    PartyDamage::Settings settings;

    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry GenericValue_Entry;
    GW::HookEntry MapLoaded_Entry;

    float GetPercentageOfTotal(const uint32_t val)
    {
        return total == 0 ? 0.0f : 100.0f * static_cast<float>(val) / total;
    }

    float GetPercentageOfTotalHealing(const uint32_t val)
    {
        return total_healing == 0 ? 0.0f : 100.0f * static_cast<float>(val) / total_healing;
    }
} // namespace

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

void PartyDamage::ReconcileDamageIndices()
{
    if (party_agent_ids_by_index == prev_party_agent_ids) return;
    prev_party_agent_ids = party_agent_ids_by_index;

    // Map old agent_id -> index in current damage array
    std::unordered_map<uint32_t, uint32_t> old_agent_to_idx;
    for (uint32_t i = 0; i < damage.size(); i++) {
        if (damage[i].agent_id != 0) old_agent_to_idx[damage[i].agent_id] = i;
    }

    // Also index departed entries by name for map-transition recovery
    std::unordered_map<std::wstring, size_t> departed_by_name;
    for (size_t i = 0; i < departed_damage.size(); i++) {
        if (!departed_damage[i].name.empty()) departed_by_name[departed_damage[i].name] = i;
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
                    if (!claimed_old_indices.count(i) && damage[i].name == name && !damage[i].name.empty()) {
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
        if (claimed_old_indices.count(i)) continue;
        if (damage[i].damage > 0 || damage[i].healing > 0)
            departed_damage.push_back(std::move(damage[i]));
    }

    damage = std::move(new_damage);

    total = 0;
    total_healing = 0;
    for (const auto& e : damage) {
        total += e.damage;
        total_healing += e.healing;
    }
    for (const auto& e : departed_damage) {
        total += e.damage;
        total_healing += e.healing;
    }
}

void PartyDamage::WriteDamageOf(size_t index, uint32_t rank)
{
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

    const auto& entry = damage[index];
    std::wstring prof_str;
    if (entry.primary != GW::Constants::Profession::None) {
        auto* prim = ToolboxUtils::GetProfessionAcronym(entry.primary);
        auto* sec = ToolboxUtils::GetProfessionAcronym(entry.secondary);
        if (prim && sec)
            prof_str = prim->wstring() + L"/" + sec->wstring();
    }

    const bool has_healing = settings.show_healing && entry.healing > 0;

    if (has_healing && entry.damage > 0) {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls %ls ~ Dmg: %3.2f%% (%d) ~ Heal: %3.2f%% (%d)",
            rank, prof_str.c_str(), entry.name.c_str(),
            GetPercentageOfTotal(entry.damage), entry.damage,
            GetPercentageOfTotalHealing(entry.healing), entry.healing);
    }
    else if (has_healing) {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls %ls ~ Heal: %3.2f%% (%d)",
            rank, prof_str.c_str(), entry.name.c_str(),
            GetPercentageOfTotalHealing(entry.healing), entry.healing);
    }
    else {
        swprintf_s(buffer, buffer_size, L"#%2d ~ %ls %ls ~ Dmg: %3.2f%% (%d)",
            rank, prof_str.c_str(), entry.name.c_str(),
            GetPercentageOfTotal(entry.damage), entry.damage);
    }

    send_queue.push(buffer);
}

void PartyDamage::WritePartyDamage()
{
    // Temporarily append departed members so WriteDamageOf can index them
    const size_t base = damage.size();
    for (const auto& entry : departed_damage) {
        if (entry.damage > 0 || entry.healing > 0) damage.push_back(entry);
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
    cond_tracker_count = 0;

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

void PartyDamage::ConditionValueCallback(GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet)
{
    const int ci = CondIdxFromEffectId(packet->value);
    if (packet->value_id == GW::Packet::StoC::GenericValueID::add_effect) {
        if (ci < 0) return;
        const auto target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->agent_id));
        if (!target || target->allegiance != GW::Constants::Allegiance::Enemy) return;
        int ti = FindTrackerByAgentId(packet->agent_id);
        if (ti < 0) {
            ti = static_cast<int>(cond_tracker_count);
            if (ti >= 64) return;
            cond_trackers[ti].agent_id = packet->agent_id;
            cond_tracker_count++;
        }
        cond_trackers[ti].apply_time[ci] = TIMER_INIT();
        return;
    }
    if (packet->value_id == GW::Packet::StoC::GenericValueID::remove_effect) {
        if (ci < 0) return;
        const int ti = FindTrackerByAgentId(packet->agent_id);
        if (ti < 0 || cond_trackers[ti].apply_time[ci] == 0) return;
        const clock_t now = TIMER_INIT();
        const double elapsed = static_cast<double>(now - cond_trackers[ti].apply_time[ci]) / 1000.0;
        cond_damage[ci] += static_cast<double>(CONDITION_DPS_RATES[ci]) * elapsed;
        cond_trackers[ti].apply_time[ci] = 0; // clear it
        // If all conditions cleared, remove tracker
        bool all_cleared = true;
        for (int cj = 0; cj < 4; cj++) {
            if (cond_trackers[ti].apply_time[cj] != 0) { all_cleared = false; break; }
        }
        if (all_cleared) {
            cond_tracker_count--;
            cond_trackers[ti] = cond_trackers[cond_tracker_count];
        }
        return;
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
    if (!is_heal && !is_damage) return;

    const auto cause = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->cause_id));
    if (!(cause && cause->GetIsLivingType())) return;                               // Ignore damage/heals caused by non-living agents
    if (cause->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) return; // Ignore damage/heals caused by non-allied NPCs

    uint32_t party_idx = 0;
    auto entry = GetDamageByAgentId(cause->agent_id, &party_idx);
    if (!entry) return;

    const auto target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->target_id));
    if (!(target && target->GetIsLivingType())) return; // Ignore damage/heals on non-living agents

    if (is_damage) {
        // For damage: target must be enemy
        if (target->login_number != 0) return; // Ignore damage inflicted on other players such as Life bond or sacrifice
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
    auto& hp_map = GW::PartyMgr::GetIsPartyInHardMode() ? hp_map_hm : hp_map_nm;
    const float magnitude = std::min(is_heal ? 1.0f - target->hp : target->hp, std::fabs(packet->value));
    if (target->max_hp > 0 && target->max_hp < 100000) {
        lvalue = std::lround(magnitude * target->max_hp);
        hp_map[target->player_number] = target->max_hp;
    }
    else {
        const auto it = hp_map.find(target->player_number);
        if (it == hp_map.end()) {
            // max hp not found, approximate with hp/lvl formula
            lvalue = std::lround(magnitude * (target->level * 20 + 100));
        }
        else {
            lvalue = std::lround(magnitude * it->second);
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
        const clock_t now = TIMER_INIT();
        entry->damage += amount;
        total += amount;
        entry->recent_damage += amount;
        entry->last_damage = now;
        if (first_packet_time == 0) {
            first_packet_time = now;
            last_packet_time = now;
        }
        else {
            const clock_t elapsed = now - last_packet_time;
            if (elapsed > 5000 && first_packet_time != 0) {
                accumulated_combat_time_ms += (last_packet_time - first_packet_time);
                first_packet_time = now;
            }
            last_packet_time = now;
        }
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
    first_packet_time = 0;
    last_packet_time = 0;
    accumulated_combat_time_ms = 0;
    for (auto& d : cond_damage) {
        d = 0.0;
    }
}

void PartyDamage::WriteOwnDamage()
{
    uint32_t my_index = 0;
    const auto entry = GetDamageByAgentId(GW::Agents::GetControlledCharacterId(), &my_index);
    if (entry) WriteDamageOf(my_index);
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

PartyDamage::PlayerDamage* PartyDamage::GetDamageByAgentId(uint32_t agent_id, uint32_t* party_index_out)
{
    const auto found = party_indeces_by_agent_id.find(agent_id);
    if (found == party_indeces_by_agent_id.end()) return nullptr;
    const auto party_idx = found->second;
    if (party_idx >= damage.size()) return nullptr;
    if (party_idx >= pets_start_idx) return nullptr; // Don't log damage for allies or pets
    if (party_index_out) *party_index_out = party_idx;
    return &damage[party_idx];
}

void PartyDamage::Initialize()
{
    SnapsToPartyWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    total = 0;
    send_timer = TIMER_INIT();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry, DamagePacketCallback, 0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, MapLoadedCallback, 0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry, ConditionValueCallback, 0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry, ConditionAgentStateCallback, 0x8000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Entry, ConditionAgentRemoveCallback, 0x8000);

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"dmg", CmdDamage);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"damage", CmdDamage);
    ResetDamage();
}

void PartyDamage::Terminate()
{
    SnapsToPartyWindow::Terminate();
    GW::StoC::RemoveCallbacks(&GenericModifier_Entry);
    GW::StoC::RemoveCallbacks(&GenericValue_Entry);
    GW::StoC::RemoveCallbacks(&MapLoaded_Entry);
    GW::StoC::RemoveCallbacks(&AgentState_Entry);
    GW::StoC::RemoveCallbacks(&AgentRemove_Entry);
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);

    party_names_by_index.clear();
}

void PartyDamage::Update(const float)
{
    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        send_timer = TIMER_INIT();
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetControlledCharacter()) {
            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
        }
    }

    // reset recent if needed
    for (auto& entry : damage) {
        if (TIMER_DIFF(entry.last_damage) > settings.recent_max_time) {
            entry.recent_damage = 0;
        }
        if (TIMER_DIFF(entry.last_healing) > settings.recent_max_time) {
            entry.recent_healing = 0;
        }
    }
    FetchPartyInfo();
    ReconcileDamageIndices();

    // Update names for damage entries whose names weren't decoded yet,
    // and restore departed entries when names become available
    for (const auto& [agent_id, party_idx] : party_indeces_by_agent_id) {
        if (party_idx >= damage.size() || party_idx >= party_names_by_index.size()) continue;
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
                    total = 0;
                    total_healing = 0;
                    for (const auto& e : damage) {
                        total += e.damage;
                        total_healing += e.healing;
                    }
                    for (const auto& e : departed_damage) {
                        total += e.damage;
                        total_healing += e.healing;
                    }
                    break;
                }
            }
        }
    }
}

static void FormatValueString(char* buffer, size_t size, float value) {
    if (value < 1000.f)
        snprintf(buffer, size, "%.0f", value);
    else if (value < 10000.f)
        snprintf(buffer, size, "%.2f k", value / 1000.f);
    else if (value < 1000000.f)
        snprintf(buffer, size, "%.1f k", value / 1000.f);
    else
        snprintf(buffer, size, "%.2f m", value / (1000.f * 1000.f));
}

static void DrawGradientBar(ImDrawList* draw_list, float x, float width, float top_y, float bottom_y, bool bars_left, float part, Color col_from, Color col_to) {
    if (part <= 0.f) return;
    const float left = bars_left ? x + width * (1.0f - part) : x;
    const float right = bars_left ? x + width : x + width * part;
    draw_list->AddRectFilledMultiColor(
        ImVec2(left, top_y), ImVec2(right, bottom_y),
        col_from, col_from, col_to, col_to
    );
}

void PartyDamage::Draw(IDirect3DDevice9*)
{
    if (!visible || (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    // @Cleanup: Only call when the party window has been moved or updated
    const clock_t combat_time = GetEffectiveCombatTime();

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
        if (max_recent < i.recent_damage) max_recent = i.recent_damage;
        if (max < i.damage) max = i.damage;
        if (max_recent_healing < i.recent_healing) max_recent_healing = i.recent_healing;
        if (max_healing < i.healing) max_healing = i.healing;
    }

    const Color damage_col_from = Colors::Add(settings.color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_col_to = Colors::Sub(settings.color_damage, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_from = Colors::Add(settings.color_recent, Colors::ARGB(0, 20, 20, 20));
    const Color damage_recent_to = Colors::Sub(settings.color_recent, Colors::ARGB(0, 20, 20, 20));
    const Color healing_from = Colors::Add(settings.color_healing, Colors::ARGB(0, 20, 20, 20));
    const Color healing_to = Colors::Sub(settings.color_healing, Colors::ARGB(0, 20, 20, 20));

    const auto width = settings.width;
    const auto user_offset_x = abs(static_cast<float>(settings.user_offset));
    float window_x = .0f;
    if (settings.overlay_party_window) {
        window_x = party_health_bars_position.top_left.x + user_offset_x;
        if (settings.user_offset < 0) {
            window_x = party_health_bars_position.bottom_right.x - user_offset_x - width;
        }
    }
    else {
        window_x = party_health_bars_position.top_left.x - user_offset_x - width;
        if (window_x < 0 || settings.user_offset < 0) {
            // Right placement
            window_x = party_health_bars_position.bottom_right.x + user_offset_x;
        }
    }

    const float cond_h = settings.show_condition_dps ? ImGui::GetTextLineHeight() + 6.0f : 0.0f;

    // Add a window to capture mouse clicks.
    ImGui::SetNextWindowPos({window_x, party_health_bars_position.top_left.y - cond_h});
    ImGui::SetNextWindowSize({width, party_health_bars_position.bottom_right.y - party_health_bars_position.top_left.y + cond_h});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        const auto draw_list = ImGui::GetWindowDrawList();
        constexpr size_t buffer_size = 16;
        char buffer[buffer_size];

        // Condition DPS header row
        if (settings.show_condition_dps) {
            const struct { uint32_t color; const char* icon; int ci; } conds[] = {
                {IM_COL32(200, 50, 50, 255), ICON_FA_TINT, 0},
                {IM_COL32(255, 120, 0, 255), ICON_FA_FIRE, 3},
                {IM_COL32(120, 200, 80, 255), ICON_FA_TINT, 1},
                {IM_COL32(200, 100, 200, 255), ICON_FA_SKULL, 2},
            };
            for (const auto& c : conds) {
                const uint32_t dps = (combat_time == 0) ? 0 : static_cast<uint32_t>(std::llround(cond_damage[c.ci] * 1000.0 / static_cast<double>(combat_time)));
                if (dps == 0) continue;
                ImGui::PushStyleColor(ImGuiCol_Text, c.color);
                ImGui::Text(c.icon);
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%d/s", dps);
                ImGui::SameLine();
            }
            ImGui::NewLine(); // flush last SameLine
        }

        for (auto& [agent_id, party_slot] : party_indeces_by_agent_id) {
            uint32_t this_agent_party_index = 0;
            const auto entry = GetDamageByAgentId(agent_id, &this_agent_party_index);
            if (!entry) continue;
            const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
            if (!health_bar_pos) continue;

            const ImVec2 damage_top_left = {window_x, health_bar_pos->top_left.y};
            const ImVec2 damage_bottom_right = {damage_top_left.x + width, health_bar_pos->bottom_right.y};
            draw_list->AddRectFilled(damage_top_left, damage_bottom_right, settings.color_background);

            const auto x = damage_top_left.x;

            const float damage_float = static_cast<float>(entry->damage);
            // Total damage bar
            DrawGradientBar(draw_list, x, width, damage_top_left.y, damage_bottom_right.y, settings.bars_left, max > 0 ? damage_float / max : 0, damage_col_from, damage_col_to);

            // Recent damage bar (bottom)
            if (settings.show_damage && entry->recent_damage) {
                DrawGradientBar(draw_list, x, width, damage_bottom_right.y - 6, damage_bottom_right.y, settings.bars_left, max_recent > 0 ? static_cast<float>(entry->recent_damage) / max_recent : 0, damage_recent_from, damage_recent_to);
            }

            // Recent healing bar (top)
            if (settings.show_healing && entry->recent_healing) {
                DrawGradientBar(draw_list, x, width, damage_top_left.y, damage_top_left.y + 6, settings.bars_left, max_recent_healing > 0 ? static_cast<float>(entry->recent_healing) / max_recent_healing : 0, healing_from, healing_to);
            }

            const auto row_height = damage_bottom_right.y - damage_top_left.y;
            const auto text_height = ImGui::GetTextLineHeight();
            const auto text_y = damage_top_left.y + (row_height - text_height) / 2;

            if (settings.show_damage && entry->damage > 0) {
                FormatValueString(buffer, buffer_size, damage_float);
                draw_list->AddText(ImVec2(x + ImGui::GetStyle().ItemSpacing.x, text_y), IM_COL32(255, 255, 255, 255), buffer);

                // Damage percentage
                if (!settings.show_healing) {
                    const float perc_of_total = GetPercentageOfTotal(entry->damage);
                    snprintf(buffer, buffer_size, "%.1f %%", perc_of_total);
                    draw_list->AddText(ImVec2(x + width / 2, text_y), IM_COL32(255, 255, 255, 255), buffer);
                }
            }

            if (settings.show_dps && settings.show_damage && entry->damage > 0) {
                const uint32_t dps = combat_time == 0 ? 0 : static_cast<uint32_t>(std::llround(static_cast<double>(entry->damage) * 1000.0 / static_cast<double>(combat_time)));
                snprintf(buffer, buffer_size, "%d/s", dps);
                const float dps_text_x = x + width * 0.75f;
                draw_list->AddText(ImVec2(dps_text_x, text_y), IM_COL32(255, 255, 255, 255), buffer);
            }

            if (settings.show_healing && entry->healing > 0) {
                FormatValueString(buffer, buffer_size, static_cast<float>(entry->healing));

                const float heal_text_x = settings.show_damage ? x + width / 2 : x + ImGui::GetStyle().ItemSpacing.x;
                draw_list->AddText(ImVec2(heal_text_x, text_y), settings.color_healing, buffer);

                // Healing percentage
                if (!settings.show_damage) {
                    const float perc_of_total_heal = GetPercentageOfTotalHealing(entry->healing);
                    snprintf(buffer, buffer_size, "%.1f %%", perc_of_total_heal);
                    draw_list->AddText(ImVec2(x + width / 2, text_y), settings.color_healing, buffer);
                }
            }


            if (settings.print_by_click && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseInRect(damage_top_left, damage_bottom_right) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                WriteDamageOf(this_agent_party_index, this_agent_party_index + 1);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);
}

void PartyDamage::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    const auto json_path = Resources::GetSettingFile(JSON_FILENAME);
    std::error_code ec;
    if (std::filesystem::exists(json_path, ec)) {
        std::ifstream file(json_path, std::ios::binary);
        const std::string buffer{std::istreambuf_iterator(file), {}};
        std::map<std::string, std::map<DWORD, DWORD>> loaded;
        if (file && !glz::read<glz::opts{.error_on_unknown_keys = false}>(loaded, buffer)) {
            for (const auto& [section, map] : section_maps) {
                const auto found = loaded.find(section);
                if (found != loaded.end()) {
                    *map = std::move(found->second);
                }
            }
        }
        // An empty json may be the result of a buggy first migration; fall through to the ini to recover
        if (!hp_map_nm.empty() || !hp_map_hm.empty()) return;
    }
    ToolboxIni inifile(false, false, false);
    inifile.LoadFile(Resources::GetLegacySettingFile(INI_FILENAME).c_str());
    const auto read_section = [&inifile](const char* section, std::map<DWORD, DWORD>& map, const bool keep_existing) {
        TNamesDepend keys;
        inifile.GetAllKeys(section, keys);
        for (const auto& key : keys) {
            uint32_t player_number = 0;
            TextUtils::ParseUInt(key.pItem, &player_number);
            const long hp = inifile.GetLongValue(section, key.pItem, 0);
            if (hp <= 0) continue;
            if (keep_existing && map.count(player_number)) continue;
            map[player_number] = static_cast<DWORD>(hp);
        }
    };
    for (const auto& [section, map] : section_maps) {
        read_section(section, *map, false);
    }
    // Legacy [health] section pre-dates the nm/hm split; treat as normal mode, newer keys win
    read_section("health", hp_map_nm, true);
}

void PartyDamage::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    std::map<std::string, std::map<DWORD, DWORD>> out;
    for (const auto& [section, map] : section_maps) {
        out[section] = *map;
    }
    std::string buffer;
    if (glz::write<glz::opts{.prettify = true}>(out, buffer)) {
        return;
    }
    std::ofstream file(Resources::GetSettingFile(JSON_FILENAME), std::ios::binary | std::ios::trunc);
    ASSERT(file && file.write(buffer.data(), static_cast<std::streamsize>(buffer.size())).good());
}
DWORD PartyDamage::GetMaxHp(const GW::AgentLiving* agent)
{
    if (!agent) return 0;
    if (agent->max_hp > 0 && agent->max_hp < 100000) return agent->max_hp;
    const auto& map = GW::PartyMgr::GetIsPartyInHardMode() ? hp_map_hm : hp_map_nm;
    const auto it = map.find(agent->player_number);
    return it != map.end() ? it->second : 0;
}

void PartyDamage::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Hide in outpost", &settings.hide_in_outpost);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Print Player Damage by Ctrl + Click", &settings.print_by_click);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("Bars towards the left", &settings.bars_left, "If unchecked, they will expand to the right");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show damage", &settings.show_damage);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show healing", &settings.show_healing);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show DPS", &settings.show_dps);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Condition DPS", &settings.show_condition_dps);

    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("Show on top of health bars", &settings.overlay_party_window, "Untick to show this widget to the left (or right) of the party window.\nTick to show this widget over the top of the party health bars inside the party window");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("Party window offset", &settings.user_offset);
    ImGui::PopItemWidth();
    ImGui::ShowHelp("Distance away from the party window");

    ImGui::DragFloat("Width", &settings.width, 1.0f, 50.0f, 0.0f, "%.0f");
    if (settings.width <= 0) {
        settings.width = 1.0f;
    }
    ImGui::DragInt("Timeout", &settings.recent_max_time, 10.0f, 1000, 10 * 1000, "%d milliseconds");
    if (settings.recent_max_time < 0) {
        settings.recent_max_time = 0;
    }
    ImGui::ShowHelp("After this amount of time, each player's recent damage/healing bars will be reset");
    Colors::DrawSettingHueWheel("Background", &settings.color_background.value);
    Colors::DrawSettingHueWheel("Damage", &settings.color_damage.value);
    Colors::DrawSettingHueWheel("Recent Damage", &settings.color_recent.value);
    Colors::DrawSettingHueWheel("Healing", &settings.color_healing.value);
}
