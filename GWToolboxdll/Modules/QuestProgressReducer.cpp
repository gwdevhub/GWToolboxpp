#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>

#include <algorithm>
#include <sstream>
#include <string_view>

namespace QuestProgress {
namespace {

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t Fnv1a64(const void* data, size_t size, uint64_t hash = kFnvOffset)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t Fnv1a64String(std::string_view s, uint64_t hash = kFnvOffset)
{
    return Fnv1a64(s.data(), s.size(), hash);
}

std::string ToHex64(uint64_t value)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<size_t>(i)] = kHex[value & 0xfu];
        value >>= 4;
    }
    return out;
}

bool HasSemanticKey(const QuestProgress& quest, const std::string& key)
{
    for (const auto& event : quest.history) {
        if (event.semantic_event_key == key) {
            return true;
        }
    }
    return false;
}

bool IsIsoOlder(const std::string& a, const std::string& b)
{
    // Lexicographic compare is valid only for canonical UTC ISO-8601 (validated upstream).
    if (a.empty() || b.empty()) {
        return false;
    }
    return a < b;
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

void AppendDiag(ReducerOutput& out, std::string message)
{
    out.diagnostics.messages.push_back(std::move(message));
}

QuestHistoryEvent MakeEvent(
    uint32_t quest_id,
    HistoryEventType type,
    ProgressState state,
    ProgressSource source,
    Confidence confidence,
    const std::string& observed_at,
    const std::vector<ObjectiveObservation>& objectives,
    EvidenceKind evidence_kind)
{
    QuestHistoryEvent event;
    event.game_quest_id = quest_id;
    event.event_type = type;
    event.state = state;
    event.source = source;
    event.confidence = confidence;
    event.observed_at = observed_at;
    event.objectives = objectives;
    event.evidence_kind = evidence_kind;
    event.semantic_event_key = BuildSemanticEventKey(
        quest_id, type, state, source, confidence, objectives, evidence_kind);
    return event;
}

bool TryAppend(
    QuestProgress& quest,
    ReducerOutput& out,
    QuestHistoryEvent event)
{
    if (IsReservedState(event.state)) {
        out.diagnostics.reserved_state_attempt = true;
        AppendDiag(out, "rejected reserved state in history append");
        return false;
    }
    if (HasSemanticKey(quest, event.semantic_event_key)) {
        return false;
    }
    quest.history.push_back(event);
    out.appended.push_back(event);
    out.semantic_changed = true;
    return true;
}

void ApplyPresenceLost(
    QuestProgress& quest,
    ReducerOutput& out,
    uint32_t quest_id,
    const std::string& observed_at)
{
    quest.state = ProgressState::Unknown;
    quest.source = ProgressSource::GameSnapshot;
    quest.confidence = Confidence::Uncertain;
    quest.last_observed_at = observed_at;
    quest.completed_at.reset();
    auto event = MakeEvent(
        quest_id,
        HistoryEventType::PresenceLost,
        ProgressState::Unknown,
        ProgressSource::GameSnapshot,
        Confidence::Uncertain,
        observed_at,
        quest.objectives,
        EvidenceKind::None);
    TryAppend(quest, out, std::move(event));
}

void ApplyPresentQuest(
    CharacterProgress& next,
    ReducerOutput& out,
    const LiveQuestObservation& obs,
    const std::string& observed_at)
{
    auto objectives = obs.objectives;
    bool duplicate_indices = false;
    NormalizeObjectives(objectives, &duplicate_indices);
    if (duplicate_indices) {
        out.diagnostics.duplicate_objective_indices = true;
        AppendDiag(out, "duplicate objective indices canonicalized deterministically");
    }
    const auto state = DerivePresentState(obs);

    auto it = next.quests.find(obs.game_quest_id);
    if (it == next.quests.end()) {
        QuestProgress quest;
        quest.game_quest_id = obs.game_quest_id;
        quest.state = state;
        quest.source = ProgressSource::GameSnapshot;
        quest.confidence = Confidence::Confirmed;
        quest.first_observed_at = observed_at;
        quest.last_observed_at = observed_at;
        quest.objectives = objectives;
        auto event = MakeEvent(
            obs.game_quest_id,
            HistoryEventType::Observation,
            state,
            ProgressSource::GameSnapshot,
            Confidence::Confirmed,
            observed_at,
            objectives,
            EvidenceKind::None);
        TryAppend(quest, out, std::move(event));
        next.quests.emplace(obs.game_quest_id, std::move(quest));
        return;
    }

    auto& quest = it->second;
    const bool state_changed = quest.state != state;
    const bool objectives_changed = [&]() {
        if (quest.objectives.size() != objectives.size()) {
            return true;
        }
        for (size_t i = 0; i < objectives.size(); ++i) {
            if (quest.objectives[i].index != objectives[i].index
                || quest.objectives[i].completed != objectives[i].completed
                || quest.objectives[i].content_fingerprint != objectives[i].content_fingerprint) {
                return true;
            }
        }
        return false;
    }();

    if (!state_changed && !objectives_changed) {
        quest.last_observed_at = observed_at;
        out.touch_last_observed = true;
        return;
    }

    quest.state = state;
    quest.source = ProgressSource::GameSnapshot;
    quest.confidence = Confidence::Confirmed;
    quest.last_observed_at = observed_at;
    quest.objectives = objectives;
    // Re-observe after terminal outcomes clears completedAt when back in log.
    if (state != ProgressState::CompletedObserved) {
        quest.completed_at.reset();
    }

    auto event = MakeEvent(
        obs.game_quest_id,
        HistoryEventType::Observation,
        state,
        ProgressSource::GameSnapshot,
        Confidence::Confirmed,
        observed_at,
        objectives,
        EvidenceKind::None);
    TryAppend(quest, out, std::move(event));
}

void ApplyPresentQuestSideEvidence(
    CharacterProgress& next,
    ReducerOutput& out,
    uint32_t quest_id,
    const std::vector<QuestEvidence>& evidence,
    const std::string& observed_at)
{
    auto it = next.quests.find(quest_id);
    if (it == next.quests.end()) {
        return;
    }
    auto& quest = it->second;

    bool saw_accepted = false;
    bool saw_chat_updated = false;
    for (const auto& item : evidence) {
        if (item.game_quest_id != quest_id) {
            continue;
        }
        if (item.kind == EvidenceKind::Accepted) {
            saw_accepted = true;
        }
        else if (item.kind == EvidenceKind::ChatUpdated) {
            saw_chat_updated = true;
        }
    }

    if (saw_accepted && !quest.accepted_at.has_value()) {
        quest.accepted_at = observed_at;
        auto event = MakeEvent(
            quest_id,
            HistoryEventType::Observation,
            ProgressState::Active,
            ProgressSource::GameEvent,
            Confidence::Confirmed,
            observed_at,
            quest.objectives,
            EvidenceKind::Accepted);
        TryAppend(quest, out, std::move(event));
    }

    if (saw_chat_updated
        && quest.state != ProgressState::ReadyForReward
        && quest.state != ProgressState::CompletedObserved
        && quest.state != ProgressState::AbandonedObserved) {
        const auto state = quest.state == ProgressState::Unknown ? ProgressState::Active : quest.state;
        auto event = MakeEvent(
            quest_id,
            HistoryEventType::Observation,
            state,
            ProgressSource::GameEvent,
            Confidence::Confirmed,
            observed_at,
            quest.objectives,
            EvidenceKind::ChatUpdated);
        TryAppend(quest, out, std::move(event));
    }
}

void ApplyMissingQuest(
    CharacterProgress& next,
    ReducerOutput& out,
    uint32_t quest_id,
    const std::vector<QuestEvidence>& evidence,
    const std::string& observed_at)
{
    auto it = next.quests.find(quest_id);
    if (it == next.quests.end()) {
        return;
    }
    auto& quest = it->second;

    // Exact evidence is evaluated before the repeated presence-loss short-circuit so
    // late abandon/reward in a later Reduce can upgrade unknown/uncertain.
    const auto resolution = ResolveEvidenceForQuest(evidence, quest_id);

    if (resolution == EvidenceResolution::Conflict) {
        out.diagnostics.conflicting_evidence = true;
        AppendDiag(out, "conflicting actionable evidence; refusing abandon/complete inference");
        if (quest.state == ProgressState::Unknown
            && quest.confidence == Confidence::Uncertain
            && HasSemanticKey(
                   quest,
                   BuildSemanticEventKey(
                       quest_id,
                       HistoryEventType::PresenceLost,
                       ProgressState::Unknown,
                       ProgressSource::GameSnapshot,
                       Confidence::Uncertain,
                       quest.objectives,
                       EvidenceKind::None))) {
            quest.last_observed_at = observed_at;
            out.touch_last_observed = true;
            return;
        }
        ApplyPresenceLost(quest, out, quest_id, observed_at);
        return;
    }

    if (resolution == EvidenceResolution::Abandon) {
        if (quest.state == ProgressState::AbandonedObserved) {
            auto event = MakeEvent(
                quest_id,
                HistoryEventType::Abandoned,
                ProgressState::AbandonedObserved,
                ProgressSource::GameEvent,
                Confidence::Probable,
                observed_at,
                quest.objectives,
                EvidenceKind::Abandon);
            if (!TryAppend(quest, out, std::move(event))) {
                quest.last_observed_at = observed_at;
                out.touch_last_observed = true;
            }
            return;
        }
        if (quest.state == ProgressState::CompletedObserved) {
            AppendDiag(out, "abandon evidence ignored for completed_observed quest");
            quest.last_observed_at = observed_at;
            out.touch_last_observed = true;
            return;
        }
        quest.state = ProgressState::AbandonedObserved;
        quest.source = ProgressSource::GameEvent;
        quest.confidence = Confidence::Probable;
        quest.last_observed_at = observed_at;
        quest.completed_at.reset();
        auto event = MakeEvent(
            quest_id,
            HistoryEventType::Abandoned,
            ProgressState::AbandonedObserved,
            ProgressSource::GameEvent,
            Confidence::Probable,
            observed_at,
            quest.objectives,
            EvidenceKind::Abandon);
        if (!TryAppend(quest, out, std::move(event))) {
            out.touch_last_observed = true;
        }
        return;
    }

    if (resolution == EvidenceResolution::Reward) {
        if (quest.state == ProgressState::CompletedObserved) {
            auto event = MakeEvent(
                quest_id,
                HistoryEventType::Completed,
                ProgressState::CompletedObserved,
                ProgressSource::GameEvent,
                Confidence::Probable,
                observed_at,
                quest.objectives,
                EvidenceKind::Reward);
            if (!TryAppend(quest, out, std::move(event))) {
                quest.last_observed_at = observed_at;
                out.touch_last_observed = true;
            }
            return;
        }
        if (quest.state == ProgressState::AbandonedObserved) {
            AppendDiag(out, "reward evidence ignored for abandoned_observed quest");
            quest.last_observed_at = observed_at;
            out.touch_last_observed = true;
            return;
        }
        // Same-tick: still ready_for_reward. Late: unknown after presence_lost, history showed ready.
        const bool ready_now = quest.state == ProgressState::ReadyForReward;
        const bool late_from_unknown =
            quest.state == ProgressState::Unknown && HistoryShowsReadyForReward(quest);
        if (ready_now || late_from_unknown) {
            quest.state = ProgressState::CompletedObserved;
            quest.source = ProgressSource::GameEvent;
            quest.confidence = Confidence::Probable;
            quest.last_observed_at = observed_at;
            quest.completed_at = observed_at;
            auto event = MakeEvent(
                quest_id,
                HistoryEventType::Completed,
                ProgressState::CompletedObserved,
                ProgressSource::GameEvent,
                Confidence::Probable,
                observed_at,
                quest.objectives,
                EvidenceKind::Reward);
            if (!TryAppend(quest, out, std::move(event))) {
                out.touch_last_observed = true;
            }
            return;
        }
        AppendDiag(out, "reward evidence ignored without ready_for_reward prior progress");
        // Fall through to unknown / presence-lost path.
    }

    // Enquire-only is non-actionable (ResolveEvidence returns None).
    if (quest.state == ProgressState::AbandonedObserved
        || quest.state == ProgressState::CompletedObserved) {
        // Terminal observed states survive repeated absence without new evidence.
        quest.last_observed_at = observed_at;
        out.touch_last_observed = true;
        return;
    }

    if (quest.state == ProgressState::Unknown
        && quest.confidence == Confidence::Uncertain) {
        const auto key = BuildSemanticEventKey(
            quest_id,
            HistoryEventType::PresenceLost,
            ProgressState::Unknown,
            ProgressSource::GameSnapshot,
            Confidence::Uncertain,
            quest.objectives,
            EvidenceKind::None);
        if (HasSemanticKey(quest, key)) {
            quest.last_observed_at = observed_at;
            out.touch_last_observed = true;
            return;
        }
    }

    ApplyPresenceLost(quest, out, quest_id, observed_at);
}

} // namespace

