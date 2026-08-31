#include "stdafx.h"

#include <Modules/QuestProgressLive.h>
#include <Modules/QuestMissionSnapshot.h>
#include <Modules/QuestCharacterJourney.h>
#include <Modules/QuestObservationService.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/MapMgr.h>

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
    std::optional<bool> pre;

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
            pre = GW::Map::IsPreSearing(avail->map_id());
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
        pre);
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
        const auto& title = world->titles[i];
        if (title.current_points == 0 && title.current_title_tier_index == 0) {
            continue;
        }
        TitleSnapshotInput row;
        row.title_id = static_cast<uint32_t>(i);
        row.tier_index = title.current_title_tier_index;
        row.current_points = title.current_points;
        inputs.push_back(row);
    }

    const auto level = world->level > 0 ? world->level : 0u;
    return MergeJourneySnapshot(
        previous_titles,
        previous_level,
        existing_events,
        inputs,
        level,
        observed_at);
}

} // namespace QuestProgress
