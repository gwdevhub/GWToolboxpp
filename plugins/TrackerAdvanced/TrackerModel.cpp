#include "TrackerModel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {
    using namespace TrackerAdvanced;

    constexpr auto ReadOptions = glz::opts{
        .error_on_unknown_keys = true,
        .error_on_missing_keys = true,
    };
    constexpr auto WriteOptions = glz::opts{
        .prettify = true,
    };

    constexpr std::array SupportedTitleEntries{
        SupportedTitle{"TyrianCarto", "Tyrian Cartographer", GW::Constants::TitleID::TyrianCarto},
        SupportedTitle{"CanthanCarto", "Canthan Cartographer", GW::Constants::TitleID::CanthanCarto},
        SupportedTitle{"ElonianCarto", "Elonian Cartographer", GW::Constants::TitleID::ElonianCarto},
        SupportedTitle{"SkillHunterTyria", "Tyrian Skill Hunter", GW::Constants::TitleID::SkillHunterTyria},
        SupportedTitle{"SkillHunterCantha", "Canthan Skill Hunter", GW::Constants::TitleID::SkillHunterCantha},
        SupportedTitle{"SkillHunterElona", "Elonian Skill Hunter", GW::Constants::TitleID::SkillHunterElona},
        SupportedTitle{"VanquisherTyria", "Tyrian Vanquisher", GW::Constants::TitleID::VanquisherTyria},
        SupportedTitle{"VanquisherCantha", "Canthan Vanquisher", GW::Constants::TitleID::VanquisherCantha},
        SupportedTitle{"VanquisherElona", "Elonian Vanquisher", GW::Constants::TitleID::VanquisherElona},
        SupportedTitle{"ProtectorTyria", "Protector of Tyria", GW::Constants::TitleID::ProtectorTyria},
        SupportedTitle{"ProtectorCantha", "Protector of Cantha", GW::Constants::TitleID::ProtectorCantha},
        SupportedTitle{"ProtectorElona", "Protector of Elona", GW::Constants::TitleID::ProtectorElona},
        SupportedTitle{"GuardianTyria", "Guardian of Tyria", GW::Constants::TitleID::GuardianTyria},
        SupportedTitle{"GuardianCantha", "Guardian of Cantha", GW::Constants::TitleID::GuardianCantha},
        SupportedTitle{"GuardianElona", "Guardian of Elona", GW::Constants::TitleID::GuardianElona},
        SupportedTitle{"Survivor", "Legendary Survivor", GW::Constants::TitleID::Survivor},
        SupportedTitle{"Party", "Party Animal", GW::Constants::TitleID::Party},
        SupportedTitle{"Drunkard", "Drunkard", GW::Constants::TitleID::Drunkard},
        SupportedTitle{"Sweets", "Sweet Tooth", GW::Constants::TitleID::Sweets},
        SupportedTitle{"LDoA", "Legendary Defender of Ascalon", GW::Constants::TitleID::LDoA},
        SupportedTitle{"Sunspear", "Sunspear", GW::Constants::TitleID::Sunspear},
        SupportedTitle{"Lightbringer", "Lightbringer", GW::Constants::TitleID::Lightbringer},
        SupportedTitle{"Asuran", "Asuran", GW::Constants::TitleID::Asuran},
        SupportedTitle{"Norn", "Norn", GW::Constants::TitleID::Norn},
        SupportedTitle{"Vanguard", "Vanguard", GW::Constants::TitleID::Vanguard},
        SupportedTitle{"Deldrimor", "Delver", GW::Constants::TitleID::Deldrimor},
        SupportedTitle{"LegendaryGuardian", "Legendary Guardian", GW::Constants::TitleID::LegendaryGuardian},
        SupportedTitle{"LegendarySkillHunter", "Legendary Skill Hunter", GW::Constants::TitleID::LegendarySkillHunter},
        SupportedTitle{"LegendaryCarto", "Legendary Cartographer", GW::Constants::TitleID::LegendaryCarto},
        SupportedTitle{"LegendaryVanquisher", "Legendary Vanquisher", GW::Constants::TitleID::LegendaryVanquisher},
        SupportedTitle{"MasterOfTheNorth", "Master Of The North", GW::Constants::TitleID::MasterOfTheNorth},
        SupportedTitle{"KoaBD", "Maxed Titles", GW::Constants::TitleID::KoaBD},
    };

    bool IsBlank(const std::string_view value)
    {
        return std::ranges::all_of(value, [](const unsigned char c) { return std::isspace(c) != 0; });
    }

    bool HasOuterWhitespace(const std::string_view value)
    {
        return !value.empty()
            && (std::isspace(static_cast<unsigned char>(value.front())) != 0
                || std::isspace(static_cast<unsigned char>(value.back())) != 0);
    }

    bool IsProfileId(const std::string_view value)
    {
        if (
            value.empty()
            || std::isalnum(static_cast<unsigned char>(value.front())) == 0) {
            return false;
        }
        return std::ranges::all_of(value, [](const unsigned char character) {
            return std::isdigit(character) != 0
                || character >= 'a' && character <= 'z'
                || character == '-'
                || character == '_';
        });
    }

    bool IsWindowsReservedStem(const std::string_view value)
    {
        const auto equals = [value](const std::string_view reserved) {
            return value.size() == reserved.size()
                && std::ranges::equal(value, reserved, [](const unsigned char left, const unsigned char right) {
                    return std::tolower(left) == std::tolower(right);
                });
        };
        if (equals("con") || equals("prn") || equals("aux") || equals("nul")) {
            return true;
        }
        return value.size() == 4
            && (std::tolower(static_cast<unsigned char>(value[0])) == 'c'
                    && std::tolower(static_cast<unsigned char>(value[1])) == 'o'
                    && std::tolower(static_cast<unsigned char>(value[2])) == 'm'
                || std::tolower(static_cast<unsigned char>(value[0])) == 'l'
                    && std::tolower(static_cast<unsigned char>(value[1])) == 'p'
                    && std::tolower(static_cast<unsigned char>(value[2])) == 't')
            && value[3] >= '1' && value[3] <= '9';
    }

    void AddError(ValidationResult& result, const std::string& path, const std::string_view message)
    {
        result.errors.push_back(path + ": " + std::string(message));
    }

    void AddWarning(ValidationResult& result, const std::string& path, const std::string_view message)
    {
        result.warnings.push_back(path + ": " + std::string(message));
    }

    void RequireText(
        ValidationResult& result,
        const std::string& path,
        const std::optional<std::string>& value)
    {
        if (!value || IsBlank(*value)) {
            AddError(result, path, "is required and cannot be blank");
        }
        else if (HasOuterWhitespace(*value)) {
            AddWarning(result, path, "has leading or trailing whitespace");
        }
    }

    void RequireText(ValidationResult& result, const std::string& path, const std::string& value)
    {
        if (IsBlank(value)) {
            AddError(result, path, "is required and cannot be blank");
        }
        else if (HasOuterWhitespace(value)) {
            AddWarning(result, path, "has leading or trailing whitespace");
        }
    }

    template <typename T>
    void RejectField(
        ValidationResult& result,
        const std::string& path,
        const char* field,
        const std::optional<T>& value)
    {
        if (value) {
            AddError(result, path + "." + field, "is not valid for this type; remove it");
        }
    }

    void RejectCriterionFields(
        ValidationResult& result,
        const std::string& path,
        const CriterionDefinition& criterion,
        const std::initializer_list<std::string_view> allowed)
    {
        const auto is_allowed = [&](const std::string_view field) {
            return std::ranges::find(allowed, field) != allowed.end();
        };
        if (!is_allowed("map")) RejectField(result, path, "map", criterion.map);
        if (!is_allowed("mission")) RejectField(result, path, "mission", criterion.mission);
        if (!is_allowed("dungeon")) RejectField(result, path, "dungeon", criterion.dungeon);
        if (!is_allowed("level")) RejectField(result, path, "level", criterion.level);
        if (!is_allowed("is_hard_mode")) RejectField(result, path, "is_hard_mode", criterion.is_hard_mode);
        if (!is_allowed("title")) RejectField(result, path, "title", criterion.title);
        if (!is_allowed("required_progress")) RejectField(result, path, "required_progress", criterion.required_progress);
    }

    void ValidateCriterion(
        ValidationResult& result,
        const std::string& path,
        const CriterionDefinition& criterion)
    {
        switch (criterion.type) {
        case CriterionType::player_level:
            if (!criterion.level) {
                AddError(result, path + ".level", "is required for player_level");
            }
            else if (*criterion.level == 0 || *criterion.level > 20) {
                AddError(result, path + ".level", "must be between 1 and 20");
            }
            RejectCriterionFields(result, path, criterion, {"level"});
            break;
        case CriterionType::map_loaded:
            RequireText(result, path + ".map", criterion.map);
            RejectCriterionFields(result, path, criterion, {"map"});
            break;
        case CriterionType::mission_complete:
            RequireText(result, path + ".mission", criterion.mission);
            if (!criterion.is_hard_mode) {
                AddError(result, path + ".is_hard_mode", "is required for mission_complete");
            }
            RejectCriterionFields(result, path, criterion, {"mission", "is_hard_mode"});
            break;
        case CriterionType::dungeon_complete:
            RequireText(result, path + ".dungeon", criterion.dungeon);
            if (!criterion.is_hard_mode) {
                AddError(result, path + ".is_hard_mode", "is required for dungeon_complete");
            }
            RejectCriterionFields(result, path, criterion, {"dungeon", "is_hard_mode"});
            break;
        case CriterionType::vanquish_complete:
            RequireText(result, path + ".map", criterion.map);
            RejectCriterionFields(result, path, criterion, {"map"});
            break;
        case CriterionType::title_progress:
            RequireText(result, path + ".title", criterion.title);
            if (criterion.title && !IsBlank(*criterion.title) && !FindTitle(*criterion.title)) {
                AddError(result, path + ".title", "is not a supported TitleID token");
            }
            if (!criterion.required_progress || *criterion.required_progress == 0) {
                AddError(result, path + ".required_progress", "must be greater than zero");
            }
            RejectCriterionFields(result, path, criterion, {"title", "required_progress"});
            break;
        case CriterionType::manual:
            RejectCriterionFields(result, path, criterion, {});
            break;
        case CriterionType::invalid:
        default:
            AddError(result, path + ".type", "is missing or invalid");
            RejectCriterionFields(result, path, criterion, {});
            break;
        }
    }

    void ValidateTitleTracker(
        ValidationResult& result,
        const std::string& path,
        const TitleTrackerDefinition& tracker)
    {
        RequireText(result, path + ".title", tracker.title);
        if (!IsBlank(tracker.title) && !FindTitle(tracker.title)) {
            AddError(result, path + ".title", "is not a supported TitleID token");
        }
    }

    void ValidateAction(
        ValidationResult& result,
        const std::string& path,
        const ActionDefinition& action)
    {
        if (!action.id) {
            AddError(result, path + ".id", "must be greater than zero");
        }
        RequireText(result, path + ".label", action.label);

        const auto reject_except = [&](const std::initializer_list<std::string_view> allowed) {
            const auto is_allowed = [&](const std::string_view field) {
                return std::ranges::find(allowed, field) != allowed.end();
            };
            if (!is_allowed("destination")) RejectField(result, path, "destination", action.destination);
            if (!is_allowed("team_build")) RejectField(result, path, "team_build", action.team_build);
            if (!is_allowed("build")) RejectField(result, path, "build", action.build);
            if (!is_allowed("name")) RejectField(result, path, "name", action.name);
        };

        switch (action.type) {
        case ActionType::travel:
            RequireText(result, path + ".destination", action.destination);
            reject_except({"destination"});
            break;
        case ActionType::player_build:
            RequireText(result, path + ".team_build", action.team_build);
            RequireText(result, path + ".build", action.build);
            reject_except({"team_build", "build"});
            break;
        case ActionType::hero_team_build:
            RequireText(result, path + ".name", action.name);
            reject_except({"name"});
            break;
        case ActionType::invalid:
        default:
            AddError(result, path + ".type", "is missing or invalid");
            reject_except({});
            break;
        }
    }

    void ValidateExperienceTracker(
        ValidationResult& result,
        const std::string& path,
        const ExperienceTrackerDefinition& tracker)
    {
        RequireText(result, path + ".label", tracker.label);
        ValidateCriterion(result, path + ".arm_criterion", tracker.arm_criterion);
        ValidateCriterion(result, path + ".increment_criterion", tracker.increment_criterion);
        if (tracker.arm_criterion.type != CriterionType::map_loaded) {
            AddError(result, path + ".arm_criterion.type", "v1 experience trackers only support map_loaded");
        }
        if (tracker.increment_criterion.type != CriterionType::map_loaded) {
            AddError(result, path + ".increment_criterion.type", "v1 experience trackers only support map_loaded");
        }
        if (!tracker.goal_experience) {
            AddError(result, path + ".goal_experience", "must be greater than zero");
        }
    }

    void ValidateTaskRecord(
        ValidationResult& result,
        const std::string& path,
        const TaskRecord& record,
        const uint64_t elapsed_ms)
    {
        if (!record.id) {
            AddError(result, path + ".id", "must be greater than zero");
        }
        switch (record.result) {
        case TaskResult::completed:
            if (!record.split_ms) {
                AddError(result, path + ".split_ms", "is required for a completed Task");
            }
            if (!record.segment_ms) {
                AddError(result, path + ".segment_ms", "is required for a completed Task");
            }
            break;
        case TaskResult::skipped:
            if (record.split_ms) {
                AddError(result, path + ".split_ms", "must be omitted for a skipped Task");
            }
            if (record.segment_ms) {
                AddError(result, path + ".segment_ms", "must be omitted for a skipped Task");
            }
            break;
        case TaskResult::invalid:
        default:
            AddError(result, path + ".result", "is missing or invalid");
            break;
        }
        if (record.split_ms && *record.split_ms > elapsed_ms) {
            AddError(result, path + ".split_ms", "cannot exceed the attempt elapsed_ms");
        }
        if (record.segment_ms && record.split_ms && *record.segment_ms > *record.split_ms) {
            AddError(result, path + ".segment_ms", "cannot exceed its cumulative split_ms");
        }
    }

    void ValidateTaskRecords(
        ValidationResult& result,
        const std::string& path,
        const std::vector<TaskRecord>& records,
        const uint64_t elapsed_ms)
    {
        std::unordered_set<uint32_t> ids;
        uint64_t previous_split_ms = 0;
        for (size_t i = 0; i < records.size(); ++i) {
            const auto record_path = path + "[" + std::to_string(i) + "]";
            const auto& record = records[i];
            ValidateTaskRecord(result, record_path, record, elapsed_ms);
            if (record.id && !ids.insert(record.id).second) {
                AddError(result, record_path + ".id", "duplicates an earlier Task record");
            }
            if (record.split_ms) {
                if (*record.split_ms < previous_split_ms) {
                    AddError(result, record_path + ".split_ms", "must not be earlier than the previous recorded split");
                }
                previous_split_ms = *record.split_ms;
            }
        }
    }

    bool IsRouteHash(const std::string_view value)
    {
        return value.size() == 16 && std::ranges::all_of(value, [](const unsigned char c) {
            return std::isdigit(c) != 0 || (c >= 'a' && c <= 'f');
        });
    }

    void ValidateDocumentHeader(
        ValidationResult& result,
        const std::string& schema,
        const uint32_t schema_version,
        const std::string_view expected_schema)
    {
        if (schema != expected_schema) {
            AddError(result, "schema", "must be '" + std::string(expected_schema) + "'");
        }
        if (schema_version != SchemaVersion) {
            AddError(result, "schema_version", "must be 1");
        }
    }

    std::string ValidationErrorText(const std::string_view document, const ValidationResult& validation)
    {
        std::string message = std::string(document) + " validation failed:";
        for (const auto& error : validation.errors) {
            message += "\n- " + error;
        }
        return message;
    }

    template <typename T, typename Validator>
    std::expected<T, std::string> ParseDocument(
        const std::string_view json,
        const std::string_view document,
        Validator&& validator)
    {
        if (json.empty()) {
            return std::unexpected(std::string(document) + " JSON is empty");
        }
        T value;
        if (const auto error = glz::read<ReadOptions>(value, json); error) {
            return std::unexpected(std::string(document) + " JSON parse failed: " + glz::format_error(error, json));
        }
        const auto validation = validator(value);
        if (!validation.Ok()) {
            return std::unexpected(ValidationErrorText(document, validation));
        }
        return value;
    }

    template <typename T, typename Validator>
    JsonResult WriteDocument(
        const T& value,
        const std::string_view document,
        Validator&& validator)
    {
        const auto validation = validator(value);
        if (!validation.Ok()) {
            return std::unexpected(ValidationErrorText(document, validation));
        }
        auto json = glz::write<WriteOptions>(value);
        if (!json) {
            return std::unexpected(std::string(document) + " JSON serialization failed: " + glz::format_error(json.error()));
        }
        return std::move(*json);
    }

    class RouteHasher {
    public:
        void AddByte(const uint8_t value)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }

        void AddUint32(const uint32_t value)
        {
            for (auto shift = 0u; shift < 32u; shift += 8u) {
                AddByte(static_cast<uint8_t>((value >> shift) & 0xffu));
            }
        }

        void AddString(const std::string_view value)
        {
            AddUint32(static_cast<uint32_t>(value.size()));
            for (const auto c : value) {
                AddByte(static_cast<uint8_t>(c));
            }
        }

        [[nodiscard]] uint64_t Value() const { return hash; }

    private:
        uint64_t hash = 14695981039346656037ull;
    };

    std::string_view CriterionTypeName(const CriterionType type)
    {
        switch (type) {
        case CriterionType::player_level: return "player_level";
        case CriterionType::map_loaded: return "map_loaded";
        case CriterionType::mission_complete: return "mission_complete";
        case CriterionType::dungeon_complete: return "dungeon_complete";
        case CriterionType::vanquish_complete: return "vanquish_complete";
        case CriterionType::title_progress: return "title_progress";
        case CriterionType::manual: return "manual";
        case CriterionType::invalid:
        default: return "invalid";
        }
    }

    void AddCriterionToHash(RouteHasher& hash, const CriterionDefinition& criterion)
    {
        hash.AddString(CriterionTypeName(criterion.type));
        switch (criterion.type) {
        case CriterionType::player_level:
            hash.AddUint32(criterion.level.value_or(0));
            break;
        case CriterionType::map_loaded:
        case CriterionType::vanquish_complete:
            hash.AddString(criterion.map.value_or(""));
            break;
        case CriterionType::mission_complete:
            hash.AddString(criterion.mission.value_or(""));
            hash.AddByte(criterion.is_hard_mode.value_or(false) ? 1 : 0);
            break;
        case CriterionType::dungeon_complete:
            hash.AddString(criterion.dungeon.value_or(""));
            hash.AddByte(criterion.is_hard_mode.value_or(false) ? 1 : 0);
            break;
        case CriterionType::title_progress:
            hash.AddString(criterion.title.value_or(""));
            hash.AddUint32(criterion.required_progress.value_or(0));
            break;
        case CriterionType::manual:
        case CriterionType::invalid:
        default:
            break;
        }
    }

    std::string HexHash(const uint64_t value)
    {
        constexpr char HexDigits[] = "0123456789abcdef";
        std::string out(16, '0');
        for (size_t i = 0; i < out.size(); ++i) {
            const auto shift = static_cast<unsigned>((out.size() - i - 1) * 4);
            out[i] = HexDigits[(value >> shift) & 0xfu];
        }
        return out;
    }
}

