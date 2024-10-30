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
enum class DoorStatus {Open, Closed };
enum class Area { Urgoz, Deep, Doa }; // Some Door IDs are duplicated between areas

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
    AnyNoYes hsr = AnyNoYes::Any;
};


enum class DoorID : uint32_t {
    Urgoz_zone_2 = 45420,  // Life Drain
    Urgoz_zone_3 = 11692,  // Levers
    Urgoz_zone_4 = 54552,  // Bridge Wolves
    Urgoz_zone_5 = 1760,   // More Wolves
    Urgoz_zone_6 = 40330,  // Energy Drain
    Urgoz_zone_7 = 60114,  // Exhaustion
    Urgoz_zone_8 = 37191,  // Pillars
    Urgoz_zone_9 = 35500,  // Blood Drinkers
    Urgoz_zone_10 = 34278, // Bridge
    Urgoz_zone_11 = 15529, // Urgoz 1

    // object_id's for doors opening.
    Deep_room_1_first = 12669,
    // Room 1 Complete = Room 5 open
    Deep_room_1_second = 11692,
    // Room 1 Complete = Room 5 open
    Deep_room_2_first = 54552,
    // Room 2 Complete = Room 5 open
    Deep_room_2_second = 1760,
    // Room 2 Complete = Room 5 open
    Deep_room_3_first = 45425,
    // Room 3 Complete = Room 5 open
    Deep_room_3_second = 48290,
    // Room 3 Complete = Room 5 open
    Deep_room_4_first = 40330,
    // Room 4 Complete = Room 5 open
    Deep_room_4_second = 60114,
    // Room 4 Complete = Room 5 open
    Deep_room_5 = 29594,
    // Room 5 Complete = Room 1,2,3,4,6 open
    Deep_room_6 = 49742,
    // Room 6 Complete = Room 7 open
    Deep_room_7 = 55680,
    // Room 7 Complete = Room 8 open
    // NOTE: Room 8 (failure) to room 10 (scorpion), no door.
    Deep_room_9 = 99887,
    // Trigger on leviathan?
    Deep_room_11 = 28961,
    // Room 11 door is always open. Use to START room 11 when it comes into range.

    DoA_foundry_entrance_r1 = 39534,
    DoA_foundry_r1_r2 = 6356,
    DoA_foundry_r2_r3 = 45276,
    DoA_foundry_r3_r4 = 55421,
    DoA_foundry_r4_r5 = 49719,
    DoA_foundry_r5_bb = 45667,
    DoA_foundry_behind_bb = 1731,
    DoA_city_entrance = 63939,
    DoA_city_wall = 54727,
    DoA_city_jadoth = 64556,
    DoA_veil_360_left = 13005,
    DoA_veil_360_middle = 11772,
    DoA_veil_360_right = 28851,
    DoA_veil_derv = 56510,
    DoA_veil_ranger = 4753,
    DoA_veil_trench_necro = 46650,
    DoA_veil_trench_mes = 29594,
    DoA_veil_trench_ele = 49742,
    DoA_veil_trench_monk = 55680,
    DoA_veil_trench_gloom = 28961,
    DoA_veil_to_gloom = 3,
    DoA_gloom_to_foundry = 17955,
    DoA_gloom_rift = 47069, // not really a door, has animation type=9 when closed
};
