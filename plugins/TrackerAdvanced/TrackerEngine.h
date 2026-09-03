#pragma once

#include "TrackerGame.h"
#include "TrackerModel.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace TrackerAdvanced {
    enum class RunState : uint8_t {
        ready,
        running,
        paused,
        completed,
    };

    enum class RunMode : uint8_t {
        live,
        test,
    };

    enum class RuntimeTaskStatus : uint8_t {
        pending,
        active,
        completed,
        skipped,
    };

    struct AutomationSettings {
        std::wstring character_name;
        bool auto_start = false;
        bool auto_resume = true;
    };

    struct ControlResult {
        bool succeeded = false;
        std::string message;

        explicit operator bool() const noexcept { return succeeded; }
    };

    struct RuntimeTask {
        RuntimeTaskStatus status = RuntimeTaskStatus::pending;
        std::optional<uint64_t> split_ms;
        std::optional<uint64_t> segment_ms;
        bool manually_split = false;
    };

    struct ExperienceRuntime {
        bool armed = false;
        uint64_t experience_gained = 0;
        uint32_t baseline_experience = 0;
        uint32_t runs = 0;
        uint64_t elapsed_ms = 0;
    };

    struct EngineNotification {
        enum class Type : uint8_t {
            info,
            warning,
            error,
            lifecycle,
            criterion,
            component,
        };

        Type type = Type::info;
        std::string text;
    };

    class TrackerEngine final {
    public:
        void SetProfile(ProfileDefinition profile);
        void ClearProfile();
        [[nodiscard]] bool HasProfile() const noexcept;
        [[nodiscard]] const ProfileDefinition* Profile() const noexcept;
        [[nodiscard]] const std::vector<FlatTaskView>& Tasks() const noexcept;
        [[nodiscard]] const std::vector<RuntimeTask>& TaskRuntime() const noexcept;

        void SetMode(RunMode mode, TrackerGame& game);
        [[nodiscard]] RunMode Mode() const noexcept;
        [[nodiscard]] RunState State() const noexcept;
        [[nodiscard]] PauseReason CurrentPauseReason() const noexcept;

        ControlResult Start(TrackerGame& game, const AutomationSettings& automation, bool automatic = false);
        ControlResult Pause(TrackerGame& game, PauseReason reason = PauseReason::manual);
        ControlResult Resume(
            TrackerGame& game,
            const AutomationSettings& automation,
            bool automatic = false,
            const PendingGameEvents* resume_events = nullptr);
        ControlResult Split(TrackerGame& game);
        ControlResult Skip(TrackerGame& game);
        ControlResult Undo(TrackerGame& game);
        void ResetFresh(TrackerGame& game);
        ControlResult EvaluateCurrentTask(TrackerGame& game);
        ControlResult EvaluateTaskForTest(size_t task_index, TrackerGame& game);
        ControlResult StartTaskTest(
            size_t task_index,
            TrackerGame& game,
            const AutomationSettings& automation);

        void Update(TrackerGame& game, const AutomationSettings& automation);
        void RefreshHooks(TrackerGame& game, const AutomationSettings& automation);
        void DisarmHooks(TrackerGame& game);

        [[nodiscard]] bool AutoStartArmed() const noexcept;
        void ArmAutoStart() noexcept;
        [[nodiscard]] bool HasUnsavedRunData() const;
        [[nodiscard]] std::optional<HistoryAttempt> BuildResetAttempt() const;
        [[nodiscard]] std::optional<ActiveRunFile> BuildActiveRunFile() const;
        [[nodiscard]] ControlResult Restore(const ActiveRunFile& active_run, TrackerGame& game);

        [[nodiscard]] uint64_t ElapsedMs() const;
        [[nodiscard]] uint64_t LastResolvedElapsedMs() const noexcept;
        [[nodiscard]] size_t ActiveTaskIndex() const noexcept;
        [[nodiscard]] const FlatTaskView* ActiveTask() const noexcept;
        [[nodiscard]] const ExperienceRuntime* ActiveExperience() const noexcept;
        [[nodiscard]] uint64_t ActiveExperienceElapsedMs() const;
        [[nodiscard]] bool CheckpointRequested() noexcept;
        [[nodiscard]] bool ActiveRunRemovalRequested() noexcept;
        [[nodiscard]] std::optional<HistoryAttempt> TakeFinishedAttempt();
        [[nodiscard]] std::vector<EngineNotification> DrainNotifications();

        [[nodiscard]] static std::string FormatDuration(uint64_t milliseconds, bool tenths = true);
        [[nodiscard]] static std::string StateLabel(RunState state);
        [[nodiscard]] static std::string ModeLabel(RunMode mode);

    private:
        using Clock = std::chrono::steady_clock;

        struct ExperienceInternal {
            ExperienceRuntime persisted;
            std::optional<Clock::time_point> active_since;
            uint64_t armed_map_loaded_sequence = 0;
        };

        struct UndoSnapshot {
            size_t active_task_index = 0;
            uint64_t last_resolved_elapsed_ms = 0;
            std::vector<RuntimeTask> runtime;
            std::optional<ExperienceRuntime> experience;
        };

        struct DeferredCriterion {
            uint32_t task_id = 0;
            PendingCriterionEvent event;
        };

        [[nodiscard]] bool CharacterMatches(
            const TrackerGame& game, const AutomationSettings& automation) const;
        [[nodiscard]] bool CanEnterRunning(
            const TrackerGame& game, const AutomationSettings& automation, std::string& reason) const;
        void BeginAttempt(TrackerGame& game);
        void ActivateCurrentTask(
            TrackerGame& game,
            bool preserve_manual_undo = false,
            bool preserve_experience = false);
        void ProcessDeferredCriterion(TrackerGame& game);
        void ResolveCurrentTask(
            TrackerGame& game,
            TaskResult result,
            bool manual_split,
            bool preserve_undo = false);
        void FinishRun(TrackerGame& game);
        void InitializeExperience(TrackerGame& game);
        void ArmExperience(TrackerGame& game);
        void ReconcileExperience(
            TrackerGame& game,
            uint64_t fallback_experience = 0);
        void UpdateExperience(TrackerGame& game, const PendingGameEvents& events);
        void PauseExperience();
        void ResumeExperience(TrackerGame& game);
        [[nodiscard]] uint64_t ExperienceElapsedMs() const;
        [[nodiscard]] TaskRecord MakeTaskRecord(size_t index) const;
        [[nodiscard]] HistoryAttempt MakeAttempt(AttemptResult result) const;
        void RequestCheckpoint();
        void Notify(EngineNotification::Type type, std::string text);
        [[nodiscard]] std::string NewAttemptId() const;
        [[nodiscard]] static std::string TimestampNow();
        [[nodiscard]] uint64_t PreviousSplitMs(size_t before_index) const;
        void ResetRuntime();

        std::optional<ProfileDefinition> profile_;
        std::vector<FlatTaskView> tasks_;
        std::vector<RuntimeTask> runtime_;
        size_t active_task_index_ = 0;

        RunState state_ = RunState::ready;
        RunMode mode_ = RunMode::live;
        PauseReason pause_reason_ = PauseReason::invalid;
        bool auto_start_armed_ = true;

        uint64_t elapsed_base_ms_ = 0;
        uint64_t last_resolved_elapsed_ms_ = 0;
        std::optional<Clock::time_point> running_since_;
        std::string attempt_id_;
        std::string character_name_;
        std::string started_at_;

        std::optional<ExperienceInternal> experience_;
        std::optional<UndoSnapshot> undo_;
        bool test_task_only_ = false;
        std::optional<DeferredCriterion> deferred_criterion_;
        std::optional<uint64_t> deferred_map_loaded_sequence_;

        bool checkpoint_requested_ = false;
        bool active_run_removal_requested_ = false;
        std::optional<HistoryAttempt> finished_attempt_;
        std::deque<EngineNotification> notifications_;
    };
}
