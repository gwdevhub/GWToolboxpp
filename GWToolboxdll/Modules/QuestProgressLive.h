#pragma once

// Live GWCA adapters for Batch 2C. Not linked into QuestProgressTests.

#include <Modules/QuestProgressService.h>
#include <Modules/QuestSessionIdentity.h>

struct LiveQuestView;

namespace QuestProgress {

// Sample account/character UUID + metadata when world context is available.
// Returns Unbound when not world-ready; Ephemeral when character UUID is zero.
SessionIdentity SampleLiveSessionIdentity(bool world_ready);

// Copy LiveQuestView into owned reducer snapshot (no borrowed GWCA pointers).
QuestSnapshot ToQuestSnapshot(const LiveQuestView& view);

// Sample mission/bonus completion bitsets when world context is available.
std::map<uint32_t, MissionRecord> SampleLiveMissionCompletion(
    std::chrono::system_clock::time_point wall_now,
    const std::map<uint32_t, MissionRecord>* previous = nullptr);

// Sample title tiers + character level for journey milestones.
JourneySnapshotResult SampleLiveJourneySnapshot(
    std::chrono::system_clock::time_point wall_now,
    const std::map<uint32_t, TitleStateRecord>& previous_titles,
    std::optional<uint32_t> previous_level,
    std::optional<uint32_t> previous_map_id,
    const std::vector<JourneyEventRecord>& existing_events);

} // namespace QuestProgress
