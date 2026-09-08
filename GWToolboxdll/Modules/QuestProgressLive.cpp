#include "stdafx.h"

#include <Modules/QuestProgressLive.h>
#include <Modules/QuestMissionSnapshot.h>
#include <Modules/QuestCharacterJourney.h>
#include <Modules/QuestObservationService.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/AccountContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

namespace QuestProgress {
namespace {

UuidWords WordsFromUint32(const uint32_t words[4])
{
    return UuidWords{words[0], words[1], words[2], words[3]};
}

std::u16string WStringToU16(const std::wstring& w)
{
    return std::u16string(w.begin(), w.end());
}

uint32_t ReadPlayerLevel(const GW::WorldContext& world)
{
    // Avoid PCH macro collisions on the field name `level`.
    return world.level_dupe > 0u ? world.level_dupe : 0u;
}

} // namespace

SessionIdentity SampleLiveSessionIdentity(bool world_ready)
{
    if (!world_ready) {
        return {};
    }

    const auto account_guid = GW::AccountMgr::GetAccountUuid();
    const auto account_str = TextUtils::GuidToString(&account_guid);

    UuidWords character_words{};
    std::string display;
    std::string profession;
    std::string secondary_profession;
    std::optional<bool> pre;
    std::optional<bool> pvp;

    if (const auto* ctx = GW::GetCharContext()) {
        character_words = WordsFromUint32(ctx->player_uuid);
        display = TextUtils::WStringToString(ctx->player_name);
    }
    else if (const auto* name = GW::AccountMgr::GetCurrentPlayerName()) {
        display = TextUtils::WStringToString(name);
    }

    if (!display.empty()) {
        if (const auto* avail = GW::AccountMgr::GetAvailableCharacter(TextUtils::StringToWString(display).c_str())) {
            if (IsZeroUuidWords(character_words)) {
                character_words = WordsFromUint32(avail->uuid);
            }
            profession = std::to_string(static_cast<uint32_t>(avail->primary()));
            const auto secondary = avail->secondary();
            if (secondary != GW::Constants::Profession::None) {
                secondary_profession = std::to_string(static_cast<uint32_t>(secondary));
            }
            pre = GW::Map::IsPreSearing(avail->map_id());
            pvp = avail->is_pvp();
        }
    }
    if (!pre.has_value()) {
        pre = GW::Map::IsPreSearing();
    }

    return MakeSessionIdentity(
        account_str,
        FormatUuidWords(character_words),
        display,
        profession,
        pre,
        secondary_profession,
        pvp);
}

QuestSnapshot ToQuestSnapshot(const LiveQuestView& view)
{
    QuestSnapshot snap;
    snap.revision = view.revision;
    snap.loading = view.loading;
    snap.world_ready = view.world_ready;
    snap.identity_captured = view.identity_captured;
    snap.account_key = view.account_key;
    snap.character_key = view.character_key;
    snap.selected_active_quest_id = static_cast<uint32_t>(view.active_quest_id);
    snap.quests.reserve(view.quests.size());
    for (const auto& q : view.quests) {
        QuestSnapshotQuest oq;
        oq.game_quest_id = static_cast<uint32_t>(q.quest_id);
        oq.in_log_completed = q.in_log_completed;
        oq.objectives_missing = q.objectives_missing;
        oq.objectives.reserve(q.objectives.size());
        for (size_t i = 0; i < q.objectives.size(); ++i) {
            ObjectiveObservation obj;
            obj.index = static_cast<uint32_t>(i);
            obj.completed = q.objectives[i].completed;
            obj.encoded_content = WStringToU16(q.objectives[i].encoded);
            oq.objectives.push_back(std::move(obj));
        }
        snap.quests.push_back(std::move(oq));
    }
    return snap;
}

std::map<uint32_t, MissionRecord> SampleLiveMissionCompletion(
    std::chrono::system_clock::time_point wall_now,
    const std::map<uint32_t, MissionRecord>* previous)
{
    if (!GW::Map::GetIsMapLoaded()) {
        return {};
    }
    const auto* game = GW::GetGameContext();
    const auto* world = game ? game->world : nullptr;
    if (!world) {
        return {};
    }

    const MissionBitsetWords completed_normal{
        world->missions_completed.m_buffer,
        world->missions_completed.m_size,
    };
    const MissionBitsetWords bonus_normal{
        world->missions_bonus.m_buffer,
        world->missions_bonus.m_size,
    };
    const MissionBitsetWords completed_hard{
        world->missions_completed_hm.m_buffer,
        world->missions_completed_hm.m_size,
    };
    const MissionBitsetWords bonus_hard{
        world->missions_bonus_hm.m_buffer,
        world->missions_bonus_hm.m_size,
    };

    const auto observed_at = FormatCanonicalUtc(wall_now);
    if (!IsCanonicalUtcTimestamp(observed_at)) {
        return {};
    }
    return BuildMissionRecordsFromBitsets(
        completed_normal,
        bonus_normal,
        completed_hard,
        bonus_hard,
        observed_at,
        previous);
}

JourneySnapshotResult SampleLiveJourneySnapshot(
    std::chrono::system_clock::time_point wall_now,
    const std::map<uint32_t, TitleStateRecord>& previous_titles,
    std::optional<uint32_t> previous_level,
    std::optional<uint32_t> previous_map_id,
    const std::vector<JourneyEventRecord>& existing_events)
{
    if (!GW::Map::GetIsMapLoaded()) {
        return {};
    }
    const auto* game = GW::GetGameContext();
    const auto* world = game ? game->world : nullptr;
    if (!world) {
        return {};
    }

    const auto observed_at = FormatCanonicalUtc(wall_now);
    if (!IsCanonicalUtcTimestamp(observed_at)) {
        return {};
    }

    std::vector<TitleSnapshotInput> inputs;
    inputs.reserve(world->titles.size());
    for (size_t i = 0; i < world->titles.size(); ++i) {
        const uint32_t tier_index = world->titles[i].current_title_tier_index;
        const uint32_t current_points = world->titles[i].current_points;
        if (current_points == 0 && tier_index == 0) {
            continue;
        }
        TitleSnapshotInput row;
        row.title_id = static_cast<uint32_t>(i);
        row.tier_index = tier_index;
        row.current_points = current_points;
        inputs.push_back(row);
    }

    auto out = MergeJourneySnapshot(
        previous_titles,
        previous_level,
        existing_events,
        inputs,
        ReadPlayerLevel(*world),
        observed_at);

    const auto current_map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    out.observed_map_id = current_map_id;
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildMapEnterEvents(previous_map_id, current_map_id, existing_events, observed_at));