namespace TrackerAdvanced {
    ProfileResult ParseProfile(const std::string_view json)
    {
        return ParseDocument<ProfileDefinition>(json, "Profile", ValidateProfile);
    }

    JsonResult WriteProfile(const ProfileDefinition& profile)
    {
        return WriteDocument(profile, "Profile", ValidateProfile);
    }

    HistoryResult ParseHistory(const std::string_view json)
    {
        return ParseDocument<HistoryFile>(json, "History", ValidateHistory);
    }

    JsonResult WriteHistory(const HistoryFile& history)
    {
        return WriteDocument(history, "History", ValidateHistory);
    }

    ActiveRunResult ParseActiveRun(const std::string_view json)
    {
        return ParseDocument<ActiveRunFile>(json, "Active run", ValidateActiveRun);
    }

    JsonResult WriteActiveRun(const ActiveRunFile& active_run)
    {
        return WriteDocument(active_run, "Active run", ValidateActiveRun);
    }

    ValidationResult ValidateProfile(const ProfileDefinition& profile)
    {
        ValidationResult result;
        ValidateDocumentHeader(result, profile.schema, profile.schema_version, ProfileSchema);
        RequireText(result, "id", profile.id);
        if (!IsBlank(profile.id) && !IsProfileId(profile.id)) {
            AddError(
                result,
                "id",
                "must be a lowercase ASCII slug using letters, numbers, '-' or '_'");
        }
        if (profile.id.size() > 64) {
            AddError(result, "id", "must contain at most 64 characters");
        }
        if (IsWindowsReservedStem(profile.id)) {
            AddError(result, "id", "must not use a reserved Windows device name");
        }
        RequireText(result, "name", profile.name);
        if (profile.description && HasOuterWhitespace(*profile.description)) {
            AddWarning(result, "description", "has leading or trailing whitespace");
        }
        if (profile.objectives.empty()) {
            AddError(result, "objectives", "must contain at least one Objective");
        }

        std::unordered_set<std::string> root_titles;
        for (size_t i = 0; i < profile.title_trackers.size(); ++i) {
            const auto path = "title_trackers[" + std::to_string(i) + "]";
            const auto& tracker = profile.title_trackers[i];
            ValidateTitleTracker(result, path, tracker);
            if (!tracker.title.empty() && !root_titles.insert(tracker.title).second) {
                AddWarning(result, path + ".title", "duplicates an earlier profile title tracker");
            }
        }

        std::unordered_set<uint32_t> action_ids;
        std::unordered_map<uint32_t, size_t> action_indices;
        for (size_t i = 0; i < profile.actions.size(); ++i) {
            const auto path = "actions[" + std::to_string(i) + "]";
            const auto& action = profile.actions[i];
            ValidateAction(result, path, action);
            if (action.id && !action_ids.insert(action.id).second) {
                AddError(result, path + ".id", "duplicates an earlier Action id");
            }
            else if (action.id) {
                action_indices.emplace(action.id, i);
            }
        }

        std::unordered_set<uint32_t> objective_ids;
        std::unordered_set<uint32_t> task_ids;
        std::unordered_set<uint32_t> referenced_action_ids;
        for (size_t objective_index = 0; objective_index < profile.objectives.size(); ++objective_index) {
            const auto objective_path = "objectives[" + std::to_string(objective_index) + "]";
            const auto& objective = profile.objectives[objective_index];
            if (!objective.id) {
                AddError(result, objective_path + ".id", "must be greater than zero");
            }
            else if (!objective_ids.insert(objective.id).second) {
                AddError(result, objective_path + ".id", "duplicates an earlier Objective id");
            }
            RequireText(result, objective_path + ".name", objective.name);
            if (objective.tasks.empty()) {
                AddError(result, objective_path + ".tasks", "must contain at least one Task");
            }

            std::unordered_set<std::string> objective_titles;
            for (size_t title_index = 0; title_index < objective.title_trackers.size(); ++title_index) {
                const auto title_path = objective_path + ".title_trackers[" + std::to_string(title_index) + "]";
                const auto& tracker = objective.title_trackers[title_index];
                ValidateTitleTracker(result, title_path, tracker);
                if (!tracker.title.empty() && !objective_titles.insert(tracker.title).second) {
                    AddWarning(result, title_path + ".title", "duplicates an earlier Objective title tracker");
                }
            }

            for (size_t task_index = 0; task_index < objective.tasks.size(); ++task_index) {
                const auto task_path = objective_path + ".tasks[" + std::to_string(task_index) + "]";
                const auto& task = objective.tasks[task_index];
                if (!task.id) {
                    AddError(result, task_path + ".id", "must be greater than zero");
                }
                else if (!task_ids.insert(task.id).second) {
                    AddError(result, task_path + ".id", "duplicates an earlier Task id");
                }
                RequireText(result, task_path + ".name", task.name);
                if (!task.expected_time_ms) {
                    AddWarning(result, task_path + ".expected_time_ms", "is zero; comparisons will show no expected time");
                }
                ValidateCriterion(result, task_path + ".end_criterion", task.end_criterion);

                std::unordered_set<uint32_t> task_action_ids;
                for (size_t action_index = 0; action_index < task.action_ids.size(); ++action_index) {
                    const auto action_path = task_path + ".action_ids[" + std::to_string(action_index) + "]";
                    const auto action_id = task.action_ids[action_index];
                    if (!action_indices.contains(action_id)) {
                        AddError(result, action_path, "references unknown Action id " + std::to_string(action_id));
                    }
                    if (!task_action_ids.insert(action_id).second) {
                        AddWarning(result, action_path, "duplicates an earlier Action reference on this Task");
                    }
                    referenced_action_ids.insert(action_id);
                }

                for (size_t note_index = 0; note_index < task.notes.size(); ++note_index) {
                    if (IsBlank(task.notes[note_index])) {
                        AddWarning(
                            result,
                            task_path + ".notes[" + std::to_string(note_index) + "]",
                            "is blank and can be removed");
                    }
                }
                if (task.experience_tracker) {
                    ValidateExperienceTracker(result, task_path + ".experience_tracker", *task.experience_tracker);
                }
            }
        }

        for (size_t i = 0; i < profile.actions.size(); ++i) {
            if (profile.actions[i].id && !referenced_action_ids.contains(profile.actions[i].id)) {
                AddWarning(
                    result,
                    "actions[" + std::to_string(i) + "].id",
                    "is never referenced by a Task");
            }
        }
        return result;
    }

