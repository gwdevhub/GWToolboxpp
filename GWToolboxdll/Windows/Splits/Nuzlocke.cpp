#include "stdafx.h"

#include <Windows/Splits/NuzlockeState.h>
#include <Windows/Splits/GoalList.h>

#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

namespace {
    constexpr ImVec4 kNuzlockeAlive     = ImVec4(1.f, 1.f, 1.f, 1.f);
    constexpr ImVec4 kNuzlockeAvailable = ImVec4(0.35f, 1.f, 0.35f, 1.f);
    constexpr ImVec4 kNuzlockeDead      = ImVec4(1.f, 0.35f, 0.35f, 1.f);

    // Henchman names come from the game as "Name [Role Henchman]" — the icon conveys profession now, so the bracket is just noise.
    std::wstring StripHenchBracket(const std::wstring& name)
    {
        std::wstring out = name;
        if (const auto bracket = out.find(L'['); bracket != std::wstring::npos) {
            out.erase(bracket);
            while (!out.empty() && out.back() == L' ') out.pop_back();
        }
        return out;
    }

    // hero_info only lists heroes this account owns — without this check a partymate's same-named hero dying would increment our own death count. It also carries profession directly since AgentLiving::primary isn't reliably populated yet when PartyHeroAdd first fires (why hero icons were coming up blank).
    const GW::HeroInfo* FindOwnedHeroInfo(const GW::Constants::HeroID hero_id)
    {
        const auto* world = GW::GetWorldContext();
        if (!world) return nullptr;
        for (const auto& hi : world->hero_info) {
            if (hi.hero_id == hero_id) return &hi;
        }
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Nuzlocke: Death Rules (party death tracking)
// ---------------------------------------------------------------------------
void NuzlockeState::OnInstanceLoad()
{
    dead_agents.clear();
    // agent_ids for hireable town henchmen aren't stable across instances — drop cached decodes from the previous outpost so a reused id can't show a stale name.
    city_hench_names.clear();
    // Same reason: a reused agent_id must not be mistaken for "same roster as last frame, already resolved" by Update()'s skip check.
    last_town_hench_ids.clear();
    town_hench_all_resolved = false;
    // Pre-seed self so they show up in the roster at full lives, same as heroes/henchmen — other players only appear once they've actually died.
    if (const wchar_t* self_name = GW::PlayerMgr::GetPlayerName()) {
        players.try_emplace(self_name, NuzlockeMember{self_name, 0});
    }
}

void NuzlockeState::ResetProgress()
{
    heroes.clear();
    henches.clear();
    players.clear();
    dead_agents.clear();
    // Forces a town rescan, since the reseed above only covers current party members, not hireable-but-unrecruited henchmen.
    last_town_hench_ids.clear();
    town_hench_all_resolved = false;

    // agents is deliberately left alone so it can reseed the display rosters below immediately instead of waiting on the next zone transition's Party*Add events.
    for (const auto& [agent_id, identity] : agents) {
        if (identity.is_hero) {
            const auto [it, inserted] = heroes.try_emplace(identity.hero_id);
            if (inserted) {
                if (const auto* hero_info = FindOwnedHeroInfo(identity.hero_id))
                    it->second.profession = hero_info->primary;
            }
        } else if (!identity.hench_name.empty()) {
            henches.try_emplace(identity.hench_name,
                NuzlockeMember{identity.hench_name, 0, identity.hench_profession});
        }
    }

    if (const wchar_t* self_name = GW::PlayerMgr::GetPlayerName()) {
        players.try_emplace(self_name, NuzlockeMember{self_name, 0});
    }
}

std::wstring NuzlockeState::HenchKey(const std::wstring& raw_name) const
{
    return merge_hench_by_name ? StripHenchBracket(raw_name) : raw_name;
}

void NuzlockeState::Update(const bool last_was_explorable)
{
    // Heroes/henchmen roster — replaces the old PartyHero/HenchmanAdd/Remove StoC hooks with a live diff against GetPartyInfo().
    if (const auto* party = GW::PartyMgr::GetPartyInfo()) {
        std::unordered_set<uint32_t> live_agents;
        for (const auto& h : party->heroes) {
            live_agents.insert(h.agent_id);
            if (agents.contains(h.agent_id)) continue;
            const auto* hero_info = FindOwnedHeroInfo(h.hero_id);
            if (!hero_info) continue; // not ours — don't track/conflate partymates' heroes
            agents[h.agent_id] = NuzlockeIdentity{true, h.hero_id, {}};
            const auto [it, inserted] = heroes.try_emplace(h.hero_id); // first-seen only; leaves existing death count alone
            if (inserted) it->second.profession = hero_info->primary;
        }
        // Henchmen carry no owner field — they're party-wide slots controlled by whoever's leader, so "not ours" means "I'm not the leader," not a per-unit check.
        if (GW::PartyMgr::GetIsLeader()) {
            for (const auto& hm : party->henchmen) {
                live_agents.insert(hm.agent_id);
                if (agents.contains(hm.agent_id)) continue;
                pending_hench_names.emplace_back(
                    hm.agent_id, std::make_unique<GuiUtils::EncString>(GW::Agents::GetAgentEncName(hm.agent_id)));
                agents[hm.agent_id].hench_profession = static_cast<GW::Constants::Profession>(hm.profession);
            }
        }
        // Anyone we were tracking who's no longer in the live roster just left the party.
        std::erase_if(agents, [&](const auto& kv) {
            if (live_agents.contains(kv.first)) return false;
            std::erase_if(pending_hench_names, [&](const auto& p) { return p.first == kv.first; });
            return true;
        });
    }

    if (!pending_hench_names.empty()) {
        std::erase_if(pending_hench_names, [this](auto& p) {
            auto& [agent_id, enc] = p;
            const std::wstring raw_name = enc->wstring();
            if (raw_name.empty()) return false; // not decoded yet

            const std::wstring key = HenchKey(raw_name);
            auto& identity = agents[agent_id];
            identity.is_hero    = false;
            identity.hench_name = key;
            henches.try_emplace(key, NuzlockeMember{key, 0, identity.hench_profession});
            return true;
        });
    }

    // Hireable roster is a town-only concept
    if (last_was_explorable) {
        // Deaths only count in explorables — polls GetIsDead()
        for (const auto& [agent_id, identity] : agents) {
            if (dead_agents.contains(agent_id)) continue;
            const auto* agent  = GW::Agents::GetAgentByID(agent_id);
            const auto* living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living || !living->GetIsDead()) continue;
            dead_agents.insert(agent_id);
            if (identity.is_hero) {
                heroes[identity.hero_id].deaths++;
            } else {
                const auto hit = henches.find(identity.hench_name);
                if (hit != henches.end()) hit->second.deaths++;
            }
        }
        // Self/other players
        auto poll_player_death = [this](const uint32_t agent_id, const bool is_self) {
            if (dead_agents.contains(agent_id)) return;
            const auto* agent  = GW::Agents::GetAgentByID(agent_id);
            const auto* living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living || !living->IsPlayer() || !living->GetIsDead()) return;
            dead_agents.insert(agent_id);
            const wchar_t* raw_name = is_self ? GW::PlayerMgr::GetPlayerName()
                                               : GW::PlayerMgr::GetPlayerName(living->login_number);
            if (!raw_name) return;
            const std::wstring name(raw_name);
            players.try_emplace(name, NuzlockeMember{name, 0}).first->second.deaths++;
        };
        const uint32_t self_id = GW::Agents::GetControlledCharacterId();
        poll_player_death(self_id, true);
        // Party members only — was a full agent-array scan (every agent in the instance), which also meant a stranger dying elsewhere in a shared explorable could get misattributed as an "other party member" death. Same login_number->agent_id resolution as PartyStatisticsWindow.cpp.
        if (const auto* party = GW::PartyMgr::GetPartyInfo()) {
            for (const auto& p : party->players) {
                const uint32_t agent_id = GW::Agents::GetAgentIdByLoginNumber(p.login_number);
                if (agent_id != 0 && agent_id != self_id) poll_player_death(agent_id, false);
            }
        }

        city_hench_available.clear();
        last_town_hench_ids.clear();
        town_hench_all_resolved = false;
        return;
    }
    const auto* world = GW::GetWorldContext();
    if (!world) return;