    const MissionBitsetWords vanquished{
        world->vanquished_areas.m_buffer,
        world->vanquished_areas.m_size,
    };
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildVanquishAreaEvents(
            PriorIdsFromJourneyEvents(existing_events, "vanquish_area"),
            CollectSetBitMapIds(vanquished),
            existing_events,
            observed_at));

    const MissionBitsetWords unlocked_maps{
        world->unlocked_map.m_buffer,
        world->unlocked_map.m_size,
    };
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildNewlySeenIdEvents(
            "map_unlock",
            JourneyUnlockIdKind::Map,
            PriorIdsFromJourneyEvents(existing_events, "map_unlock"),
            CollectSetBitMapIds(unlocked_maps),
            existing_events,
            observed_at));

    const MissionBitsetWords unlocked_skills{
        world->unlocked_character_skills.m_buffer,
        world->unlocked_character_skills.m_size,
    };
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildNewlySeenIdEvents(
            "skill_unlock",
            JourneyUnlockIdKind::Skill,
            PriorIdsFromJourneyEvents(existing_events, "skill_unlock"),
            CollectSetBitMapIds(unlocked_skills),
            existing_events,
            observed_at));

    if (const auto* account = game->account) {
            std::vector<uint32_t> account_skill_ids;
            account_skill_ids.reserve(account->unlocked_account_skills.size());
            for (size_t i = 0; i < account->unlocked_account_skills.size(); ++i) {
                const auto skill_id = account->unlocked_account_skills[i];
                if (skill_id != 0) {
                    account_skill_ids.push_back(skill_id);
                }
            }
            AppendUniqueJourneyEvents(
                out.new_events,
                BuildNewlySeenIdEvents(
                    "account_skill_unlock",
                    JourneyUnlockIdKind::Skill,
                    PriorIdsFromJourneyEvents(existing_events, "account_skill_unlock"),
                    account_skill_ids,
                    existing_events,
                    observed_at));
        }

    std::vector<uint32_t> hero_ids;
    hero_ids.reserve(world->hero_info.size());
    for (size_t i = 0; i < world->hero_info.size(); ++i) {
        const auto hero_id = static_cast<uint32_t>(world->hero_info[i].hero_id);
        if (hero_id != 0) {
            hero_ids.push_back(hero_id);
        }
    }
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildNewlySeenIdEvents(
            "hero_unlock",
            JourneyUnlockIdKind::Hero,
            PriorIdsFromJourneyEvents(existing_events, "hero_unlock"),
            hero_ids,
            existing_events,
            observed_at));

    AppendUniqueJourneyEvents(
        out.new_events,
        BuildHardModeUnlockEvents(
            HasJourneyKind(existing_events, "hard_mode_unlock"),
            world->is_hard_mode_unlocked != 0,
            existing_events,
            observed_at));

    if (const auto* player = GW::PlayerMgr::GetPlayerByID()) {
        const GW::ProfessionState* found = nullptr;
        for (size_t i = 0; i < world->party_profession_states.size(); ++i) {
            if (world->party_profession_states[i].agent_id == player->agent_id) {
                found = &world->party_profession_states[i];
                break;
            }
        }
        if (found) {
            std::vector<uint32_t> profession_ids;
            for (uint32_t prof = 1; prof <= 10; ++prof) {
                if ((found->unlocked_professions >> prof & 1u) != 0) {
                    profession_ids.push_back(prof);
                }
            }
            AppendUniqueJourneyEvents(
                out.new_events,
                BuildNewlySeenIdEvents(
                    "profession_unlock",
                    JourneyUnlockIdKind::Profession,
                    PriorIdsFromJourneyEvents(existing_events, "profession_unlock"),
                    profession_ids,
                    existing_events,
                    observed_at));
        }
    }

    const auto* carto_bits = reinterpret_cast<const uint32_t*>(world->cartographed_areas.m_buffer);
    const auto carto_pct = ComputeCartographyCoveragePercent(
        carto_bits,
        world->cartographed_areas.size(),
        world->h05B4[0],
        world->h05B4[1]);
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildCartographyThresholdEvents(
            MaxCartographyPercentFromEvents(existing_events),
            carto_pct,
            current_map_id,
            existing_events,
            observed_at));

    static const std::vector<uint32_t> kSkillPointThresholds{
        1, 5, 10, 25, 50, 75, 100, 150, 200, 250, 300};
    AppendUniqueJourneyEvents(
        out.new_events,
        BuildAbsoluteThresholdEvents(
            "skill_point_threshold",
            "skill_points",
            MaxAmountFromJourneyEvents(existing_events, "skill_point_threshold", "skill_points"),
            world->total_earned_skill_points,
            kSkillPointThresholds,
            existing_events,
            observed_at));

    static const std::vector<uint32_t> kFactionThresholds{
        1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000, 1000000};
    const struct {
        const char* prefix;
        uint32_t value;
    } factions[] = {
        {"faction:kurzick", world->total_earned_kurzick},
        {"faction:luxon", world->total_earned_luxon},
        {"faction:balthazar", world->total_earned_balth},
        {"faction:imperial", world->total_earned_imperial},
    };
    for (const auto& faction : factions) {
        AppendUniqueJourneyEvents(
            out.new_events,
            BuildAbsoluteThresholdEvents(
                "faction_threshold",
                faction.prefix,
                MaxAmountFromJourneyEvents(existing_events, "faction_threshold", faction.prefix),
                faction.value,
                kFactionThresholds,
                existing_events,
                observed_at));
    }

    out.experience_total = world->experience;

    return out;
}

} // namespace QuestProgress
