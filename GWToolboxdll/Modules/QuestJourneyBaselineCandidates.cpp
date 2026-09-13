#include <Modules/QuestJourneyBaselineCandidates.h>

namespace QuestProgress {

void JourneyBaselineCandidateOwner::Clear()
{
    active_ = false;
    account_key_.clear();
    character_key_.clear();
    candidates_ = {};
}

void JourneyBaselineCandidateOwner::OpenForPersistentIdentity(const SessionIdentity& identity)
{
    if (identity.kind != IdentityKind::Persistent
        || identity.account_key.empty()
        || identity.character_key.empty()) {
        Clear();
        return;
    }
    if (active_
        && account_key_ == identity.account_key
        && character_key_ == identity.character_key) {
        return;
    }
    Clear();
    active_ = true;
    account_key_ = identity.account_key;
    character_key_ = identity.character_key;
    candidates_ = {};
}

CharacterJourneyBaselineCandidates* JourneyBaselineCandidateOwner::Active()
{
    return active_ ? &candidates_ : nullptr;
}

const CharacterJourneyBaselineCandidates* JourneyBaselineCandidateOwner::Active() const
{
    return active_ ? &candidates_ : nullptr;
}

} // namespace QuestProgress