    // Keeps henchman name and icons up to date until fully resolved 
    const auto& ids = world->henchmen_agent_ids;
    if (town_hench_all_resolved &&
        std::equal(ids.begin(), ids.end(), last_town_hench_ids.begin(), last_town_hench_ids.end()))
        return;

    city_hench_available.clear();
    bool all_resolved = true;
    for (const uint32_t agent_id : ids) {
        auto& enc = city_hench_names[agent_id];
        if (!enc) enc = std::make_unique<GuiUtils::EncString>(GW::Agents::GetAgentEncName(agent_id));
        const std::wstring raw_name = enc->wstring();
        if (raw_name.empty()) { all_resolved = false; continue; } // not decoded yet this frame

        // Seed the roster just from being hireable here, not only from actually being hired — otherwise a henchman never brought into the party never shows up at all.
        const std::wstring key = HenchKey(raw_name);
        const auto it = henches.try_emplace(key, NuzlockeMember{key, 0}).first;
        // These NPCs aren't party members, so there's no PartyInfo entry to read profession from — keep retrying every tick until AgentLiving::primary resolves instead of locking in a blank icon.
        if (it->second.profession == GW::Constants::Profession::None) {
            if (const auto* agent = GW::Agents::GetAgentByID(agent_id)) {
                if (const auto* living = agent->GetAsAgentLiving())
                    it->second.profession = static_cast<GW::Constants::Profession>(living->primary);
            }
            if (it->second.profession == GW::Constants::Profession::None) all_resolved = false;
        }
        city_hench_available.insert(StripHenchBracket(raw_name));
    }
    last_town_hench_ids.assign(ids.begin(), ids.end());
    town_hench_all_resolved = all_resolved;
}

