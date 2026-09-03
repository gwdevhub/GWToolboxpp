#include "TrackerEngine.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <ctime>
#include <format>
#include <limits>
#include <ranges>
#include <utility>

namespace TrackerAdvanced {
    namespace {
        constexpr size_t MaxNotifications = 500;

        std::string TaskLabel(const FlatTaskView* task)
        {
            return task && task->task ? task->task->name : "(no task)";
        }

        std::string WideToUtf8(const std::wstring_view value)
        {
            if (value.empty() || value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                return {};
            }
            const auto size = WideCharToMultiByte(
                CP_UTF8,
                0,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (size <= 0) {
                return {};
            }
            std::string result(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                value.data(),
                static_cast<int>(value.size()),
                result.data(),
                size,
                nullptr,
                nullptr);
            return result;
        }
    }

    void TrackerEngine::SetProfile(ProfileDefinition profile)
    {
        profile_ = std::move(profile);
        tasks_ = FlattenTasks(*profile_);
        ResetRuntime();
        auto_start_armed_ = true;
        Notify(EngineNotification::Type::lifecycle, "Loaded run profile '" + profile_->name + "'.");
    }

    void TrackerEngine::ClearProfile()
    {
        profile_.reset();
        tasks_.clear();
        runtime_.clear();
        ResetRuntime();
    }

    bool TrackerEngine::HasProfile() const noexcept
    {
        return profile_.has_value() && !tasks_.empty();
    }

    const ProfileDefinition* TrackerEngine::Profile() const noexcept
    {
        return profile_ ? &*profile_ : nullptr;
    }

    const std::vector<FlatTaskView>& TrackerEngine::Tasks() const noexcept
    {
        return tasks_;
    }

    const std::vector<RuntimeTask>& TrackerEngine::TaskRuntime() const noexcept
    {
        return runtime_;
    }

    void TrackerEngine::SetMode(const RunMode mode, TrackerGame& game)
    {
        if (mode_ == mode || state_ == RunState::running || state_ == RunState::paused) {
            return;
        }
        mode_ = mode;
        ResetFresh(game);
        Notify(
            EngineNotification::Type::lifecycle,
            "Switched to " + ModeLabel(mode_) + " mode.");
    }

    RunMode TrackerEngine::Mode() const noexcept
    {
        return mode_;
    }

    RunState TrackerEngine::State() const noexcept
    {
        return state_;
    }

    PauseReason TrackerEngine::CurrentPauseReason() const noexcept
    {
        return pause_reason_;
    }

    ControlResult TrackerEngine::Start(
        TrackerGame& game, const AutomationSettings& automation, const bool automatic)
    {
        if (mode_ == RunMode::test) {
            return {false, "Use the Test Mode Task selector to arm an isolated Task."};
        }
        if (state_ != RunState::ready) {
            return {false, "Start is only available while the run is ready."};
        }
        std::string reason;
        if (!CanEnterRunning(game, automation, reason)) {
            if (!automatic) {
                Notify(EngineNotification::Type::warning, reason);
            }
            return {false, std::move(reason)};
        }
        BeginAttempt(game);
        state_ = RunState::running;
        running_since_ = Clock::now();
        pause_reason_ = PauseReason::invalid;
        auto_start_armed_ = false;
        if (!runtime_.empty()) {
            runtime_[0].status = RuntimeTaskStatus::active;
        }
        Notify(
            EngineNotification::Type::lifecycle,
            std::string(automatic ? "Automatically started" : "Started")
                + " run for " + character_name_ + ".");
        ActivateCurrentTask(game);
        RefreshHooks(game, automation);
        RequestCheckpoint();
        return {true, {}};
    }

    ControlResult TrackerEngine::Pause(TrackerGame& game, const PauseReason reason)
    {
        if (state_ != RunState::running) {
            return {false, "Pause is only available while the run is running."};
        }
        elapsed_base_ms_ = ElapsedMs();
        running_since_.reset();
        ReconcileExperience(game);
        PauseExperience();
        state_ = RunState::paused;
        pause_reason_ = reason;
        const auto pending_criterion =
            reason == PauseReason::logout
            ? deferred_criterion_
            : std::nullopt;
        DisarmHooks(game);
        deferred_criterion_ = pending_criterion;
        RequestCheckpoint();
        Notify(
            EngineNotification::Type::lifecycle,
            reason == PauseReason::logout ? "Paused after logout." : "Paused run.");
        return {true, {}};
    }

