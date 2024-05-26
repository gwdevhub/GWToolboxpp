#pragma once

enum class Trigger { None, InstanceLoad, HardModePing, Hotkey };
enum class Class { Any, Warrior, Ranger, Monk, Necro, Mesmer, Elementalist, Assassin, Ritualist, Paragon, Dervish };
enum class Status { Any, Dead, Alive };
enum class AgentType { Any, Self, PartyMember, Friendly, Hostile };
enum class Sorting { AgentId, ClosestToPlayer, FurthestFromPlayer, ClosestToTarget, FurthestFromTarget, LowestHp, HighestHp };
enum class HexedStatus { Any, NotHexed, Hexed };
enum class Channel { All, Guild, Team, Trade, Alliance, Whisper, Emote };
enum class QuestStatus { NotStarted, Started, Completed, Failed };
enum class GoToTargetFinishCondition { None, StoppedMovingNextToTarget, DialogOpen };
enum class HasSkillRequirement {OnBar, OffCooldown, ReadyToUse};