    ValidationResult ValidateHistory(const HistoryFile& history)
    {
        ValidationResult result;
        ValidateDocumentHeader(result, history.schema, history.schema_version, HistorySchema);
        RequireText(result, "profile_id", history.profile_id);

        std::unordered_set<std::string> attempt_ids;
        for (size_t i = 0; i < history.attempts.size(); ++i) {
            const auto path = "attempts[" + std::to_string(i) + "]";
            const auto& attempt = history.attempts[i];
            RequireText(result, path + ".id", attempt.id);
            if (!attempt.id.empty() && !attempt_ids.insert(attempt.id).second) {
                AddError(result, path + ".id", "duplicates an earlier Attempt id");
            }
            if (!IsRouteHash(attempt.route_hash)) {
                AddError(result, path + ".route_hash", "must be a 16-character lowercase hexadecimal v1 route hash");
            }
            RequireText(result, path + ".character_name", attempt.character_name);
            RequireText(result, path + ".started_at", attempt.started_at);
            RequireText(result, path + ".ended_at", attempt.ended_at);
            if (attempt.result == AttemptResult::invalid) {
                AddError(result, path + ".result", "is missing or invalid");
            }
            ValidateTaskRecords(result, path + ".tasks", attempt.tasks, attempt.elapsed_ms);
            if (attempt.result == AttemptResult::completed && attempt.tasks.empty()) {
                AddError(result, path + ".tasks", "a completed Attempt must contain resolved Tasks");
            }
        }
        return result;
    }

