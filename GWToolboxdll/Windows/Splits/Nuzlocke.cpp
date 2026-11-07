#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <ImGuiAddons.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/NuzlockeState.h>

NuzlockeState::NuzlockeState()  = default;
NuzlockeState::~NuzlockeState() = default;

namespace {
    constexpr ImVec4 kNuzlockeAlive     = ImVec4(1.f, 1.f, 1.f, 1.f);
    constexpr ImVec4 kNuzlockeAvailable = ImVec4(0.35f, 1.f, 0.35f, 1.f);
    constexpr ImVec4 kNuzlockeDead      = ImVec4(1.f, 0.35f, 0.35f, 1.f);

    std::wstring StripHenchBracket(const std::wstring& name)
    {
        std::wstring out = name;
        if (const auto bracket = out.find(L'['); bracket != std::wstring::npos) {
            out.erase(bracket);
            while (!out.empty() && out.back() == L' ') out.pop_back();
        }
        return out;
    }
}

void NuzlockeState::OnInstanceLoad()
{
    dead_agents.clear();
    // Town hench agent_ids not stable across instances.
    city_hench_names.clear();
    last_town_hench_ids.clear();
    town_hench_all_resolved = false;
    // Pre-seed self at full lives. Others show only after death.
    if (const wchar_t* self_name = GW::PlayerMgr::GetPlayerName()) {
        players.try_emplace(self_name, NuzlockeMember{self_name, 0});
    }
}

void NuzlockeState::OnPartyResigned()
{
    for (const auto& [agent_id, identity] : agents) dead_agents.insert(agent_id);
    if (const uint32_t self_id = GW::Agents::GetControlledCharacterId(); self_id != 0)
        dead_agents.insert(self_id);
    if (const auto* party = GW::PartyMgr::GetPartyInfo()) {
        for (const auto& p : party->players) {
            const uint32_t agent_id = GW::Agents::GetAgentIdByLoginNumber(p.login_number);
            if (agent_id != 0) dead_agents.insert(agent_id);
        }
    }
}

