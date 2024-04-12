#include <utils.h>

namespace 
{
    const std::string endOfStringSignifier{"ENDOFSTRING"};
}

std::string readStringWithSpaces(std::istringstream& stream) 
{
    std::string result;
    std::string word;
    while (stream >> word) {
        if (word == endOfStringSignifier) break;
        result += word + " ";
    }
    if (!result.empty()) {
        result.erase(result.size() - 1, 1); // last character is space
    }

    return result;
}

void writeStringWithSpaces(std::ostringstream& stream, const std::string& word) 
{
    stream << word << " " << endOfStringSignifier << " ";
}

std::string_view toString(Status status)
{
    switch (status) {
        case Status::Any:
            return "Any";
        case Status::Dead:
            return "Dead";
        case Status::Alive:
            return "Alive";
    }
    return "";
}

std::string toString(Class c)
{
    switch (c) {
        case Class::Warrior:
            return "Warrior";
        case Class::Ranger:
            return "Ranger";
        case Class::Monk:
            return "Monk";
        case Class::Necro:
            return "Necro";
        case Class::Mesmer:
            return "Mesmer";
        case Class::Elementalist:
            return "Elementalist";
        case Class::Assassin:
            return "Assassin";
        case Class::Ritualist:
            return "Ritualist";
        case Class::Paragon:
            return "Paragon";
        case Class::Dervish:
            return "Dervish";
        default:
            return "Any";
    }
}
std::string_view toString(AgentType type) {
    switch (type) {
        case AgentType::Any:
            return "Any";
        case AgentType::PartyMember:
            return "Party member";
        case AgentType::Friendly:
            return "Friendly";
        case AgentType::Hostile:
            return "Hostile";
        default: 
            return "Any";
    }
}
std::string_view toString(Sorting sorting)
{
    switch (sorting) {
        case Sorting::AgentId:
            return "Any";
        case Sorting::ClosestToPlayer:
            return "Closest to player";
        case Sorting::FurthestFromPlayer:
            return "Furthest from player";
        case Sorting::ClosestToTarget:
            return "Closest to target";
        case Sorting::FurthestFromTarget:
            return "Furthest from target";
        case Sorting::LowestHp:
            return "Lowest HP";
        case Sorting::HighestHp:
            return "Highest HP";
        default:
            return "";
    }
}
std::string_view toString(HexedStatus status) {
    switch (status) {
        case HexedStatus::Any:
            return "Any";
        case HexedStatus::NotHexed:
            return "Not hexed";
        case HexedStatus::Hexed:
            return "Hexed";
        default:
            return "";
    }
}
