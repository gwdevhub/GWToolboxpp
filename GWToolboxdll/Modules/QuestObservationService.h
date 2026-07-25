#pragma once

#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hook.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>

struct OwnedObjective {
    std::wstring encoded;
    bool completed = false;
};

struct OwnedQuestEntry {
    GW::Constants::QuestID quest_id = GW::Constants::QuestID::None;
    uint32_t log_state = 0;
    bool in_log_completed = false;
    std::wstring name_encoded;
    std::vector<OwnedObjective> objectives;
    bool objectives_missing = false;
};

struct OwnedMissionObjective {
    uint32_t objective_id = 0;
    std::wstring enc;
    uint32_t type = 0;
    bool bullet = false;
    bool completed = false;
};

struct LiveQuestView {
    uint64_t revision = 0;
    bool loading = false;
    bool world_ready = false;
    GW::Constants::QuestID active_quest_id = GW::Constants::QuestID::None;
    bool mission_mode = false;
    std::vector<OwnedQuestEntry> quests;
    std::vector<OwnedMissionObjective> mission_objectives;
};

// Game-thread observation helper; not a ToolboxModule. Publishes immutable snapshots for Draw.
class QuestObservationService {
public:
    void Initialize();
    void Update(float delta);
    void SignalTerminate();
    void Terminate();

    std::shared_ptr<const LiveQuestView> AcquireSnapshot() const;

private:
    static constexpr GW::Constants::QuestID custom_marker_quest_id =
        static_cast<GW::Constants::QuestID>(0x0000fdd);
    static constexpr auto request_cooldown = std::chrono::seconds(2);
    static constexpr int max_request_attempts = 3;
    static constexpr uint32_t OBJECTIVE_FLAG_BULLET = 0x1;
    static constexpr uint32_t OBJECTIVE_FLAG_COMPLETED = 0x2;

    void RegisterCallbacks();
    void UnregisterCallbacks();
    void MarkAllDirty();
    void ResetRequestAttemptCycle();
    void Publish(std::shared_ptr<const LiveQuestView> view);
    void PublishLoadingInvalid();
    bool IsWorldReady() const;
    LiveQuestView SnapshotAllChannels(uint64_t revision) const;
    void SnapshotQuestLog(LiveQuestView& view) const;
    void SnapshotActiveQuest(LiveQuestView& view) const;
    void SnapshotMissionObjectives(LiveQuestView& view) const;
    void SyncPendingRequestsFromSnapshot(const LiveQuestView& view);
    void ProcessPendingRequests();
    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam);

    static void ParseQuestObjectivesOwned(const wchar_t* objectives, std::vector<OwnedObjective>& out);
    static std::wstring CopyEnc(const wchar_t* enc);

    GW::HookEntry ui_hook_entry_{};
    bool callbacks_registered_ = false;
    bool terminated_ = false;

    bool quest_log_dirty_ = false;
    bool active_quest_dirty_ = false;
    bool mission_objectives_dirty_ = false;
    bool loading_transition_pending_ = false;
    bool published_loading_invalid_ = false;

    uint64_t next_revision_ = 1;

    mutable std::mutex snapshot_mutex_;
    std::shared_ptr<const LiveQuestView> published_;

    struct RequestState {
        std::chrono::steady_clock::time_point last_request{};
        int attempts = 0;
    };
    std::unordered_map<GW::Constants::QuestID, RequestState> request_state_;
};