    ControlResult TrackerEngine::Resume(
        TrackerGame& game,
        const AutomationSettings& automation,
        const bool automatic,
        const PendingGameEvents* resume_events)
    {
        if (state_ != RunState::paused) {
            return {false, "Resume is only available while the run is paused."};
        }
        if (automatic && pause_reason_ == PauseReason::manual) {
            return {false, "A manually paused run must be resumed manually."};
        }
        std::string reason;
        if (!CanEnterRunning(game, automation, reason)) {
            if (!automatic) {
                Notify(EngineNotification::Type::warning, reason);
            }
            return {false, std::move(reason)};
        }
        if (resume_events) {
            UpdateExperience(game, *resume_events);
        }
        state_ = RunState::running;
        if (mode_ == RunMode::live) {
            running_since_ = Clock::now();
        }
        else {
            running_since_.reset();
        }
        pause_reason_ = PauseReason::invalid;
        ResumeExperience(game);
        Notify(
            EngineNotification::Type::lifecycle,
            std::string(automatic ? "Automatically resumed" : "Resumed") + " run.");
        ActivateCurrentTask(game, false, true);
        RefreshHooks(game, automation);
        RequestCheckpoint();
        return {true, {}};
    }

    ControlResult TrackerEngine::Split(TrackerGame& game)
    {
        if (state_ != RunState::running && state_ != RunState::paused) {
            return {false, "Split is only available during an active run."};
        }
        if (!ActiveTask()) {
            return {false, "There is no active Task to split."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Wait until Guild Wars has safely loaded a map before splitting."};
        }
        UndoSnapshot snapshot{
            .active_task_index = active_task_index_,
            .last_resolved_elapsed_ms = last_resolved_elapsed_ms_,
            .runtime = runtime_,
        };
        if (experience_) {
            snapshot.experience = experience_->persisted;
            snapshot.experience->elapsed_ms = ExperienceElapsedMs();
        }
        undo_ = snapshot;
        ResolveCurrentTask(game, TaskResult::completed, true);
        ActivateCurrentTask(game, true);
        return {true, {}};
    }

    ControlResult TrackerEngine::Skip(TrackerGame& game)
    {
        if (state_ != RunState::running && state_ != RunState::paused) {
            return {false, "Skip is only available during an active run."};
        }
        if (!ActiveTask()) {
            return {false, "There is no active Task to skip."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Wait until Guild Wars has safely loaded a map before skipping."};
        }
        undo_.reset();
        ResolveCurrentTask(game, TaskResult::skipped, false);
        ActivateCurrentTask(game);
        return {true, {}};
    }

    ControlResult TrackerEngine::Undo(TrackerGame& game)
    {
        if (state_ == RunState::completed) {
            return {false, "Completed runs are immutable."};
        }
        if (!undo_) {
            return {false, "Only the most recent manual split can be undone."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Wait until Guild Wars has safely loaded a map before undoing a split."};
        }
        if (
            undo_->active_task_index >= runtime_.size()
            || undo_->runtime.size() != runtime_.size()) {
            undo_.reset();
            return {false, "The last manual split can no longer be undone."};
        }
        PauseExperience();
        game.SetExperienceHook(false);
        game.SetActiveCriterion(nullptr);
        deferred_criterion_.reset();
        auto snapshot = std::move(*undo_);
        undo_.reset();
        active_task_index_ = snapshot.active_task_index;
        last_resolved_elapsed_ms_ = snapshot.last_resolved_elapsed_ms;
        runtime_ = std::move(snapshot.runtime);
        runtime_[active_task_index_].status = RuntimeTaskStatus::active;
        if (snapshot.experience) {
            experience_ = ExperienceInternal{
                .persisted = *snapshot.experience,
                .armed_map_loaded_sequence = game.MapLoadedSequence(),
            };
            if (state_ == RunState::running && experience_->persisted.armed) {
                experience_->persisted.baseline_experience = game.CurrentExperience().value_or(
                    experience_->persisted.baseline_experience);
                experience_->active_since = Clock::now();
            }
        }
        else {
            experience_.reset();
            InitializeExperience(game);
        }
        Notify(
            EngineNotification::Type::lifecycle,
            "Undid manual split; restored '" + TaskLabel(ActiveTask()) + "' without rewinding the timer.");
        RequestCheckpoint();
        return {true, {}};
    }

    void TrackerEngine::ResetFresh(TrackerGame& game)
    {
        DisarmHooks(game);
        ResetRuntime();
        auto_start_armed_ = false;
        Notify(EngineNotification::Type::lifecycle, "Reset to a new run at 0:00.");
    }

    ControlResult TrackerEngine::EvaluateCurrentTask(TrackerGame& game)
    {
        const auto active = ActiveTask();
        if (!active) {
            return {false, "There is no active Task to evaluate."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Guild Wars is not in a safe loaded-map state."};
        }
        const auto met = game.EvaluateCriterion(active->task->end_criterion);
        Notify(
            EngineNotification::Type::criterion,
            std::string("Current-state evaluation for '") + active->task->name
                + (met ? "' passed." : "' did not pass."));
        if (met && (state_ == RunState::running || state_ == RunState::paused)) {
            undo_.reset();
            ResolveCurrentTask(game, TaskResult::completed, false);
            ActivateCurrentTask(game);
        }
        return {true, met ? "Criterion met." : "Criterion not met."};
    }

    ControlResult TrackerEngine::EvaluateTaskForTest(
        const size_t task_index,
        TrackerGame& game)
    {
        if (mode_ != RunMode::test) {
            return {false, "Select Test Mode before evaluating an individual Task."};
        }
        if (task_index >= tasks_.size()) {
            return {false, "Select a valid Task to test."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Guild Wars is not in a safe loaded-map state."};
        }
        const auto met = game.EvaluateCriterion(tasks_[task_index].task->end_criterion);
        Notify(
            EngineNotification::Type::criterion,
            std::string("Current-state evaluation for '") + tasks_[task_index].task->name
                + (met ? "' passed." : "' did not pass."));
        return {true, met ? "Criterion met." : "Criterion not met."};
    }

    ControlResult TrackerEngine::StartTaskTest(
        const size_t task_index,
        TrackerGame& game,
        const AutomationSettings& automation)
    {
        if (mode_ != RunMode::test) {
            return {false, "Select Test Mode before arming an individual Task."};
        }
        if (state_ == RunState::running || state_ == RunState::paused) {
            return {false, "Reset the current test before arming another Task."};
        }
        if (task_index >= tasks_.size()) {
            return {false, "Select a valid Task to test."};
        }
        if (!HasProfile()) {
            return {false, "Load a valid run profile first."};
        }
        if (!game.IsSafeGameState()) {
            return {false, "Wait until Guild Wars has safely loaded a map."};
        }

        ResetRuntime();
        test_task_only_ = true;
        active_task_index_ = task_index;
        attempt_id_ = NewAttemptId();
        character_name_ = WideToUtf8(game.CurrentCharacterName());
        started_at_ = TimestampNow();
        state_ = RunState::running;
        runtime_[active_task_index_].status = RuntimeTaskStatus::active;
        Notify(
            EngineNotification::Type::lifecycle,
            "Armed isolated Task test for '" + tasks_[task_index].task->name + "'.");
        ActivateCurrentTask(game);
        RefreshHooks(game, automation);
        return {true, {}};
    }

    void TrackerEngine::Update(TrackerGame& game, const AutomationSettings& automation)
    {
        auto events = game.DrainPendingEvents();
        if (events.map_loaded) {
            deferred_map_loaded_sequence_ = events.map_loaded_sequence;
        }
        events.map_loaded =
            deferred_map_loaded_sequence_.has_value()
            && game.IsSafeGameState();
        if (events.map_loaded) {
            events.map_loaded_sequence = *deferred_map_loaded_sequence_;
        }
        if (events.criterion.pending) {
            if (const auto active = ActiveTask()) {
                deferred_criterion_ = {
                    .task_id = active->task->id,
                    .event = events.criterion,
                };
            }
            else {
                deferred_criterion_.reset();
            }
        }
        auto component_processed = false;
        if (state_ == RunState::running) {
            UpdateExperience(game, events);
            component_processed = true;
        }
        else {
            events.experience_event_count = 0;
            events.experience_amount_total = 0;
        }
        const auto returning_to_character_select =
            events.logout && events.returning_to_character_select;
        if (returning_to_character_select && state_ == RunState::running) {
            ProcessDeferredCriterion(game);
        }
        if (events.logout) {
            Notify(
                EngineNotification::Type::lifecycle,
                std::format(
                    "kLogout received (character_select={}, unknown={}).",
                    events.logout_character_select,
                    events.logout_unknown));
            if (returning_to_character_select && state_ == RunState::running) {
                Pause(game, PauseReason::logout);
            }
            else if (returning_to_character_select && state_ == RunState::ready) {
                auto_start_armed_ = true;
            }
        }

        if (
            mode_ == RunMode::live
            && state_ == RunState::ready
            && automation.auto_start
            && auto_start_armed_) {
            Start(game, automation, true);
        }
        else if (
            state_ == RunState::paused
            && automation.auto_resume
            && pause_reason_ != PauseReason::manual
            && (events.map_loaded
                || pause_reason_ == PauseReason::recovery && game.IsSafeGameState())) {
            PendingGameEvents resume_events{
                .map_loaded = events.map_loaded,
                .map_loaded_sequence = events.map_loaded_sequence,
            };
            const auto resumed = Resume(
                game,
                automation,
                true,
                events.map_loaded && !component_processed
                    ? &resume_events
                    : nullptr);
            if (resumed) {
                events.map_loaded = false;
                component_processed = true;
            }
        }

        if (state_ != RunState::running) {
            if (deferred_map_loaded_sequence_ && game.IsSafeGameState()) {
                deferred_map_loaded_sequence_.reset();
            }
            RefreshHooks(game, automation);
            return;
        }

        if (!component_processed) {
            UpdateExperience(game, events);
        }
        ProcessDeferredCriterion(game);
        if (deferred_map_loaded_sequence_ && game.IsSafeGameState()) {
            deferred_map_loaded_sequence_.reset();
        }
        RefreshHooks(game, automation);
    }

    void TrackerEngine::RefreshHooks(TrackerGame& game, const AutomationSettings& automation)
    {
        const auto active = ActiveTask();
        const auto running = state_ == RunState::running;
        game.SetActiveCriterion(running && active ? &active->task->end_criterion : nullptr);

        const auto needs_component_map = running && active && active->task->experience_tracker.has_value();
        const auto wait_to_start =
            mode_ == RunMode::live
            && state_ == RunState::ready
            && automation.auto_start
            && auto_start_armed_;
        const auto wait_to_rearm =
            mode_ == RunMode::live
            && state_ == RunState::ready
            && automation.auto_start
            && !auto_start_armed_;
        const auto wait_to_resume =
            state_ == RunState::paused
            && automation.auto_resume
            && pause_reason_ != PauseReason::manual;
        game.SetLifecycleHooks(
            needs_component_map || wait_to_start || wait_to_resume,
            running || wait_to_rearm);
        game.SetExperienceHook(running && experience_ && experience_->persisted.armed);
    }

    void TrackerEngine::DisarmHooks(TrackerGame& game)
    {
        game.SetActiveCriterion(nullptr);
        game.SetLifecycleHooks(false, false);
        game.SetExperienceHook(false);
        deferred_criterion_.reset();
    }

    bool TrackerEngine::AutoStartArmed() const noexcept
    {
        return auto_start_armed_;
    }

    void TrackerEngine::ArmAutoStart() noexcept
    {
        auto_start_armed_ = true;
    }

    bool TrackerEngine::HasUnsavedRunData() const
    {
        return mode_ == RunMode::live
            && (state_ == RunState::running || state_ == RunState::paused)
            && (ElapsedMs() != 0
                || std::ranges::any_of(runtime_, [](const RuntimeTask& task) {
                    return task.status == RuntimeTaskStatus::completed
                        || task.status == RuntimeTaskStatus::skipped;
                }));
    }

    std::optional<HistoryAttempt> TrackerEngine::BuildResetAttempt() const
    {
        if (!HasUnsavedRunData()) {
            return std::nullopt;
        }
        return MakeAttempt(AttemptResult::reset);
    }

    std::optional<ActiveRunFile> TrackerEngine::BuildActiveRunFile() const
    {
        if (
            mode_ != RunMode::live
            || !profile_
            || (state_ != RunState::running && state_ != RunState::paused)
            || !ActiveTask()) {
            return std::nullopt;
        }
        ActiveRunFile active{
            .profile_id = profile_->id,
            .route_hash = ComputeRouteHash(*profile_),
            .attempt_id = attempt_id_,
            .character_name = character_name_,
            .started_at = started_at_,
            .updated_at = TimestampNow(),
            .state = state_ == RunState::running ? ActiveRunStatus::running : ActiveRunStatus::paused,
            .elapsed_ms = ElapsedMs(),
            .active_task_id = ActiveTask()->task->id,
            .last_resolved_elapsed_ms = last_resolved_elapsed_ms_,
        };
        if (state_ == RunState::paused) {
            active.pause_reason = pause_reason_;
        }
        for (size_t i = 0; i < active_task_index_; ++i) {
            if (
                runtime_[i].status == RuntimeTaskStatus::completed
                || runtime_[i].status == RuntimeTaskStatus::skipped) {
                active.resolved_tasks.push_back(MakeTaskRecord(i));
            }
        }
        if (experience_) {
            active.experience_tracker = ExperienceTrackerState{
                .task_id = ActiveTask()->task->id,
                .armed = experience_->persisted.armed,
                .experience_gained = experience_->persisted.experience_gained,
                .baseline_experience = experience_->persisted.baseline_experience,
                .runs = experience_->persisted.runs,
                .elapsed_ms = ExperienceElapsedMs(),
            };
        }
        return active;
    }

    ControlResult TrackerEngine::Restore(const ActiveRunFile& active_run, TrackerGame& game)
    {
        if (!profile_) {
            return {false, "Cannot recover a run before its profile is loaded."};
        }
        if (active_run.profile_id != profile_->id) {
            return {false, "Recovery state belongs to a different profile."};
        }
        if (active_run.route_hash != ComputeRouteHash(*profile_)) {
            return {false, "Recovery state route hash does not match the current profile."};
        }

        ResetRuntime();
        size_t expected_index = 0;
        for (const auto& record : active_run.resolved_tasks) {
            if (expected_index >= tasks_.size() || tasks_[expected_index].task->id != record.id) {
                ResetRuntime();
                return {false, "Recovery state Tasks are not a sequential prefix of this route."};
            }
            runtime_[expected_index].status =
                record.result == TaskResult::skipped ? RuntimeTaskStatus::skipped : RuntimeTaskStatus::completed;
            runtime_[expected_index].split_ms = record.split_ms;
            runtime_[expected_index].segment_ms = record.segment_ms;
            ++expected_index;
        }
        if (
            expected_index >= tasks_.size()
            || tasks_[expected_index].task->id != active_run.active_task_id) {
            ResetRuntime();
            return {false, "Recovery state active_task_id is not the next Task in this route."};
        }
        active_task_index_ = expected_index;
        runtime_[active_task_index_].status = RuntimeTaskStatus::active;
        last_resolved_elapsed_ms_ = active_run.last_resolved_elapsed_ms;
        state_ = RunState::paused;
        pause_reason_ = active_run.state == ActiveRunStatus::paused
            ? active_run.pause_reason.value_or(PauseReason::recovery)
            : PauseReason::recovery;
        elapsed_base_ms_ = active_run.elapsed_ms;
        attempt_id_ = active_run.attempt_id;
        character_name_ = active_run.character_name;
        started_at_ = active_run.started_at;
        auto_start_armed_ = false;
        if (active_run.experience_tracker && tasks_[active_task_index_].task->experience_tracker) {
            experience_ = ExperienceInternal{
                .persisted = {
                    .armed = active_run.experience_tracker->armed,
                    .experience_gained = active_run.experience_tracker->experience_gained,
                    .baseline_experience = active_run.experience_tracker->baseline_experience,
                    .runs = active_run.experience_tracker->runs,
                    .elapsed_ms = active_run.experience_tracker->elapsed_ms,
                },
                .armed_map_loaded_sequence = game.MapLoadedSequence(),
            };
        }
        else {
            InitializeExperience(game);
            PauseExperience();
        }
        RequestCheckpoint();
        Notify(
            EngineNotification::Type::lifecycle,
            "Recovered run at '" + TaskLabel(ActiveTask()) + "'; downtime was excluded.");
        return {true, {}};
    }

    uint64_t TrackerEngine::ElapsedMs() const
    {
        if (mode_ == RunMode::test) {
            return 0;
        }
        if (!running_since_) {
            return elapsed_base_ms_;
        }
        const auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - *running_since_);
        return elapsed_base_ms_ + static_cast<uint64_t>(std::max<int64_t>(0, current.count()));
    }

    uint64_t TrackerEngine::LastResolvedElapsedMs() const noexcept
    {
        return last_resolved_elapsed_ms_;
    }

    size_t TrackerEngine::ActiveTaskIndex() const noexcept
    {
        return active_task_index_;
    }

    const FlatTaskView* TrackerEngine::ActiveTask() const noexcept
    {
        return active_task_index_ < tasks_.size() ? &tasks_[active_task_index_] : nullptr;
    }

    const ExperienceRuntime* TrackerEngine::ActiveExperience() const noexcept
    {
        return experience_ ? &experience_->persisted : nullptr;
    }

    uint64_t TrackerEngine::ActiveExperienceElapsedMs() const
    {
        return ExperienceElapsedMs();
    }

    bool TrackerEngine::CheckpointRequested() noexcept
    {
        return std::exchange(checkpoint_requested_, false);
    }

    bool TrackerEngine::ActiveRunRemovalRequested() noexcept
    {
        return std::exchange(active_run_removal_requested_, false);
    }

    std::optional<HistoryAttempt> TrackerEngine::TakeFinishedAttempt()
    {
        return std::exchange(finished_attempt_, std::nullopt);
    }

    std::vector<EngineNotification> TrackerEngine::DrainNotifications()
    {
        std::vector<EngineNotification> result;
        result.reserve(notifications_.size());
        while (!notifications_.empty()) {
            result.push_back(std::move(notifications_.front()));
            notifications_.pop_front();
        }
        return result;
    }

    std::string TrackerEngine::FormatDuration(const uint64_t milliseconds, const bool tenths)
    {
        const auto hours = milliseconds / 3'600'000;
        const auto minutes = milliseconds / 60'000 % 60;
        const auto seconds = milliseconds / 1'000 % 60;
        const auto decimal = milliseconds / 100 % 10;
        if (hours) {
            return tenths
                ? std::format("{}:{:02}:{:02}.{}", hours, minutes, seconds, decimal)
                : std::format("{}:{:02}:{:02}", hours, minutes, seconds);
        }
        return tenths
            ? std::format("{}:{:02}.{}", minutes, seconds, decimal)
            : std::format("{}:{:02}", minutes, seconds);
    }

    std::string TrackerEngine::StateLabel(const RunState state)
    {
        switch (state) {
            case RunState::ready: return "Ready";
            case RunState::running: return "Running";
            case RunState::paused: return "Paused";
            case RunState::completed: return "Completed";
        }
        return "Unknown";
    }

    std::string TrackerEngine::ModeLabel(const RunMode mode)
    {
        return mode == RunMode::test ? "Test" : "Live";
    }

    bool TrackerEngine::CharacterMatches(
        const TrackerGame& game, const AutomationSettings& automation) const
    {
        if (automation.character_name.empty()) {
            return false;
        }
        const auto current = game.CurrentCharacterName();
        if (current.empty()) {
            return false;
        }
        return CompareStringOrdinal(
            current.c_str(),
            static_cast<int>(current.size()),
            automation.character_name.c_str(),
            static_cast<int>(automation.character_name.size()),
            TRUE) == CSTR_EQUAL;
    }

    bool TrackerEngine::CanEnterRunning(
        const TrackerGame& game,
        const AutomationSettings& automation,
        std::string& reason) const
    {
        if (!HasProfile()) {
            reason = "Load a valid run profile first.";
            return false;
        }
        if (!game.IsSafeGameState()) {
            reason = "Wait until Guild Wars has safely loaded a map.";
            return false;
        }
        if (mode_ == RunMode::test) {
            return true;
        }
        if (automation.character_name.empty()) {
            reason = "Configure the run character name in plugin settings.";
            return false;
        }
        if (!CharacterMatches(game, automation)) {
            reason = "The loaded character does not match the configured run character.";
            return false;
        }
        return true;
    }

    void TrackerEngine::BeginAttempt(TrackerGame& game)
    {
        ResetRuntime();
        attempt_id_ = NewAttemptId();
        character_name_ = WideToUtf8(game.CurrentCharacterName());
        started_at_ = TimestampNow();
    }

    void TrackerEngine::ActivateCurrentTask(
        TrackerGame& game,
        const bool preserve_manual_undo,
        const bool preserve_experience)
    {
        auto keep_experience = preserve_experience;
        while (state_ == RunState::running || state_ == RunState::paused) {
            const auto active = ActiveTask();
            if (!active) {
                FinishRun(game);
                return;
            }
            runtime_[active_task_index_].status = RuntimeTaskStatus::active;
            if (!std::exchange(keep_experience, false)) {
                InitializeExperience(game);
            }
            if (
                test_task_only_
                || !game.IsSafeGameState()
                || !game.EvaluateCriterion(active->task->end_criterion)) {
                Notify(
                    EngineNotification::Type::criterion,
                    "Activated '" + active->task->name + "'; waiting for its criterion.");
                return;
            }
            Notify(
                EngineNotification::Type::criterion,
                "Criterion already met for '" + active->task->name + "'; completed immediately.");
            ResolveCurrentTask(game, TaskResult::completed, false, preserve_manual_undo);
        }
    }

    void TrackerEngine::ProcessDeferredCriterion(TrackerGame& game)
    {
        if (!deferred_criterion_) {
            return;
        }
        const auto active = ActiveTask();
        if (!(active && active->task->id == deferred_criterion_->task_id)) {
            deferred_criterion_.reset();
            return;
        }
        if (!game.IsSafeGameState()) {
            return;
        }
        const auto event = deferred_criterion_->event;
        deferred_criterion_.reset();
        Notify(
            EngineNotification::Type::criterion,
            std::format(
                "Active criterion callback received (message=0x{:08x}, value0={}, value1={}).",
                static_cast<uint32_t>(event.message),
                event.value0,
                event.value1));
        if (event.message == GW::UI::UIMessage::kMapLoaded) {
            const auto& criterion = active->task->end_criterion;
            const auto targets = criterion.map
                ? game.ResolveMapIds(*criterion.map, MapUsage::Any)
                : std::vector<uint32_t>{};
            if (std::ranges::find(targets, event.value0) != targets.end()) {
                ResolveCurrentTask(game, TaskResult::completed, false);
                ActivateCurrentTask(game);
            }
            return;
        }
        EvaluateCurrentTask(game);
    }

    void TrackerEngine::ResolveCurrentTask(
        TrackerGame& game,
        const TaskResult result,
        const bool manual_split,
        const bool preserve_undo)
    {
        if (active_task_index_ >= runtime_.size()) {
            return;
        }
        deferred_criterion_.reset();
        PauseExperience();
        game.SetExperienceHook(false);
        game.SetActiveCriterion(nullptr);
        const auto elapsed = ElapsedMs();
        last_resolved_elapsed_ms_ = elapsed;
        auto& task = runtime_[active_task_index_];
        task.status = result == TaskResult::skipped
            ? RuntimeTaskStatus::skipped
            : RuntimeTaskStatus::completed;
        task.manually_split = manual_split;
        if (result == TaskResult::completed) {
            task.split_ms = elapsed;
            task.segment_ms = elapsed - PreviousSplitMs(active_task_index_);
        }
        else {
            task.split_ms.reset();
            task.segment_ms.reset();
        }
        Notify(
            EngineNotification::Type::criterion,
            std::string(result == TaskResult::skipped ? "Skipped '" : "Completed '")
                + tasks_[active_task_index_].task->name + "'.");
        ++active_task_index_;
        experience_.reset();
        if (!manual_split && !preserve_undo) {
            undo_.reset();
        }
        RequestCheckpoint();
        if (test_task_only_) {
            active_task_index_ = tasks_.size();
            FinishRun(game);
            return;
        }
        if (active_task_index_ >= tasks_.size()) {
            FinishRun(game);
        }
    }

    void TrackerEngine::FinishRun(TrackerGame& game)
    {
        if (state_ == RunState::completed) {
            return;
        }
        elapsed_base_ms_ = ElapsedMs();
        running_since_.reset();
        PauseExperience();
        DisarmHooks(game);
        state_ = RunState::completed;
        pause_reason_ = PauseReason::invalid;
        undo_.reset();
        if (mode_ == RunMode::live) {
            finished_attempt_ = MakeAttempt(AttemptResult::completed);
        }
        active_run_removal_requested_ = mode_ == RunMode::live;
        Notify(EngineNotification::Type::lifecycle, "Run completed.");
    }

    void TrackerEngine::InitializeExperience(TrackerGame& game)
    {
        experience_.reset();
        const auto active = ActiveTask();
        if (!(active && active->task->experience_tracker)) {
            return;
        }
        experience_.emplace();
        if (game.IsSafeGameState()
            && game.EvaluateCriterion(active->task->experience_tracker->arm_criterion)) {
            ArmExperience(game);
        }
    }

    void TrackerEngine::ArmExperience(TrackerGame& game)
    {
        if (!experience_ || experience_->persisted.armed) {
            return;
        }
        const auto current_experience = game.CurrentExperience();
        if (!current_experience) {
            Notify(
                EngineNotification::Type::component,
                "Experience tracker is waiting for the character's current experience.");
            return;
        }
        experience_->persisted.armed = true;
        experience_->persisted.baseline_experience = *current_experience;
        experience_->armed_map_loaded_sequence = game.MapLoadedSequence();
        if (state_ == RunState::running) {
            experience_->active_since = Clock::now();
        }
        Notify(EngineNotification::Type::component, "Experience tracker armed.");
    }

    void TrackerEngine::ReconcileExperience(
        TrackerGame& game,
        const uint64_t fallback_experience)
    {
        if (!(experience_ && experience_->persisted.armed)) {
            return;
        }
        if (const auto current = game.CurrentExperience()) {
            if (*current >= experience_->persisted.baseline_experience) {
                experience_->persisted.experience_gained +=
                    *current - experience_->persisted.baseline_experience;
            }
            experience_->persisted.baseline_experience = *current;
            return;
        }
        if (fallback_experience) {
            const auto gained_room =
                std::numeric_limits<uint64_t>::max()
                - experience_->persisted.experience_gained;
            experience_->persisted.experience_gained +=
                std::min(gained_room, fallback_experience);
            const auto baseline_room =
                std::numeric_limits<uint32_t>::max()
                - experience_->persisted.baseline_experience;
            experience_->persisted.baseline_experience += static_cast<uint32_t>(
                std::min<uint64_t>(baseline_room, fallback_experience));
        }
    }

    void TrackerEngine::UpdateExperience(TrackerGame& game, const PendingGameEvents& events)
    {
        const auto active = ActiveTask();
        if (!(active && active->task->experience_tracker && experience_)) {
            return;
        }
        const auto& definition = *active->task->experience_tracker;
        if (events.map_loaded) {
            if (!experience_->persisted.armed && game.EvaluateCriterion(definition.arm_criterion)) {
                ArmExperience(game);
            }
            else if (
                experience_->persisted.armed
                && events.map_loaded_sequence > experience_->armed_map_loaded_sequence
                && game.EvaluateCriterion(definition.increment_criterion)) {
                ++experience_->persisted.runs;
                Notify(
                    EngineNotification::Type::component,
                    std::format("Experience tracker run count is now {}.", experience_->persisted.runs));
            }
            if (experience_->persisted.armed) {
                experience_->armed_map_loaded_sequence =
                    events.map_loaded_sequence;
            }
        }
        if (events.experience_event_count && experience_->persisted.armed) {
            ReconcileExperience(game, events.experience_amount_total);
        }
    }

    void TrackerEngine::PauseExperience()
    {
        if (!(experience_ && experience_->active_since)) {
            return;
        }
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - *experience_->active_since);
        experience_->persisted.elapsed_ms += static_cast<uint64_t>(std::max<int64_t>(0, delta.count()));
        experience_->active_since.reset();
    }

    void TrackerEngine::ResumeExperience(TrackerGame& game)
    {
        if (!experience_) {
            InitializeExperience(game);
            return;
        }
        if (experience_->persisted.armed) {
            experience_->persisted.baseline_experience = game.CurrentExperience().value_or(
                experience_->persisted.baseline_experience);
            experience_->active_since = Clock::now();
        }
        else if (const auto active = ActiveTask(); active && active->task->experience_tracker
                 && game.EvaluateCriterion(active->task->experience_tracker->arm_criterion)) {
            ArmExperience(game);
        }
    }

    uint64_t TrackerEngine::ExperienceElapsedMs() const
    {
        if (!(experience_ && experience_->active_since)) {
            return experience_ ? experience_->persisted.elapsed_ms : 0;
        }
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - *experience_->active_since);
        return experience_->persisted.elapsed_ms
            + static_cast<uint64_t>(std::max<int64_t>(0, delta.count()));
    }

    TaskRecord TrackerEngine::MakeTaskRecord(const size_t index) const
    {
        const auto& task = runtime_[index];
        return {
            .id = tasks_[index].task->id,
            .result = task.status == RuntimeTaskStatus::skipped
                ? TaskResult::skipped
                : TaskResult::completed,
            .split_ms = task.split_ms,
            .segment_ms = task.segment_ms,
        };
    }

    HistoryAttempt TrackerEngine::MakeAttempt(const AttemptResult result) const
    {
        HistoryAttempt attempt{
            .id = attempt_id_,
            .route_hash = profile_ ? ComputeRouteHash(*profile_) : "",
            .character_name = character_name_,
            .started_at = started_at_,
            .ended_at = TimestampNow(),
            .result = result,
            .elapsed_ms = ElapsedMs(),
        };
        const auto limit = result == AttemptResult::completed
            ? runtime_.size()
            : std::min(active_task_index_, runtime_.size());
        for (size_t i = 0; i < limit; ++i) {
            if (
                runtime_[i].status == RuntimeTaskStatus::completed
                || runtime_[i].status == RuntimeTaskStatus::skipped) {
                attempt.tasks.push_back(MakeTaskRecord(i));
            }
        }
        return attempt;
    }

    void TrackerEngine::RequestCheckpoint()
    {
        if (mode_ == RunMode::live) {
            checkpoint_requested_ = true;
        }
    }

    void TrackerEngine::Notify(const EngineNotification::Type type, std::string text)
    {
        if (notifications_.size() == MaxNotifications) {
            notifications_.pop_front();
        }
        notifications_.push_back({type, std::move(text)});
    }

    std::string TrackerEngine::NewAttemptId() const
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
        return std::format("{:x}", static_cast<uint64_t>(micros));
    }

    std::string TrackerEngine::TimestampNow()
    {
        const auto now = std::chrono::system_clock::now();
        const auto value = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
        gmtime_s(&utc, &value);
        return std::format(
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
            utc.tm_year + 1900,
            utc.tm_mon + 1,
            utc.tm_mday,
            utc.tm_hour,
            utc.tm_min,
            utc.tm_sec);
    }

    uint64_t TrackerEngine::PreviousSplitMs(const size_t before_index) const
    {
        for (auto index = before_index; index > 0; --index) {
            if (runtime_[index - 1].split_ms) {
                return *runtime_[index - 1].split_ms;
            }
        }
        return 0;
    }

    void TrackerEngine::ResetRuntime()
    {
        runtime_.assign(tasks_.size(), {});
        active_task_index_ = 0;
        state_ = RunState::ready;
        pause_reason_ = PauseReason::invalid;
        elapsed_base_ms_ = 0;
        last_resolved_elapsed_ms_ = 0;
        running_since_.reset();
        attempt_id_.clear();
        character_name_.clear();
        started_at_.clear();
        experience_.reset();
        undo_.reset();
        deferred_criterion_.reset();
        deferred_map_loaded_sequence_.reset();
        test_task_only_ = false;
        checkpoint_requested_ = false;
        active_run_removal_requested_ = false;
        finished_attempt_.reset();
    }
}