    ValidationResult ValidateActiveRun(const ActiveRunFile& active_run)
    {
        ValidationResult result;
        ValidateDocumentHeader(result, active_run.schema, active_run.schema_version, ActiveRunSchema);
        RequireText(result, "profile_id", active_run.profile_id);
        if (!IsRouteHash(active_run.route_hash)) {
            AddError(result, "route_hash", "must be a 16-character lowercase hexadecimal v1 route hash");
        }
        RequireText(result, "attempt_id", active_run.attempt_id);
        RequireText(result, "character_name", active_run.character_name);
        RequireText(result, "started_at", active_run.started_at);
        RequireText(result, "updated_at", active_run.updated_at);
        if (!active_run.active_task_id) {
            AddError(result, "active_task_id", "must be greater than zero");
        }
        if (active_run.state == ActiveRunStatus::invalid) {
            AddError(result, "state", "is missing or invalid");
        }
        if (active_run.state == ActiveRunStatus::paused) {
            if (!active_run.pause_reason || *active_run.pause_reason == PauseReason::invalid) {
                AddError(result, "pause_reason", "is required while state is paused");
            }
        }
        else if (active_run.pause_reason) {
            AddError(result, "pause_reason", "must be omitted unless state is paused");
        }

        ValidateTaskRecords(result, "resolved_tasks", active_run.resolved_tasks, active_run.elapsed_ms);
        if (active_run.last_resolved_elapsed_ms > active_run.elapsed_ms) {
            AddError(
                result,
                "last_resolved_elapsed_ms",
                "cannot exceed the active run elapsed_ms");
        }
        if (
            active_run.resolved_tasks.empty()
            && active_run.last_resolved_elapsed_ms != 0) {
            AddError(
                result,
                "last_resolved_elapsed_ms",
                "must be zero when no Tasks have resolved");
        }
        const auto last_split = std::ranges::find_if(
            active_run.resolved_tasks.rbegin(),
            active_run.resolved_tasks.rend(),
            [](const TaskRecord& record) { return record.split_ms.has_value(); });
        if (
            last_split != active_run.resolved_tasks.rend()
            && active_run.last_resolved_elapsed_ms < *last_split->split_ms) {
            AddError(
                result,
                "last_resolved_elapsed_ms",
                "cannot be earlier than the most recent cumulative split_ms");
        }
        if (std::ranges::any_of(active_run.resolved_tasks, [&](const TaskRecord& record) {
                return record.id == active_run.active_task_id;
            })) {
            AddError(result, "active_task_id", "cannot also appear in resolved_tasks");
        }
        if (active_run.experience_tracker) {
            const auto& tracker = *active_run.experience_tracker;
            if (!tracker.task_id) {
                AddError(result, "experience_tracker.task_id", "must be greater than zero");
            }
            else if (tracker.task_id != active_run.active_task_id) {
                AddError(result, "experience_tracker.task_id", "must match active_task_id");
            }
            if (!tracker.armed
                && (tracker.experience_gained || tracker.runs || tracker.elapsed_ms)) {
                AddWarning(
                    result,
                    "experience_tracker",
                    "contains measurements even though the tracker is not armed");
            }
        }
        return result;
    }

