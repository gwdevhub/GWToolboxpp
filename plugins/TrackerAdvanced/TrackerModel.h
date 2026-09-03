#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <GWCA/Constants/Constants.h>

#include <glaze/glaze.hpp>

namespace TrackerAdvanced {
    inline constexpr std::string_view ProfileSchema = "trackeradvanced.profile";
    inline constexpr std::string_view HistorySchema = "trackeradvanced.history";
    inline constexpr std::string_view ActiveRunSchema = "trackeradvanced.active_run";
    inline constexpr uint32_t SchemaVersion = 1;

    enum class CriterionType {
        invalid,
        player_level,
        map_loaded,
        mission_complete,
        dungeon_complete,
        vanquish_complete,
        title_progress,
        manual,
    };

    enum class ActionType {
        invalid,
        travel,
        player_build,
        hero_team_build,
    };

    enum class TaskResult {
        invalid,
        completed,
        skipped,
    };

    enum class AttemptResult {
        invalid,
        completed,
        reset,
    };

    enum class ActiveRunStatus {
        invalid,
        running,
        paused,
    };

    enum class PauseReason {
        invalid,
        manual,
        logout,
        recovery,
    };

    struct CriterionDefinition {
        CriterionType type = CriterionType::invalid;
        std::optional<std::string> map;
        std::optional<std::string> mission;
        std::optional<std::string> dungeon;
        std::optional<uint32_t> level;
        std::optional<bool> is_hard_mode;
        std::optional<std::string> title;
        std::optional<uint32_t> required_progress;
    };

    struct ExperienceTrackerDefinition {
        std::string label;
        CriterionDefinition arm_criterion;
        CriterionDefinition increment_criterion;
        uint32_t goal_experience = 0;
    };

    struct ActionDefinition {
        uint32_t id = 0;
        ActionType type = ActionType::invalid;
        std::string label;
        std::optional<std::string> destination;
        std::optional<std::string> team_build;
        std::optional<std::string> build;
        std::optional<std::string> name;
    };

    struct TitleTrackerDefinition {
        std::string title;
        bool hide_when_complete = false;
    };

    struct TaskDefinition {
        uint32_t id = 0;
        std::string name;
        uint64_t expected_time_ms = 0;
        CriterionDefinition end_criterion;
        std::vector<std::string> notes;
        std::vector<uint32_t> action_ids;
        std::optional<ExperienceTrackerDefinition> experience_tracker;
    };

    struct ObjectiveDefinition {
        uint32_t id = 0;
        std::string name;
        std::vector<TitleTrackerDefinition> title_trackers;
        std::vector<TaskDefinition> tasks;
    };

    struct ProfileDefinition {
        std::string schema{ProfileSchema};
        uint32_t schema_version = SchemaVersion;
        std::string id;
        std::string name;
        std::optional<std::string> description;
        std::vector<TitleTrackerDefinition> title_trackers;
        std::vector<ActionDefinition> actions;
        std::vector<ObjectiveDefinition> objectives;
    };

    struct TaskRecord {
        uint32_t id = 0;
        TaskResult result = TaskResult::invalid;
        std::optional<uint64_t> split_ms;
        std::optional<uint64_t> segment_ms;
    };

    struct HistoryAttempt {
        std::string id;
        std::string route_hash;
        std::string character_name;
        std::string started_at;
        std::string ended_at;
        AttemptResult result = AttemptResult::invalid;
        uint64_t elapsed_ms = 0;
        std::vector<TaskRecord> tasks;
    };

    struct HistoryFile {
        std::string schema{HistorySchema};
        uint32_t schema_version = SchemaVersion;
        std::string profile_id;
        std::vector<HistoryAttempt> attempts;
    };

    struct ExperienceTrackerState {
        uint32_t task_id = 0;
        bool armed = false;
        uint64_t experience_gained = 0;
        uint32_t baseline_experience = 0;
        uint32_t runs = 0;
        uint64_t elapsed_ms = 0;
    };