const char* ToString(ProgressState state)
{
    switch (state) {
        case ProgressState::Unknown: return "unknown";
        case ProgressState::Available: return "available";
        case ProgressState::Active: return "active";
        case ProgressState::ObjectiveProgress: return "objective_progress";
        case ProgressState::ReadyForReward: return "ready_for_reward";
        case ProgressState::CompletedObserved: return "completed_observed";
        case ProgressState::CompletedManual: return "completed_manual";
        case ProgressState::AbandonedObserved: return "abandoned_observed";
    }
    return "unknown";
}

const char* ToString(ProgressSource source)
{
    switch (source) {
        case ProgressSource::GameSnapshot: return "game_snapshot";
        case ProgressSource::GameEvent: return "game_event";
        case ProgressSource::MissionCompletionData: return "mission_completion_data";
        case ProgressSource::ToolboxExistingData: return "toolbox_existing_data";
        case ProgressSource::ManualUserInput: return "manual_user_input";
        case ProgressSource::ImportedHistory: return "imported_history";
        case ProgressSource::Migration: return "migration";
    }
    return "game_snapshot";
}

const char* ToString(Confidence confidence)
{
    switch (confidence) {
        case Confidence::Confirmed: return "confirmed";
        case Confidence::Probable: return "probable";
        case Confidence::Uncertain: return "uncertain";
        case Confidence::Manual: return "manual";
    }
    return "uncertain";
}