    std::string ComputeRouteHash(const ProfileDefinition& profile)
    {
        RouteHasher hash;
        hash.AddString("trackeradvanced.route.v1");
        const auto tasks = FlattenTasks(profile);
        hash.AddUint32(static_cast<uint32_t>(tasks.size()));
        for (const auto& view : tasks) {
            hash.AddUint32(view.task->id);
            AddCriterionToHash(hash, view.task->end_criterion);
            hash.AddByte(view.task->experience_tracker ? 1 : 0);
            if (view.task->experience_tracker) {
                AddCriterionToHash(hash, view.task->experience_tracker->arm_criterion);
                AddCriterionToHash(hash, view.task->experience_tracker->increment_criterion);
                hash.AddUint32(view.task->experience_tracker->goal_experience);
            }
        }
        return HexHash(hash.Value());
    }

    std::vector<FlatTaskView> FlattenTasks(const ProfileDefinition& profile)
    {
        size_t count = 0;
        for (const auto& objective : profile.objectives) {
            count += objective.tasks.size();
        }

        std::vector<FlatTaskView> tasks;
        tasks.reserve(count);
        for (size_t objective_index = 0; objective_index < profile.objectives.size(); ++objective_index) {
            const auto& objective = profile.objectives[objective_index];
            for (size_t task_index = 0; task_index < objective.tasks.size(); ++task_index) {
                tasks.push_back({
                    .objective = &objective,
                    .task = &objective.tasks[task_index],
                    .objective_index = objective_index,
                    .task_index = task_index,
                    .flat_index = tasks.size(),
                });
            }
        }
        return tasks;
    }

    std::span<const SupportedTitle> SupportedTitles()
    {
        return SupportedTitleEntries;
    }

    const SupportedTitle* FindTitle(const std::string_view token)
    {
        const auto found = std::ranges::find(SupportedTitleEntries, token, &SupportedTitle::token);
        return found == SupportedTitleEntries.end() ? nullptr : &*found;
    }
}
