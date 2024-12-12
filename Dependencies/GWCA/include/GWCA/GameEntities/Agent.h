#pragma once

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Item.h>

namespace GW {
    typedef uint32_t AgentID;
    typedef uint32_t PlayerNumber;
    typedef uint32_t ItemID;
    
    namespace Constants {
        enum class Allegiance : uint8_t;
    }

    struct Vec3f;
    struct GamePos;

    struct VisibleEffect {
        uint32_t unk; //enchantment = 1, weapon spell = 9
        Constants::EffectID id;
        uint32_t has_ended; //effect no longer active, effect ending animation plays.
    };

    typedef TList<VisibleEffect> VisibleEffectList;

    // Courtesy of DerMonech14
    struct Equipment {

        /* +h0000 */ void     *vtable;
        /* +h0004 */ uint32_t h0004;            // always 2 ?
        /* +h0008 */ uint32_t h0008;            // Ptr PlayerModelFile?
        /* +h000C */ uint32_t h000C;            // 
        /* +h0010 */ ItemData* reft_hand_ptr;   // Ptr Bow, Hammer, Focus, Daggers, Scythe
        /* +h0014 */ ItemData* right_hand_ptr;  // Ptr Sword, Spear, Staff, Daggers, Axe, Zepter, Bundle
        /* +h0018 */ uint32_t h0018;            // 
        /* +h001C */ ItemData* shield_ptr;      // Ptr Shield
        /* +h0020 */ uint8_t left_hand_map;     // Weapon1     None = 9, Bow = 0, Hammer = 0, Focus = 1, Daggers = 0, Scythe = 0
        /* +h0021 */ uint8_t right_hand_map;    // Weapon2     None = 9, Sword = 0, Spear = 0, Staff = 0, Daggers = 0, Axe = 0, Zepter = 0, Bundle
        /* +h0022 */ uint8_t head_map;          // Head        None = 9, Headpiece Ele = 4
        /* +h0023 */ uint8_t shield_map;        // Shield      None = 9, Shield = 1
        union {
        /* +h0024 */ ItemData items[9];
            struct {
        /* +h0024 */ ItemData weapon;
        /* +h0034 */ ItemData offhand;
        /* +h0044 */ ItemData chest;
        /* +h0054 */ ItemData legs;
        /* +h0064 */ ItemData head;
        /* +h0074 */ ItemData feet;
        /* +h0084 */ ItemData hands;
        /* +h0094 */ ItemData costume_body;
        /* +h00A4 */ ItemData costume_head;
            };
        };
        union {
            /* +h00B4 */ ItemID item_ids[9];
            struct {
                /* +h00B4 */ ItemID item_id_weapon;
                /* +h00B8 */ ItemID item_id_offhand;
                /* +h00BC */ ItemID item_id_chest;
                /* +h00C0 */ ItemID item_id_legs;
                /* +h00C4 */ ItemID item_id_head;
                /* +h00C8 */ ItemID item_id_feet;
                /* +h00CC */ ItemID item_id_hands;
                /* +h00D0 */ ItemID item_id_costume_body;
                /* +h00D4 */ ItemID item_id_costume_head;
            };
        };
    };

    struct TagInfo {
        /* +h0000 */ uint16_t guild_id;
        /* +h0002 */ uint8_t primary;
        /* +h0003 */ uint8_t secondary;
        /* +h0004 */ uint16_t level;
        // ...
    };

    struct AgentItem;
    struct AgentGadget;
    struct AgentLiving;

