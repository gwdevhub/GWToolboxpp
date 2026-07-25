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

EvidenceKind FindEvidence(const std::vector<QuestEvidence>& evidence, uint32_t quest_id)
{
    for (const auto& item : evidence) {
        if (item.game_quest_id == quest_id) {
            return item.kind;
        }
    }
    return EvidenceKind::None;
}

bool IsIsoOlder(const std::string& a, const std::string& b)
{
    // Lexicographic compare is valid for zero-padded UTC ISO-8601 used by callers/tests.
    if (a.empty() || b.empty()) {
        return false;
    }
    return a < b;
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

ProgressState PresentStateOrActive(const LiveQuestObservation& obs)
{
    return DerivePresentState(obs);
}

void ApplyPresentQuest(
    CharacterProgress& next,
    ReducerOutput& out,
    const LiveQuestObservation& obs,
    const std::string& observed_at)
{
    auto objectives = obs.objectives;
    NormalizeObjectives(objectives);
    const auto state = PresentStateOrActive(obs);

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

    // Already stably unknown from a prior presence-loss — timestamp touch only.
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

    const auto kind = FindEvidence(evidence, quest_id);

    if (kind == EvidenceKind::Abandon) {
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
        TryAppend(quest, out, std::move(event));
        return;
    }

    if (kind == EvidenceKind::Reward && quest.state == ProgressState::ReadyForReward) {
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
        TryAppend(quest, out, std::move(event));
        return;
    }

    // EnquireReward alone, mismatched reward (not ready), or no evidence → unknown.
    if (kind == EvidenceKind::EnquireReward) {
        AppendDiag(out, "enquire_reward alone is not turn-in evidence");
    }
    else if (kind == EvidenceKind::Reward) {
        AppendDiag(out, "reward evidence ignored without ready_for_reward prior state");
    }

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

std::string FingerprintEncodedContent(std::u16string_view encoded)
{
    // Strip trailing NULs so padding differences do not churn identity.
    while (!encoded.empty() && encoded.back() == u'\0') {
        encoded.remove_suffix(1);
    }
    const auto hash = Fnv1a64(encoded.data(), encoded.size() * sizeof(char16_t));
    return ToHex64(hash);
}

void NormalizeObjectives(std::vector<ObjectiveObservation>& objectives)
{
    std::sort(objectives.begin(), objectives.end(),
        [](const ObjectiveObservation& a, const ObjectiveObservation& b) {
            return a.index < b.index;
        });
    for (auto& objective : objectives) {
        objective.content_fingerprint = FingerprintEncodedContent(objective.encoded_content);
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

ReducerOutput Reduce(const ReducerInput& input)
{
    ReducerOutput out;
    out.next = input.previous;

    if (input.previous.character_key.empty() && out.next.character_key.empty()) {
        // Caller may supply key only on previous; keep empty if both empty.
    }
    if (!input.previous.character_key.empty()) {
        out.next.character_key = input.previous.character_key;
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

    // Update / insert present quests.
    for (const auto& [quest_id, obs] : present) {
        (void)quest_id;
        ApplyPresentQuest(out.next, out, obs, input.observed_at_utc);
    }

    // Missing from snapshot: pair evidence or become unknown (including session_gap).
    std::vector<uint32_t> missing;
    for (const auto& [quest_id, quest] : out.next.quests) {
        (void)quest;
        if (present.find(quest_id) == present.end()) {
            missing.push_back(quest_id);
        }
    }
    for (const auto quest_id : missing) {
        // Evidence for a different quest must not affect this disappearance.
        ApplyMissingQuest(out.next, out, quest_id, input.evidence, input.observed_at_utc);
    }

    // session_gap is informational for callers; missing-path already applies unknown.
    if (input.session_gap) {
        AppendDiag(out, "session_gap: missing quests resolved without fabricating completion");
    }

    // Guard: never leave reserved states in output.
    for (auto& [quest_id, quest] : out.next.quests) {
        (void)quest_id;
        if (IsReservedState(quest.state)) {
            out.diagnostics.reserved_state_attempt = true;
            AppendDiag(out, "sanitized reserved state to unknown");
            quest.state = ProgressState::Unknown;
            quest.confidence = Confidence::Uncertain;
        }
    }

    if (!out.diagnostics.rejected_stale) {
        out.next.last_reduced_at = input.observed_at_utc;
    }

    return out;
}

} // namespace QuestProgress
