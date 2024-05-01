#pragma once

#include "Windows.h"
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>

std::string readStringWithSpaces(std::istringstream&);
void writeStringWithSpaces(std::ostringstream&, const std::string& word);

std::vector<GW::Vec2f> readPositions(std::istringstream&);
void writePositions(std::ostringstream&, const std::vector<GW::Vec2f>&);

void drawPolygonSelector(std::vector<GW::Vec2f>& polygon);
bool pointIsInsidePolygon(const GW::GamePos pos, const std::vector<GW::Vec2f>& polygon);

std::string exportString(const std::string&);
std::string importString(std::string&&);

void ShowHelp(const char* help);
void drawHotkeySelector(long& keyData, long& modifier, std::string& description, float selectorWidth);
std::string makeHotkeyDescription(long keyData, long modifier);

enum class Class : int { Any, Warrior, Ranger, Monk, Necro, Mesmer, Elementalist, Assassin, Ritualist, Paragon, Dervish };
enum class Status : int { Any, Dead, Alive };
enum class AgentType : int { Any, PartyMember, Friendly, Hostile };
enum class Sorting : int { AgentId, ClosestToPlayer, FurthestFromPlayer, ClosestToTarget, FurthestFromTarget, LowestHp, HighestHp };
enum class HexedStatus : int { Any, NotHexed, Hexed };

std::string_view toString(Status);
std::string_view toString(AgentType);
std::string_view toString(Sorting);
std::string_view toString(HexedStatus);
std::string toString(Class);

template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
std::ostream& operator<<(std::ostream& os, T t)
{
    using ut = std::underlying_type_t<T>;
    os << static_cast<ut>(t);
    return os;
}
template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
std::istringstream& operator>>(std::istringstream& is, T& t)
{
    using ut = std::underlying_type_t<T>;
    ut read;
    is >> read;
    t = static_cast<T>(read);
    return is;
}