    struct ActiveRunFile {
        std::string schema{ActiveRunSchema};
        uint32_t schema_version = SchemaVersion;
        std::string profile_id;
        std::string route_hash;
        std::string attempt_id;
        std::string character_name;
        std::string started_at;
        std::string updated_at;
        ActiveRunStatus state = ActiveRunStatus::invalid;
        std::optional<PauseReason> pause_reason;
        uint64_t elapsed_ms = 0;
        uint32_t active_task_id = 0;
        uint64_t last_resolved_elapsed_ms = 0;
        std::vector<TaskRecord> resolved_tasks;
        std::optional<ExperienceTrackerState> experience_tracker;
    };

    struct ValidationResult {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        [[nodiscard]] bool Ok() const noexcept { return errors.empty(); }
    };

    struct SupportedTitle {
        std::string_view token;
        std::string_view label;
        GW::Constants::TitleID id;
    };

    struct FlatTaskView {
        const ObjectiveDefinition* objective = nullptr;
        const TaskDefinition* task = nullptr;
        size_t objective_index = 0;
        size_t task_index = 0;
        size_t flat_index = 0;
    };

    using ProfileResult = std::expected<ProfileDefinition, std::string>;
    using HistoryResult = std::expected<HistoryFile, std::string>;
    using ActiveRunResult = std::expected<ActiveRunFile, std::string>;
    using JsonResult = std::expected<std::string, std::string>;

    [[nodiscard]] ProfileResult ParseProfile(std::string_view json);
    [[nodiscard]] JsonResult WriteProfile(const ProfileDefinition& profile);
    [[nodiscard]] HistoryResult ParseHistory(std::string_view json);
    [[nodiscard]] JsonResult WriteHistory(const HistoryFile& history);
    [[nodiscard]] ActiveRunResult ParseActiveRun(std::string_view json);
    [[nodiscard]] JsonResult WriteActiveRun(const ActiveRunFile& active_run);

    [[nodiscard]] ValidationResult ValidateProfile(const ProfileDefinition& profile);
    [[nodiscard]] ValidationResult ValidateHistory(const HistoryFile& history);
    [[nodiscard]] ValidationResult ValidateActiveRun(const ActiveRunFile& active_run);
    [[nodiscard]] std::string ComputeRouteHash(const ProfileDefinition& profile);
    [[nodiscard]] std::vector<FlatTaskView> FlattenTasks(const ProfileDefinition& profile);
    [[nodiscard]] std::span<const SupportedTitle> SupportedTitles();
    [[nodiscard]] const SupportedTitle* FindTitle(std::string_view token);
}

template <>
struct glz::meta<TrackerAdvanced::CriterionType> {
    using enum TrackerAdvanced::CriterionType;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "player_level", player_level,
        "map_loaded", map_loaded,
        "mission_complete", mission_complete,
        "dungeon_complete", dungeon_complete,
        "vanquish_complete", vanquish_complete,
        "title_progress", title_progress,
        "manual", manual);
};

template <>
struct glz::meta<TrackerAdvanced::ActionType> {
    using enum TrackerAdvanced::ActionType;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "travel", travel,
        "player_build", player_build,
        "hero_team_build", hero_team_build);
};

template <>
struct glz::meta<TrackerAdvanced::TaskResult> {
    using enum TrackerAdvanced::TaskResult;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "completed", completed,
        "skipped", skipped);
};

template <>
struct glz::meta<TrackerAdvanced::AttemptResult> {
    using enum TrackerAdvanced::AttemptResult;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "completed", completed,
        "reset", reset);
};

template <>
struct glz::meta<TrackerAdvanced::ActiveRunStatus> {
    using enum TrackerAdvanced::ActiveRunStatus;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "running", running,
        "paused", paused);
};

template <>
struct glz::meta<TrackerAdvanced::PauseReason> {
    using enum TrackerAdvanced::PauseReason;
    static constexpr auto value = glz::enumerate(
        "invalid", invalid,
        "manual", manual,
        "logout", logout,
        "recovery", recovery);
};
