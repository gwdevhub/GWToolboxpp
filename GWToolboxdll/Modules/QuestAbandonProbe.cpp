#include <Modules/QuestAbandonProbe.h>

namespace QuestProgress {

bool QuestAbandonProbeTracker::OnAbandon(uint32_t quest_id, std::chrono::steady_clock::time_point now)
{
    if (quest_id == 0) {
        return false;
    }
    auto [it, inserted] = probes_.try_emplace(quest_id);
    if (!inserted) {
        // Deduplicate: keep the existing schedule; do not reset (avoids unbounded growth /
        // pairing-window extension spam on repeated abandon clicks).
        return false;
    }
    it->second.quest_id = quest_id;
    it->second.started_at = now;
    it->second.next_offset_index = 0;
    it->second.timed_out = false;
    return true;
}

bool QuestAbandonProbeTracker::Tick(std::chrono::steady_clock::time_point now, bool world_ready)
{
    if (probes_.empty()) {
        return false;
    }
    if (!world_ready) {
        // Suspend: do not advance offsets or request refresh while loading/unready.
        return false;
    }

    bool request_refresh = false;
    for (auto& [quest_id, probe] : probes_) {
        (void)quest_id;
        if (probe.timed_out || probe.next_offset_index >= kAbandonProbeAttemptCount) {
            continue;
        }
        const auto due = probe.started_at + kAbandonProbeOffsets[probe.next_offset_index];
        if (now < due) {
            continue;
        }
        ++probe.next_offset_index;
        request_refresh = true;
    }
    return request_refresh;
}

void QuestAbandonProbeTracker::OnWorldReadyQuestLog(
    std::chrono::steady_clock::time_point now,
    const std::unordered_set<uint32_t>& present_quest_ids,
    std::vector<std::string>* diagnostics)
{
    for (auto it = probes_.begin(); it != probes_.end();) {
        auto& probe = it->second;
        if (!present_quest_ids.contains(probe.quest_id)) {
            // Confirmed absence — stop probe immediately.
            it = probes_.erase(it);
            continue;
        }

        // Still present. Time out only after the last scheduled attempt has fired and elapsed.
        if (probe.next_offset_index >= kAbandonProbeAttemptCount) {
            const auto last_due =
                probe.started_at + kAbandonProbeOffsets[kAbandonProbeAttemptCount - 1];
            if (now >= last_due) {
                if (diagnostics && !probe.timed_out) {
                    diagnostics->push_back(
                        "abandon probe timed out; quest still present in log (id="
                        + std::to_string(probe.quest_id) + ")");
                }
                probe.timed_out = true;
                it = probes_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void QuestAbandonProbeTracker::Clear()
{
    probes_.clear();
}

} // namespace QuestProgress