    struct Agent {
        /* +h0000 */ uint32_t* vtable;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C[2];
        /* +h0014 */ uint32_t timer; // Agent Instance Timer (in Frames)
        /* +h0018 */ uint32_t timer2;
        /* +h001C */ TLink<Agent> link;
        /* +h0024 */ TLink<Agent> link2;
        /* +h002C */ AgentID agent_id;
        /* +h0030 */ float z; // Z coord in float
        /* +h0034 */ float width1;  // Width of the model's box
        /* +h0038 */ float height1; // Height of the model's box
        /* +h003C */ float width2;  // Width of the model's box (same as 1)
        /* +h0040 */ float height2; // Height of the model's box (same as 1)
        /* +h0044 */ float width3;  // Width of the model's box (usually same as 1)
        /* +h0048 */ float height3; // Height of the model's box (usually same as 1)
        /* +h004C */ float rotation_angle; // Rotation in radians from East (-pi to pi)
        /* +h0050 */ float rotation_cos; // cosine of rotation
        /* +h0054 */ float rotation_sin; // sine of rotation
        /* +h0058 */ uint32_t name_properties; // Bitmap basically telling what the agent is
        /* +h005C */ uint32_t ground;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ Vec3f terrain_normal;
        /* +h0070 */ uint8_t  h0070[4];
        union {
            struct {
        /* +h0074 */ float x;
        /* +h0078 */ float y;
        /* +h007C */ uint32_t plane;
            };
        /* +h0074 */ GamePos pos;          
        };
        /* +h0080 */ uint8_t h0080[4];
        /* +h0084 */ float name_tag_x; // Exactly the same as X above
        /* +h0088 */ float name_tag_y; // Exactly the same as Y above
        /* +h008C */ float name_tag_z; // Z coord in float
        /* +h0090 */ uint16_t visual_effects; // Number of Visual Effects of Agent (Skills, Weapons); 1 = Always set;
        /* +h0092 */ uint16_t h0092;
        /* +h0094 */ uint32_t h0094[2];
        /* +h009C */ uint32_t type; // Livings = 0xDB, Gadgets = 0x200, Items = 0x400.
        union {
            struct {
        /* +h00A0 */ float move_x; //If moving, how much on the X axis per second
        /* +h00A4 */ float move_y; //If moving, how much on the Y axis per second
            };
        /* +h00A0 */ Vec2f velocity;
        };
        /* +h00A8 */ uint32_t h00A8; // always 0?
        /* +h00AC */ float rotation_cos2; // same as cosine above
        /* +h00B0 */ float rotation_sin2; // same as sine above
        /* +h00B4 */ uint32_t h00B4[4];

        // Agent Type Bitmasks.
        inline bool GetIsItemType()        const { return (type & 0x400) != 0; }
        inline bool GetIsGadgetType()      const { return (type & 0x200) != 0; }
        inline bool GetIsLivingType()      const { return (type & 0xDB)  != 0; }

        inline AgentItem*   GetAsAgentItem();
        inline AgentGadget* GetAsAgentGadget();
        inline AgentLiving* GetAsAgentLiving();

        inline const AgentItem*   GetAsAgentItem() const;
        inline const AgentGadget* GetAsAgentGadget() const;
        inline const AgentLiving* GetAsAgentLiving() const;
    };

    struct AgentItem : public Agent { // total: 0xD4/212
        /* +h00C4 */ AgentID owner;
        /* +h00C8 */ ItemID item_id;
        /* +h00CC */ uint32_t h00CC;
        /* +h00D0 */ uint32_t extra_type;
    };
    static_assert(sizeof(AgentItem) == 212, "struct AgentItem has incorrect size");
    static_assert(offsetof(AgentItem, owner) == 0xC4, "struct AgentItem offsets are incorrect");

    struct AgentGadget : public Agent { // total: 0xE4/228
        /* +h00C4 */ uint32_t h00C4;
        /* +h00C8 */ uint32_t h00C8;
        /* +h00CC */ uint32_t extra_type;
        /* +h00D0 */ uint32_t gadget_id;
        /* +h00D4 */ uint32_t h00D4[4];
    };
    static_assert(sizeof(AgentGadget) == 228, "struct AgentGadget has incorrect size");
    static_assert(offsetof(AgentGadget, h00C4) == 0xC4, "struct AgentGadget offsets are incorrect");

