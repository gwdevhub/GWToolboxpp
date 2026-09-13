#pragma once

#include <Modules/QuestJourneyBaselineTransition.h>
#include <Modules/QuestSessionIdentity.h>

#include <string>

namespace QuestProgress {

struct CharacterJourneyBaselineCandidates {
    IdSetBaselineCandidate maps;
    IdSetBaselineCandidate character_skills;
    IdSetBaselineCandidate heroes;
    IdSetBaselineCandidate professions;
    IdSetBaselineCandidate vanquish_areas;
    FlagBaselineCandidate hard_mode;

    friend bool operator==(
        const CharacterJourneyBaselineCandidates& a,
        const CharacterJourneyBaselineCandidates& b)
    {
        return a.maps == b.maps
            && a.character_skills == b.character_skills
            && a.heroes == b.heroes
            && a.professions == b.professions
            && a.vanquish_areas == b.vanquish_areas
            && a.hard_mode == b.hard_mode;
    }
};

class JourneyBaselineCandidateOwner {
public:
    void Clear();
    void OpenForPersistentIdentity(const SessionIdentity& identity);

    bool has_active_scope() const { return active_; }
    const std::string& account_key() const { return account_key_; }
    const std::string& character_key() const { return character_key_; }

    CharacterJourneyBaselineCandidates* Active();
    const CharacterJourneyBaselineCandidates* Active() const;

private:
    bool active_ = false;
    std::string account_key_;
    std::string character_key_;
    CharacterJourneyBaselineCandidates candidates_{};
};

} // namespace QuestProgress