void NuzlockeState::ResetProgress()
{
    heroes.clear();
    henches.clear();
    players.clear();
    dead_agents.clear();
    // Force town rescan: reseed only cover party.
    last_town_hench_ids.clear();
    town_hench_all_resolved = false;

    for (const auto& [agent_id, identity] : agents) {
        if (identity.is_hero) {
            const auto [it, inserted] = heroes.try_emplace(identity.hero_id);
            if (inserted) {
                if (const auto* hero_info = GW::PartyMgr::GetHeroInfo(identity.hero_id))
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
    return settings.merge_hench_by_name ? StripHenchBracket(raw_name) : raw_name;
}

void NuzlockeState::Update(const bool last_was_explorable)
{
    if (const auto* party = GW::PartyMgr::GetPartyInfo()) {
        auto& live_agents = live_agents_scratch;
        live_agents.clear();
        for (const auto& h : party->heroes) {
            live_agents.insert(h.agent_id);
            if (agents.contains(h.agent_id)) continue;
            // Account's own heroes only, else a partymate's copy of the hero costs our lives. Profession here: AgentLiving::primary not ready at PartyHeroAdd.
            const auto* hero_info = GW::PartyMgr::GetHeroInfo(h.hero_id);
            if (!hero_info) continue;
            agents[h.agent_id] = NuzlockeIdentity{true, h.hero_id, {}};
            const auto [it, inserted] = heroes.try_emplace(h.hero_id);
            if (inserted) it->second.profession = hero_info->primary;
        }
        // Henches no owner: party slots, leader controls.
        if (GW::PartyMgr::GetIsLeader()) {
            for (const auto& hm : party->henchmen) {
                live_agents.insert(hm.agent_id);
                if (agents.contains(hm.agent_id)) continue;
                pending_hench_names.emplace_back(
                    hm.agent_id, std::make_unique<GuiUtils::EncString>(GW::Agents::GetAgentEncName(hm.agent_id)));
                agents[hm.agent_id].hench_profession = static_cast<GW::Constants::Profession>(hm.profession);
            }
        }
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
            if (raw_name.empty()) return false;

            const std::wstring key = HenchKey(raw_name);
            auto& identity = agents[agent_id];
            identity.is_hero    = false;
            identity.hench_name = key;
            henches.try_emplace(key, NuzlockeMember{key, 0, identity.hench_profession});
            return true;
        });
    }

    if (last_was_explorable) {
        for (const auto& [agent_id, identity] : agents) {
            const auto* agent  = GW::Agents::GetAgentByID(agent_id);
            const auto* living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living) continue;
            if (!living->GetIsDead()) {
                dead_agents.erase(agent_id); // rezzed: next death count
                continue;
            }
            if (dead_agents.contains(agent_id)) continue;
            dead_agents.insert(agent_id);
            if (identity.is_hero) {
                heroes[identity.hero_id].deaths++;
            } else {
                const auto hit = henches.find(identity.hench_name);
                if (hit != henches.end()) hit->second.deaths++;
            }
        }
        auto poll_player_death = [this](const uint32_t agent_id, const bool is_self) {
            const auto* agent  = GW::Agents::GetAgentByID(agent_id);
            const auto* living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living || !living->IsPlayer()) return;
            if (!living->GetIsDead()) {
                dead_agents.erase(agent_id); // rezzed: next death count
                return;
            }
            if (dead_agents.contains(agent_id)) return;
            dead_agents.insert(agent_id);
            const wchar_t* raw_name = is_self ? GW::PlayerMgr::GetPlayerName()
                                               : GW::PlayerMgr::GetPlayerName(living->login_number);
            if (!raw_name) return;
            const std::wstring name(raw_name);
            players.try_emplace(name, NuzlockeMember{name, 0}).first->second.deaths++;
        };
        const uint32_t self_id = GW::Agents::GetControlledCharacterId();
        poll_player_death(self_id, true);
        // Party only: full agent scan count strangers.
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
        if (raw_name.empty()) { all_resolved = false; continue; }

        // Seed from hireable too, else never-hired hench never show.
        const std::wstring key = HenchKey(raw_name);
        const auto it = henches.try_emplace(key, NuzlockeMember{key, 0}).first;
        // Not party members: no PartyInfo. Retry till primary set.
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
    if (!ImGui::CollapsingHeader("Death Tracker")) return;

    if (heroes.empty() && henches.empty() && players.empty()) {
        ImGui::TextDisabled("Nobody tracked yet this session.");
        return;
    }

    ImGui::TextColored(kNuzlockeAlive, "White");
    ImGui::SameLine(0, 4.f * ImGui::FontScale()); ImGui::TextDisabled("alive");
    ImGui::SameLine(0, 12.f * ImGui::FontScale()); ImGui::TextColored(kNuzlockeAvailable, "Green");
    ImGui::SameLine(0, 4.f * ImGui::FontScale()); ImGui::TextDisabled("henchman hireable here");
    ImGui::SameLine(0, 12.f * ImGui::FontScale()); ImGui::TextColored(kNuzlockeDead, "Red");
    ImGui::SameLine(0, 4.f * ImGui::FontScale()); ImGui::TextDisabled("out of lives");

    if (!players.empty()) {
        struct PlayerLabel { std::string text; int remaining; };
        std::vector<PlayerLabel> labels;
        labels.reserve(players.size());
        const float sep_w = ImGui::CalcTextSize("    ").x;
        float textw = 0.f;
        char buf[96];
        for (auto& [name, member] : players) {
            const int remaining = settings.player_lives - member.deaths;
            snprintf(buf, sizeof(buf), "%s %d/%d", TextUtils::WStringToString(name).c_str(),
                     remaining > 0 ? remaining : 0, settings.player_lives);
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
            const int remaining = settings.hench_lives - member.deaths;
            const std::wstring display_name = StripHenchBracket(name);

            ImVec4 color = kNuzlockeAlive;
            if (remaining <= 0) color = kNuzlockeDead;
            else if (city_hench_available.contains(display_name)) color = kNuzlockeAvailable;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", TextUtils::WStringToString(display_name).c_str(),
                remaining > 0 ? remaining : 0, settings.hench_lives);
        }

        ImGui::TableSetColumnIndex(1);
        for (auto& [hero_id, member] : heroes) {
            auto* name = Resources::GetHeroName(hero_id);
            const int remaining = settings.hero_lives - member.deaths;
            const ImVec4 color = remaining <= 0 ? kNuzlockeDead : kNuzlockeAlive;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", name ? name->string().c_str() : "(hero)",
                remaining > 0 ? remaining : 0, settings.hero_lives);
        }

        ImGui::EndTable();
    }
}

int NuzlockeState::TotalPoints(const GoalList& list) const
{
    using T = GoalTrigger::Type;
    int total = 0;
    for (const auto& g : list.goals) {
        if (g.is_header || g.status != GoalStatus::Completed) continue;
        switch (g.trigger.type) {
            case T::Manual:          total += settings.points_manual;       break;
            case T::MissionComplete:
            case T::MissionBonus:    total += settings.points_missions;     break;
            case T::MapEnter:
            case T::EnterExplorable:
            case T::ExitExplorable:
            case T::VanquishComplete: total += settings.points_explorables; break;
            case T::EnterOutpost:
            case T::ExitOutpost:     total += settings.points_towns;        break;
            case T::ReachTitleRank:  total += settings.points_titles;       break;
            case T::ReachLevel:      total += settings.points_reach_level;  break;
            case T::QuestPickup:
            case T::QuestComplete:   total += settings.points_quest;        break;
            case T::SkillLearnt:     total += settings.points_skill_learnt; break;
            default:                 break;
        }
    }
    return total;
}