const char* ToString(HistoryEventType type)
{
    switch (type) {
        case HistoryEventType::Observation: return "observation";
        case HistoryEventType::PresenceLost: return "presence_lost";
        case HistoryEventType::Abandoned: return "abandoned";
        case HistoryEventType::Completed: return "completed";
    }
    return "observation";
}

const char* ToString(EvidenceKind kind)
{
    switch (kind) {
        case EvidenceKind::None: return "none";
        case EvidenceKind::Abandon: return "abandon";
        case EvidenceKind::Reward: return "reward";
        case EvidenceKind::EnquireReward: return "enquire_reward";
        case EvidenceKind::Accepted: return "accepted";
        case EvidenceKind::ChatReward: return "chat_reward";
        case EvidenceKind::ChatUpdated: return "chat_updated";
    }
    return "none";
}

bool IsReservedState(ProgressState state)
{
    return state == ProgressState::Available || state == ProgressState::CompletedManual;
}

bool IsSyntheticQuestId(uint32_t game_quest_id)
{
    return game_quest_id == kSyntheticCustomMarkerQuestId;
}

bool IsCanonicalUtcTimestamp(std::string_view timestamp)
{
    // YYYY-MM-DDTHH:MM:SS.sssZ — fixed-width, Z suffix, no locale parsing.
    if (timestamp.size() != 24) {
        return false;
    }
    const auto at = [&](size_t i) { return timestamp[i]; };
    for (size_t i : {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 22}) {
        if (!IsDigit(at(i))) {
            return false;
        }
    }
    return at(4) == '-' && at(7) == '-' && at(10) == 'T'
        && at(13) == ':' && at(16) == ':' && at(19) == '.'
        && at(23) == 'Z';
}