void NuzlockeState::Draw()
{
    // Enable/lives settings live in Settings > Splits; this is display-only. Points is a separate module whose total draws in the header clock row instead — both are Manual-profile only.
    if (!ImGui::CollapsingHeader("Death Rules")) return;

    if (heroes.empty() && henches.empty() && players.empty()) {
        ImGui::TextDisabled("Nobody tracked yet this session.");
        return;
    }

    ImGui::TextColored(kNuzlockeAlive, "White");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("alive");
    ImGui::SameLine(0, 12); ImGui::TextColored(kNuzlockeAvailable, "Green");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("henchman hireable here");
    ImGui::SameLine(0, 12); ImGui::TextColored(kNuzlockeDead, "Red");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("out of lives");

    // Players info/lives, centered above the Henchmen/Heroes table. Each label is built once and reused for both the width measurement and the draw, instead of formatting each name twice.
    if (!players.empty()) {
        struct PlayerLabel { std::string text; int remaining; };
        std::vector<PlayerLabel> labels;
        labels.reserve(players.size());
        const float sep_w = ImGui::CalcTextSize("    ").x;
        float textw = 0.f;
        char buf[96];
        for (auto& [name, member] : players) {
            const int remaining = player_lives - member.deaths;
            snprintf(buf, sizeof(buf), "%s %d/%d", TextUtils::WStringToString(name).c_str(),
                     remaining > 0 ? remaining : 0, player_lives);
            if (!labels.empty()) textw += sep_w;
            textw += ImGui::CalcTextSize(buf).x;
            labels.push_back({buf, remaining});
        }
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (avail - textw) * 0.5f));

        bool first = true;
        for (const auto& lbl : labels) {
            if (!first) {
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("    ");
                ImGui::SameLine(0, 0);
            }
            first = false;
            ImGui::TextColored(lbl.remaining <= 0 ? kNuzlockeDead : kNuzlockeAlive, "%s", lbl.text.c_str());
        }
    }

    auto icon_size = ImGui::CalcTextSize(" ");
    icon_size.x = icon_size.y;

    if (ImGui::BeginTable("nuzlocke_hench_hero_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Henchmen");
        ImGui::TableSetupColumn("Heroes");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        for (auto& [name, member] : henches) {
            const int remaining = hench_lives - member.deaths;
            const std::wstring display_name = StripHenchBracket(name);

            ImVec4 color = kNuzlockeAlive;
            if (remaining <= 0) color = kNuzlockeDead;
            else if (city_hench_available.contains(display_name)) color = kNuzlockeAvailable;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", TextUtils::WStringToString(display_name).c_str(),
                remaining > 0 ? remaining : 0, hench_lives);
        }

        ImGui::TableSetColumnIndex(1);
        for (auto& [hero_id, member] : heroes) {
            auto* name = Resources::GetHeroName(hero_id);
            const int remaining = hero_lives - member.deaths;
            // Only owned heroes ever make it into heroes (see FindOwnedHeroInfo), so there's no "not yours" case left to color here.
            const ImVec4 color = remaining <= 0 ? kNuzlockeDead : kNuzlockeAlive;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", name ? name->string().c_str() : "(hero)",
                remaining > 0 ? remaining : 0, hero_lives);
        }

        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Nuzlocke: Points
// ---------------------------------------------------------------------------
int NuzlockeState::TotalPoints(const GoalList& list) const
{
    using T = GoalTrigger::Type;
    int total = 0;
    for (const auto& g : list.goals) {
        if (g.is_header || g.status != GoalStatus::Completed) continue;
        switch (g.trigger.type) {
            case T::Manual:          total += goal_points.manual;       break;
            case T::MissionComplete:
            case T::MissionBonus:    total += goal_points.missions;     break;
            case T::MapEnter:        total += goal_points.explorables;  break;
            case T::EnterExplorable:
            case T::ExitExplorable:
            case T::ExitOutpost:     total += goal_points.towns;        break;
            case T::ReachTitleRank:  total += goal_points.titles;       break;
            case T::ReachLevel:      total += goal_points.reach_level;  break;
            case T::QuestPickup:
            case T::QuestComplete:   total += goal_points.quest;        break;
            case T::SkillLearnt:     total += goal_points.skill_learnt; break;
            default:                 break; // preset-only triggers (dungeons/elites) aren't scored
        }
    }
    return total;
}
