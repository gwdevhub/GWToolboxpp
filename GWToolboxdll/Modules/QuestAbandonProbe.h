#pragma once

// Bounded abandon re-observation probe (Batch 2C.1). Offline-testable; no GWCA / I/O.
// Keeps requesting quest-log snapshots after kSendAbandonQuest until the quest is absent
// from a valid world-ready snapshot, or the schedule times out — never fabricates removal.

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace QuestProgress {

inline constexpr std::chrono::milliseconds kAbandonProbeOffsets[] = {
    std::chrono::milliseconds(100),
    std::chrono::milliseconds(250),
    std::chrono::milliseconds(500),
    std::chrono::seconds(1),
    std::chrono::seconds(2),
    std::chrono::seconds(4),
};
inline constexpr size_t kAbandonProbeAttemptCount =
    sizeof(kAbandonProbeOffsets) / sizeof(kAbandonProbeOffsets[0]);

struct AbandonProbeState {
    uint32_t quest_id = 0;
    std::chrono::steady_clock::time_point started_at{};
    size_t next_offset_index = 0;
    bool timed_out = false;
};

class QuestAbandonProbeTracker {
public:
    // Schedule (or refresh start of) a probe for quest_id. Dedupes by quest id.
    // Returns true if a new probe was created.
    bool OnAbandon(uint32_t quest_id, std::chrono::steady_clock::time_point now);

    // While world_ready: when now reaches the next schedule offset, request another snapshot.
    // While !world_ready: suspend (do not advance offsets; do not fabricate absence).
    // Returns true if quest_log should be marked dirty for a re-snapshot.
    bool Tick(std::chrono::steady_clock::time_point now, bool world_ready);

    // After a valid world-ready quest-log snapshot: stop probes whose quest ids are absent.
    // Does nothing useful for loading/invalid snapshots (caller must not pass those as present).
    // Appends bounded diagnostics for newly timed-out probes (still present after last offset).
    void OnWorldReadyQuestLog(
        std::chrono::steady_clock::time_point now,
        const std::unordered_set<uint32_t>& present_quest_ids,
        std::vector<std::string>* diagnostics = nullptr);

    void Clear();
    size_t size() const { return probes_.size(); }
    bool Contains(uint32_t quest_id) const { return probes_.contains(quest_id); }
    const std::unordered_map<uint32_t, AbandonProbeState>& probes() const { return probes_; }

private:
    std::unordered_map<uint32_t, AbandonProbeState> probes_;
};

} // namespace QuestProgress