std::string FingerprintEncodedContent(std::u16string_view encoded)
{
    // Strip trailing NULs so padding differences do not churn identity.
    while (!encoded.empty() && encoded.back() == u'\0') {
        encoded.remove_suffix(1);
    }
    const auto hash = Fnv1a64(encoded.data(), encoded.size() * sizeof(char16_t));
    return ToHex64(hash);
}

void NormalizeObjectives(
    std::vector<ObjectiveObservation>& objectives,
    bool* duplicate_indices_out)
{
    for (auto& objective : objectives) {
        objective.content_fingerprint = FingerprintEncodedContent(objective.encoded_content);
    }
    std::sort(objectives.begin(), objectives.end(),
        [](const ObjectiveObservation& a, const ObjectiveObservation& b) {
            if (a.index != b.index) {
                return a.index < b.index;
            }
            if (a.completed != b.completed) {
                return !a.completed && b.completed;
            }
            return a.content_fingerprint < b.content_fingerprint;
        });

    bool duplicates = false;
    for (size_t i = 1; i < objectives.size(); ++i) {
        if (objectives[i].index == objectives[i - 1].index) {
            duplicates = true;
            break;
        }
    }
    if (duplicate_indices_out) {
        *duplicate_indices_out = duplicates;
    }
}

std::string BuildSemanticEventKey(
    uint32_t game_quest_id,
    HistoryEventType event_type,
    ProgressState state,
    ProgressSource source,
    Confidence confidence,
    const std::vector<ObjectiveObservation>& objectives,
    EvidenceKind evidence_kind)
{
    auto normalized = objectives;
    NormalizeObjectives(normalized);

    std::ostringstream oss;
    oss << "v1"
        << "|qid=" << game_quest_id
        << "|evt=" << ToString(event_type)
        << "|state=" << ToString(state)
        << "|src=" << ToString(source)
        << "|conf=" << ToString(confidence)
        << "|ev=" << ToString(evidence_kind)
        << "|obj=";
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (i != 0) {
            oss << ',';
        }
        oss << normalized[i].index << ':'
            << (normalized[i].completed ? '1' : '0') << ':'
            << normalized[i].content_fingerprint;
    }

    const auto canonical = oss.str();
    return ToHex64(Fnv1a64String(canonical));
}