    struct AgentLiving : public Agent { // total: 0x1C0/448
        /* +h00C4 */ AgentID owner;
        /* +h00C8 */ uint32_t h00C8;
        /* +h00CC */ uint32_t h00CC;
        /* +h00D0 */ uint32_t h00D0;
        /* +h00D4 */ uint32_t h00D4[3];
        /* +h00E0 */ float animation_type;
        /* +h00E4 */ uint32_t h00E4[2];
        /* +h00EC */ float weapon_attack_speed; // The base attack speed in float of last attacks weapon. 1.33 = axe, sWORD, daggers etc.
        /* +h00F0 */ float attack_speed_modifier; // Attack speed modifier of the last attack. 0.67 = 33% increase (1-.33)
        // NB: h00F4 is actually a composite uint32_t that the game uses to EITHER get the player model info or npc model info
        /* +h00F4 */ uint16_t player_number; // Selfexplanatory. All non-players have identifiers for their type. Two of the same mob = same number
        /* +h00F6 */ uint16_t agent_model_type; // Player = 0x3000, NPC = 0x2000
        /* +h00F8 */ uint32_t transmog_npc_id; // Actually, it's 0x20000000 | npc_id, It's not defined for npc, minipet, etc...
        /* +h00FC */ Equipment** equip;
        /* +h0100 */ uint32_t h0100;
        /* +h0104 */ TagInfo *tags; // struct { uint16_t guild_id, uint8_t primary, uint8_t secondary, uint16_t level
        /* +h0108 */ uint16_t  h0108;
        /* +h010A */ uint8_t  primary; // Primary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
        /* +h010B */ uint8_t  secondary; // Secondary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
        /* +h010C */ uint8_t  level; // Duh!
        /* +h010D */ uint8_t  team_id; // 0=None, 1=Blue, 2=Red, 3=Yellow
        /* +h010E */ uint8_t  h010E[2];
        /* +h0110 */ uint32_t h0110;
        /* +h0114 */ float energy_regen;
        /* +h0118 */ uint32_t h0118;
        /* +h011C */ float energy; // Only works for yourself
        /* +h0120 */ uint32_t max_energy; // Only works for yourself
        /* +h0124 */ uint32_t h0124;
        /* +h0128 */ float hp_pips; // Regen/degen as float
        /* +h012C */ uint32_t h012C;
        /* +h0130 */ float hp; // Health in % where 1=100% and 0=0%
        /* +h0134 */ uint32_t max_hp; // Only works for yourself
        /* +h0138 */ uint32_t effects; // Bitmap for effects to display when targetted. DOES include hexes
        /* +h013C */ uint32_t h013C;
        /* +h0140 */ uint8_t  hex; // Bitmap for the hex effect when targetted (apparently obsolete!) (yes)
        /* +h0141 */ uint8_t  h0141[19];
        /* +h0154 */ uint32_t model_state; // Different values for different states of the model.
        /* +h0158 */ uint32_t type_map; // Odd variable! 0x08 = dead, 0xC00 = boss, 0x40000 = spirit, 0x400000 = player
        /* +h015C */ uint32_t h015C[4];
        /* +h016C */ uint32_t in_spirit_range; // Tells if agent is within spirit range of you. Doesn't work anymore?
        /* +h0170 */ VisibleEffectList visible_effects;
        /* +h017C */ uint32_t h017C;
        /* +h0180 */ uint32_t login_number; // Unique number in instance that only works for players
        /* +h0184 */ float    animation_speed;  // Speed of the current animation
        /* +h0188 */ uint32_t animation_code; // related to animations
        /* +h018C */ uint32_t animation_id;     // Id of the current animation
        /* +h0190 */ uint8_t  h0190[32];
        /* +h01B0 */ uint8_t  dagger_status; // 0x1 = used lead attack, 0x2 = used offhand attack, 0x3 = used dual attack
        /* +h01B1 */ Constants::Allegiance  allegiance; // 0x1 = ally/non-attackable, 0x2 = neutral, 0x3 = enemy, 0x4 = spirit/pet, 0x5 = minion, 0x6 = npc/minipet
        /* +h01B2 */ uint16_t  weapon_type; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff
        /* +h01B4 */ uint16_t  skill; // 0 = not using a skill. Anything else is the Id of that skill
        /* +h01B6 */ uint16_t  h01B6;
        /* +h01B8 */ uint8_t  weapon_item_type;
        /* +h01B9 */ uint8_t  offhand_item_type;
        /* +h01BA */ uint16_t  weapon_item_id;
        /* +h01BC */ uint16_t  offhand_item_id;

        // Health Bar Effect Bitmasks.
        inline bool GetIsBleeding()        const { return (effects & 0x0001) != 0; }
        inline bool GetIsConditioned()     const { return (effects & 0x0002) != 0; }
        inline bool GetIsCrippled()        const { return (effects & 0x000A) == 0xA; }
        inline bool GetIsDead()            const { return (effects & 0x0010) != 0; }
        inline bool GetIsDeepWounded()     const { return (effects & 0x0020) != 0; }
        inline bool GetIsPoisoned()        const { return (effects & 0x0040) != 0; }
        inline bool GetIsEnchanted()       const { return (effects & 0x0080) != 0; }
        inline bool GetIsDegenHexed()      const { return (effects & 0x0400) != 0; }
        inline bool GetIsHexed()           const { return (effects & 0x0800) != 0; }
        inline bool GetIsWeaponSpelled()   const { return (effects & 0x8000) != 0; }

        // Agent TypeMap Bitmasks.
        inline bool GetInCombatStance()    const { return (type_map & 0x000001) != 0; }
        inline bool GetHasQuest()          const { return (type_map & 0x000002) != 0; } // if agent has quest marker
        inline bool GetIsDeadByTypeMap()   const { return (type_map & 0x000008) != 0; }
        inline bool GetIsFemale()          const { return (type_map & 0x000200) != 0; }
        inline bool GetHasBossGlow()       const { return (type_map & 0x000400) != 0; }
        inline bool GetIsHidingCape()      const { return (type_map & 0x001000) != 0; }
        inline bool GetCanBeViewedInPartyWindow() const { return (type_map & 0x20000) != 0; }
        inline bool GetIsSpawned()         const { return (type_map & 0x040000) != 0; }
        inline bool GetIsBeingObserved()   const { return (type_map & 0x400000) != 0; }

