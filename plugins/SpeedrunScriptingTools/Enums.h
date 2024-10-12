#pragma once

#include <cstdint>
#include <string>

namespace GW::Constants {
    enum class SkillID : uint32_t;
}

enum class Trigger { None, InstanceLoad, HardModePing, Hotkey, ChatMessage, BeginSkillCast, BeginCooldown, SkillCastInterrupt };
enum class Class { Any, Warrior, Ranger, Monk, Necro, Mesmer, Elementalist, Assassin, Ritualist, Paragon, Dervish };
enum class AgentType { Any, Self, PartyMember, Friendly, Hostile };
enum class Sorting { AgentId, ClosestToPlayer, FurthestFromPlayer, ClosestToTarget, FurthestFromTarget, LowestHp, HighestHp, ModelID };
enum class AnyNoYes { Any, No, Yes };
enum class Channel { All, Guild, Team, Trade, Alliance, Whisper, Emote, Log };
enum class QuestStatus { NotStarted, Started, Completed, Failed };
enum class GoToTargetFinishCondition { None, StoppedMovingNextToTarget, DialogOpen };
enum class HasSkillRequirement {OnBar, OffCooldown, ReadyToUse};
enum class PlayerConnectednessRequirement {All, Individual};
enum class Status {Enchanted, WeaponSpelled, Alive, Bleeding, Crippled, DeepWounded, Poisoned, Hexed, Idle, KnockedDown, Moving, Attacking, Casting};
enum class EquippedItemSlot {Mainhand, Offhand, Chest, Legs, Head, Feet, Hands};
enum class WeaponType {Any, None, Bow, Axe, Hammer, Daggers, Scythe, Spear, Sword, Wand, Staff};
enum class TrueFalse {True, False};
enum class MoveToBehaviour {SendOnce, RepeatIfIdle, ImmediateFinish};
enum class ReferenceFrame { Player, Camera };
enum class Bag {Backpack, BeltPouch, Bag1, Bag2, EquipmentPack};
enum class ComparisonOperator {Equals, Less, Greater, LessOrEqual, GreaterOrEqual, NotEquals};
enum class IsIsNot {Is, IsNot};

enum class ActionBehaviourFlag : uint32_t 
{
    Default = 0,

    ImmediateFinish = 1 << 0,
    CanBeRunInOutpost = 1 << 1,

    All = 0xFFFF,
};

class ActionBehaviourFlags 
{
public:
    ActionBehaviourFlags() = default;
    ActionBehaviourFlags(ActionBehaviourFlag flag) { flags = (uint32_t)flag; }

    bool test(ActionBehaviourFlag flag) { return flags & (uint32_t)flag; }
    ActionBehaviourFlags& operator|(ActionBehaviourFlag flag)
    {
        flags |= (uint32_t)flag;
        return *this;
    }
    ActionBehaviourFlags& operator&=(ActionBehaviourFlags other) { 
        flags &= other.flags;
        return *this;
    }
    
private:
    uint32_t flags = 0;
};
inline ActionBehaviourFlags operator|(ActionBehaviourFlag a, ActionBehaviourFlag b) 
{
    return ActionBehaviourFlags{} | a | b;
}

struct Hotkey 
{
    long keyData = 0;
    long modifier = 0;
    bool operator==(const Hotkey&) const = default;
};

struct TriggerData 
{
    Hotkey hotkey{};
    std::string message{};
    GW::Constants::SkillID skillId{};
};