ProgressState DerivePresentState(const LiveQuestObservation& obs)
{
    if (obs.in_log_completed) {
        return ProgressState::ReadyForReward;
    }
    if (!obs.objectives_missing && !obs.objectives.empty()) {
        return ProgressState::ObjectiveProgress;
    }
    return ProgressState::Active;
}

EvidenceResolution ResolveEvidenceForQuest(
    const std::vector<QuestEvidence>& evidence,
    uint32_t quest_id)
{
    bool saw_abandon = false;
    bool saw_reward = false;
    for (const auto& item : evidence) {
        if (item.game_quest_id != quest_id) {
            continue;
        }
        if (item.kind == EvidenceKind::Abandon) {
            saw_abandon = true;
        }
        else if (item.kind == EvidenceKind::Reward || item.kind == EvidenceKind::ChatReward) {
            saw_reward = true;
        }
        // Accepted, ChatUpdated, and EnquireReward are handled on present quests or ignored here.
    }
    if (saw_abandon && saw_reward) {
        return EvidenceResolution::Conflict;
    }
    if (saw_abandon) {
        return EvidenceResolution::Abandon;
    }
    if (saw_reward) {
        return EvidenceResolution::Reward;
    }
    return EvidenceResolution::None;
}

bool HistoryShowsReadyForReward(const QuestProgress& quest)
{
    if (quest.state == ProgressState::ReadyForReward) {
        return true;
    }
    for (const auto& event : quest.history) {
        if (event.state == ProgressState::ReadyForReward) {
            return true;
        }
    }
    return false;
}

ReducerOutput Reduce(const ReducerInput& input)
{
    ReducerOutput out;
    out.next = input.previous;
    if (!input.previous.character_key.empty()) {
        out.next.character_key = input.previous.character_key;
    }

    if (!IsCanonicalUtcTimestamp(input.observed_at_utc)) {
        out.diagnostics.invalid_timestamp = true;
        AppendDiag(out, "rejected non-canonical UTC timestamp; history not modified");
        return out;
    }

    if (!input.previous.last_reduced_at.empty()
        && !IsCanonicalUtcTimestamp(input.previous.last_reduced_at)) {
        out.diagnostics.invalid_timestamp = true;
        AppendDiag(out, "previous last_reduced_at is non-canonical; refusing reduce");
        return out;
    }

    if (input.treat_as_stale_if_older
        && !input.previous.last_reduced_at.empty()
        && IsIsoOlder(input.observed_at_utc, input.previous.last_reduced_at)) {
        out.diagnostics.rejected_stale = true;
        AppendDiag(out, "rejected stale observation; history not reordered");
        return out;
    }

    std::map<uint32_t, LiveQuestObservation> present;
    for (const auto& obs : input.observed_quests) {
        if (IsSyntheticQuestId(obs.game_quest_id)) {
            AppendDiag(out, "ignored synthetic quest id 0xfdd");
            continue;
        }
        present[obs.game_quest_id] = obs;
    }

    for (const auto& [quest_id, obs] : present) {
        (void)obs;
        ApplyPresentQuest(out.next, out, obs, input.observed_at_utc);
        ApplyPresentQuestSideEvidence(out.next, out, quest_id, input.evidence, input.observed_at_utc);
    }

    std::vector<uint32_t> missing;
    for (const auto& [quest_id, quest] : out.next.quests) {
        (void)quest;
        if (present.find(quest_id) == present.end()) {
            missing.push_back(quest_id);
        }
    }
    for (const auto quest_id : missing) {
        ApplyMissingQuest(out.next, out, quest_id, input.evidence, input.observed_at_utc);
    }

    // Advisory only — missing quests already use the same safe unknown policy.
    if (input.session_gap) {
        AppendDiag(out, "session_gap: missing quests resolved without fabricating completion");
    }

    for (auto& [quest_id, quest] : out.next.quests) {
        (void)quest_id;
        if (IsReservedState(quest.state)) {
            out.diagnostics.reserved_state_attempt = true;
            AppendDiag(out, "sanitized reserved state to unknown");
            quest.state = ProgressState::Unknown;
            quest.confidence = Confidence::Uncertain;
        }
    }

    if (!out.diagnostics.rejected_stale && !out.diagnostics.invalid_timestamp) {
        out.next.last_reduced_at = input.observed_at_utc;
    }

    return out;
}

} // namespace QuestProgress
