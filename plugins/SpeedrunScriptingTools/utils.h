#pragma once

#include<sstream>

std::string readStringWithSpaces(std::istringstream&);
void writeStringWithSpaces(std::ostringstream&, const std::string& word);
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