        // Modelstates.
        inline bool GetIsKnockedDown()     const { return model_state == 1104; }
        inline bool GetIsMoving()          const { return model_state == 12 || model_state == 76   || model_state == 204; }
        inline bool GetIsAttacking()       const { return model_state == 96 || model_state == 1088 || model_state == 1120; }
        inline bool GetIsCasting()         const { return model_state == 65 || model_state == 581; }
        inline bool GetIsIdle()            const { return model_state == 68 || model_state == 64 || model_state == 100; }

        // Composite bool, sometimes agents can be dead but have hp (e.g. packets are received in wrong order)
        inline bool GetIsAlive()            const { return !GetIsDead() && hp > .0f; }

        inline bool IsPlayer()             const { return login_number != 0; }
        inline bool IsNPC()                const { return login_number == 0; }
    };
    static_assert(sizeof(AgentLiving) == 0x1C0, "struct AgentLiving has incorrect size");
    static_assert(offsetof(AgentLiving, owner) == 0xC4, "struct AgentLiving offsets are incorrect");

    AgentItem* Agent::GetAsAgentItem() {
        if (GetIsItemType())
            return static_cast<AgentItem*>(this);
        else
            return nullptr;
    }

    AgentGadget* Agent::GetAsAgentGadget() {
        if (GetIsGadgetType())
            return static_cast<AgentGadget*>(this);
        else
            return nullptr;
    }

    AgentLiving* Agent::GetAsAgentLiving() {
        if (GetIsLivingType())
            return static_cast<AgentLiving*>(this);
        else
            return nullptr;
    }

    const AgentItem* Agent::GetAsAgentItem() const {
        if (GetIsItemType())
            return static_cast<const AgentItem*>(this);
        else
            return nullptr;
    }

    const AgentGadget* Agent::GetAsAgentGadget() const {
        if (GetIsGadgetType())
            return static_cast<const AgentGadget*>(this);
        else
            return nullptr;
    }

    const AgentLiving* Agent::GetAsAgentLiving() const {
        if (GetIsLivingType())
            return static_cast<const AgentLiving*>(this);
        else
            return nullptr;
    }

    struct MapAgent {
        /* +h0000 */ float cur_energy;
        /* +h0004 */ float max_energy;
        /* +h0008 */ float energy_regen;
        /* +h000C */ uint32_t skill_timestamp;
        /* +h0010 */ float h0010;
        /* +h0014 */ float max_energy2;
        /* +h0018 */ float h0018;
        /* +h001C */ uint32_t h001C;
        /* +h0020 */ float cur_health;
        /* +h0024 */ float max_health;
        /* +h0028 */ float health_regen;
        /* +h002C */ uint32_t h002C;
        /* +h0030 */ uint32_t effects;

        // Health Bar Effect Bitmasks.
        inline bool GetIsBleeding()         const { return (effects & 0x0001) != 0; }
        inline bool GetIsConditioned()      const { return (effects & 0x0002) != 0; }
        inline bool GetIsCrippled()        const { return (effects & 0x000A) == 0xA; }
        inline bool GetIsDead()             const { return (effects & 0x0010) != 0; }
        inline bool GetIsDeepWounded()      const { return (effects & 0x0020) != 0; }
        inline bool GetIsPoisoned()         const { return (effects & 0x0040) != 0; }
        inline bool GetIsEnchanted()        const { return (effects & 0x0080) != 0; }
        inline bool GetIsDegenHexed()       const { return (effects & 0x0400) != 0; }
        inline bool GetIsHexed()            const { return (effects & 0x0800) != 0; }
        inline bool GetIsWeaponSpelled()    const { return (effects & 0x8000) != 0; }
    };

    struct AgentMovement {
        /* +h0000 */ uint32_t h0000[3];
        /* +h000C */ uint32_t agent_id;
        /* +h0010 */ uint32_t h0010[3];
        /* +h001C */ uint32_t agentDef; // GW_AGENTDEF_CHAR = 1
        /* +h0020 */ uint32_t h0020[6];
        /* +h0038 */ uint32_t moving1; //tells if you are stuck even if your client doesn't know
        /* +h003C */ uint32_t h003C[2];
        /* +h0044 */ uint32_t moving2; //exactly same as Moving1
        /* +h0048 */ uint32_t h0048[7];
        /* +h0064 */ Vec3f h0064;
        /* +h0070 */ uint32_t h0070;
        /* +h0074 */ Vec3f h0074;
    };

    struct AgentInfo {
        uint32_t h0000[13];
        wchar_t *name_enc;
    };
    static_assert(sizeof(AgentInfo) == 0x38, "struct AgentInfo has incorrect size");

    typedef TList<Agent> AgentList;
    typedef Array<Agent *> AgentArray;

    typedef Array<MapAgent> MapAgentArray;
    typedef Array<AgentInfo> AgentInfoArray;
    typedef Array<AgentMovement *> AgentMovementArray;
}
