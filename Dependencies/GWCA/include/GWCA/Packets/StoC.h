#pragma once

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/GameContainers/GamePos.h>

/*
Server to client packets, sorted by header

most packets are not filled in, however the list and types of
fields is given as comments

feel free to fill packets, and you can also add a suffix to
the packet name, e.g. P391 -> P391_InstanceLoadMap
*/

namespace GW {
    typedef uint32_t AgentID;
    typedef uint32_t ItemID;
    namespace Chat {
        enum Channel : int;
    }
    namespace Constants {
        enum class QuestID : uint32_t;
        enum class MapID : uint32_t;
    }
    namespace Packet {
        namespace StoC {
            struct PacketBase {
                uint32_t header;
            };
            // Used for:
            //  GenericValue
            //  GenericValueTarget
            //  GenericModifier (i.e. GenericFloatTarget)
            //  GenericFloat
            namespace GenericValueID {
                const uint32_t melee_attack_finished    = 1;  // GenericValue. Last melee attack finished successfully.
                const uint32_t attack_stopped           = 3;  // GenericValue. Last melee/ranged attack stopped unsuccessfully. May be followed by interrupted (35).
                const uint32_t attack_started           = 4;  // GenericTargetValue. caster_id is victim. target_id is attacker.

                const uint32_t add_effect               = 6;
                const uint32_t remove_effect            = 7;
                const uint32_t disabled                 = 8;  // GenericValue. e.g. aftercast. value = 1: disabled. value = 0: not disabled.
                const uint32_t skill_damage             = 10; // GenericValue. The skill responsible for the last damage packet received (GenericTargetModifier).
                const uint32_t apply_marker             = 11; // Exclamation mark / arrow above NPC head
                const uint32_t remove_marker            = 12; // Exclamation mark / arrow above NPC head
                const uint32_t damage                   = 16; // GenericTargetModifier. Non-armor-ignoring attack, spells
                const uint32_t critical                 = 17; // GenericTargetModifier. Critical hit on autoattack
                const uint32_t effect_on_target         = 20; // e.g. casting a skill on someone
                const uint32_t effect_on_agent          = 21; // e.g. casting a skill on myself/location
                const uint32_t animation                = 22;
                const uint32_t animation_special        = 23; // When received before dance, makes it fancy e.g. CE dance, glowing hands
                const uint32_t animation_loop           = 28; // e.g. dance
                const uint32_t max_hp_reached           = 32; // GenericValue. Remove passive regen pips.
                const uint32_t health                   = 34;
                const uint32_t interrupted              = 35; // GenericValue. The last action (skill or attack) was interrupted. Follows <action>_stopped.
                const uint32_t change_health_regen      = 44; // GenericFloat. Passive regen pips (?).
                const uint32_t attack_skill_finished    = 46; // GenericValue. Last attack skill finished successfully.
                const uint32_t instant_skill_activated  = 48; // GenericValue|(GenericTargetValue?). Unblocked skills - Stances, shouts, etc, not protectors defence.
                const uint32_t attack_skill_stopped     = 49; // GenericValue. Last attack skill stopped unsuccessfully. May be followed by interrupted (35).
                const uint32_t attack_skill_activated   = 50; // GenericValue|GenericValueTarget. caster_id is victim and target_id is caster.
                const uint32_t energygain               = 52; // For example from Critical Strikes or energy tap
                const uint32_t armorignoring            = 55; // GenericTargetModifier. All armor ignoring damage and heals
                const uint32_t skill_finished           = 58; // GenericValue. Last skill finished successfully.
                const uint32_t skill_stopped            = 59; // GenericValue. Last skill stopped unsuccessfully. May be followed by interrupted(35).
                const uint32_t skill_activated          = 60; // GenericValue|GenericValueTarget. caster_id is victim and target_id is caster.
                const uint32_t casttime                 = 61; // Non-standard cast time, value in seconds
                const uint32_t energy_spent             = 62; // GenericFloat. The energy using your last skill.
                const uint32_t knocked_down             = 63; // GenericFloat. value: duration.
            }
            namespace P156_Type = GenericValueID;

            namespace JumboMessageType {
                const uint8_t BASE_UNDER_ATTACK = 0;
                const uint8_t GUILD_LORD_UNDER_ATTACK = 1;
                const uint8_t CAPTURED_SHRINE = 3;
                const uint8_t CAPTURED_TOWER = 5;
                const uint8_t PARTY_DEFEATED = 6; // received in 3-way Heroes Ascent matches when one party is defeated
                const uint8_t MORALE_BOOST = 9;
                const uint8_t VICTORY = 16;
                const uint8_t FLAWLESS_VICTORY = 17;
            }

            namespace JumboMessageValue {
                // The following values represent the first and second parties in an explorable areas
                // (inc. observer mode) with 2 parties.
                // if there are 3 parties in the explorable area (like some HA maps) then:
                //  - party 1 = 6579558
                //  - party 2 = 1635021873
                //  - party 3 = 1635021874

                // TODO:
                // These numbers appear big and random, their origin or relation to other values
                // is not understood.
                // In addition, there may be a danger these variables could change with GW updates...
                // Consider these values as experimental and use with caution
                const uint32_t PARTY_ONE = 1635021873;
                const uint32_t PARTY_TWO = 1635021874;
            }

            template <class Specific>
            struct Packet : PacketBase {
                static const uint32_t STATIC_HEADER;
            };
            struct TradeStart : Packet<TradeStart> {
                uint32_t player_number;
            };
            constexpr uint32_t Packet<TradeStart>::STATIC_HEADER = GAME_SMSG_TRADE_REQUEST;

            struct TradeCancel : Packet<TradeCancel> {
                uint32_t player_number;
            };
            constexpr uint32_t Packet<TradeCancel>::STATIC_HEADER = GAME_SMSG_TRADE_TERMINATE;

            struct Ping : Packet<Ping> {
                uint32_t ping;
            };
            constexpr uint32_t Packet<Ping>::STATIC_HEADER = GAME_SMSG_PING_REQUEST;

            struct InstanceTimer : Packet<InstanceTimer> {
                uint32_t instance_time;
            };
            constexpr uint32_t Packet<InstanceTimer>::STATIC_HEADER = GAME_SMSG_AGENT_INSTANCE_TIMER;

            // Called when the client needs to add an agent to memory (i.e. agent appeared within compass range)
            struct AgentAdd : Packet<AgentAdd> {
                /* +h0000 */ uint32_t agent_id;
                /* +h0004 */ uint32_t agent_type; // Bitwise field. 0x20000000 = NPC | PlayerNumber, 0x30000000 = Player | PlayerNumber, 0x00000000 = Signpost
                /* +h0008 */ uint32_t type; // byte, agent_type > 0 ? 1 : 4
                /* +h000C */ uint32_t unk3; // byte
                /* +h0010 */ Vec2f position;
                /* +h0018 */ uint32_t unk4; // word
                /* +h001C */ Vec2f unk5;
                /* +h0020 */ uint32_t unk6; // word
                /* +h0024 */ float speed; // default 288.0
                /* +h0028 */ float unk7; // default 1.0
                /* +h002C */ uint32_t unknown_bitwise_1;
                /* +h0030 */ uint32_t allegiance_bits;
                /* +h0034 */ uint32_t unk8[5];
                /* +h0048 */ Vec2f unk9;
                /* +h0050 */ Vec2f unk10; // inf, inf
                /* +h0058 */ uint32_t unk11[2];
                /* +h0060 */ Vec2f unk12; // inf, inf
                /* +h0064 */ uint32_t unk13;
            };
            constexpr uint32_t Packet<AgentAdd>::STATIC_HEADER = GAME_SMSG_AGENT_SPAWNED;

            // Called when the client needs to remove an agent from memory (e.g. out of range)
            struct AgentRemove : Packet<AgentRemove> {
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<AgentRemove>::STATIC_HEADER = GAME_SMSG_AGENT_DESPAWNED;

            struct AgentSetPlayer : Packet<AgentSetPlayer> {
                uint32_t unk1;
                uint32_t unk2;
            };
            constexpr uint32_t Packet<AgentSetPlayer>::STATIC_HEADER = GAME_SMSG_AGENT_SET_PLAYER;

            struct AgentUpdateAllegiance : Packet<AgentUpdateAllegiance> {
                uint32_t agent_id;
                uint32_t allegiance_bits; // more than just allegiance, determines things that change.
            };
            constexpr uint32_t Packet<AgentUpdateAllegiance>::STATIC_HEADER = GAME_SMSG_AGENT_UPDATE_ALLEGIANCE;

            // creates the "ping" on an enemy when some player targets it
            struct AgentPinged : Packet<AgentPinged> {
                uint32_t Player; // who sent the message
                uint32_t agent_id; // target agent
            };
            constexpr uint32_t Packet<AgentPinged>::STATIC_HEADER = GAME_SMSG_AGENT_PINGED;

            struct PartyRemoveAlly : Packet<PartyRemoveAlly> {
                uint32_t agent_id; // target agent
            };
            constexpr uint32_t Packet<PartyRemoveAlly>::STATIC_HEADER = GAME_SMSG_AGENT_ALLY_DESTROY;

            // prety much a bond from someone else / hero bond
            struct AddExternalBond : Packet<AddExternalBond> {
                uint32_t caster_id;
                uint32_t receiver_id;
                uint32_t skill_id;
                uint32_t effect_type;
                uint32_t effect_id;
            };
            constexpr uint32_t Packet<AddExternalBond>::STATIC_HEADER = GAME_SMSG_EFFECT_UPKEEP_APPLIED;

            struct AddEffect : Packet<AddEffect> {
                uint32_t agent_id;
                uint32_t skill_id;
                uint32_t attribute_level;
                uint32_t effect_id;
                uint32_t timestamp;
            };
            constexpr uint32_t Packet<AddEffect>::STATIC_HEADER = GAME_SMSG_EFFECT_APPLIED;

            // Display Cape (?)
            struct DisplayCape : Packet<DisplayCape> {
                uint32_t agent_id;
                uint8_t unk0;
            };
            constexpr uint32_t Packet<DisplayCape>::STATIC_HEADER = GAME_SMSG_AGENT_DISPLAY_CAPE;

            struct QuestAdd : Packet<QuestAdd> {
                GW::Constants::QuestID quest_id;
                GW::GamePos marker;
                GW::Constants::MapID map_to;
                uint32_t log_state;
                wchar_t location[8];
                wchar_t name[8];
                wchar_t npc[8];
                GW::Constants::MapID map_from;
            };
            constexpr uint32_t Packet<QuestAdd>::STATIC_HEADER = GAME_SMSG_QUEST_ADD;

            static_assert(sizeof(QuestAdd) == 0x50);

            struct NpcGeneralStats : Packet<NpcGeneralStats> {
                uint32_t npc_id;
                uint32_t file_id;
                uint32_t data1;
                uint32_t scale;
                uint32_t data2;
                uint32_t flags;
                uint32_t profession;
                uint32_t level;
                wchar_t name[8];
            };
            constexpr uint32_t Packet<NpcGeneralStats>::STATIC_HEADER = GAME_SMSG_NPC_UPDATE_PROPERTIES;

            // NPC model file (?)
            struct NPCModelFile : Packet<NPCModelFile> {
                uint32_t npc_id;
                uint32_t count;
                uint32_t data[8];
            };
            constexpr uint32_t Packet<NPCModelFile>::STATIC_HEADER = GAME_SMSG_NPC_UPDATE_MODEL;

            struct PlayerJoinInstance : Packet<PlayerJoinInstance> {
                uint32_t player_number;
                uint32_t agent_id;
                uint32_t file_id1; // dword
                uint32_t secondary_profession; // byte
                uint32_t unk3; // dword
                uint32_t file_id2; // dword
                wchar_t player_name[32];
            };
            constexpr uint32_t Packet<PlayerJoinInstance>::STATIC_HEADER = GAME_SMSG_AGENT_CREATE_PLAYER;

            struct PlayerLeaveInstance : Packet<PlayerLeaveInstance> {
                uint32_t player_number;
            };
            constexpr uint32_t Packet<PlayerLeaveInstance>::STATIC_HEADER = GAME_SMSG_AGENT_DESTROY_PLAYER;

            // Define chat message
            struct MessageCore : Packet<MessageCore> {
                wchar_t message[122]; // prefixType="int16"
            };
            constexpr uint32_t Packet<MessageCore>::STATIC_HEADER = GAME_SMSG_CHAT_MESSAGE_CORE;

            // Deliver chat message (no owner)
            struct MessageServer : Packet<MessageServer> {
                uint32_t id; // some kind of ID of the affected target
                uint32_t channel; // enum ChatChannel above.
            };
            constexpr uint32_t Packet<MessageServer>::STATIC_HEADER = GAME_SMSG_CHAT_MESSAGE_SERVER;

            // Deliver chat message (sender is an NPC)
            struct MessageNPC : Packet<MessageNPC> {
                uint32_t agent_id;
                uint32_t channel; // enum ChatChannel above.
                wchar_t sender_name[8];
            };
            constexpr uint32_t Packet<MessageNPC>::STATIC_HEADER = GAME_SMSG_CHAT_MESSAGE_NPC;

            // Deliver chat message (player sender in guild or alliance chat)
            struct MessageGlobal : Packet<MessageGlobal> {
                uint32_t channel; // enum ChatChannel above.
                wchar_t sender_name[32]; // full in-game name
                wchar_t sender_guild[6]; // guild tag for alliance chat, empty for guild chat
            };
            constexpr uint32_t Packet<MessageGlobal>::STATIC_HEADER = GAME_SMSG_CHAT_MESSAGE_GLOBAL;

            // Deliver chat message (player sender in the instance)
            struct MessageLocal : Packet<MessageLocal> {
                uint32_t player_number; // PlayerNumber of the sender
                Chat::Channel channel; // enum ChatChannel above.
            };
            constexpr uint32_t Packet<MessageLocal>::STATIC_HEADER = GAME_SMSG_CHAT_MESSAGE_LOCAL;

            // Alcohol Post Process Effect
            struct PostProcess : Packet<PostProcess> {
                uint32_t level;
                uint32_t tint;
            };
            constexpr uint32_t Packet<PostProcess>::STATIC_HEADER = GAME_SMSG_POST_PROCESS;

            struct DungeonReward : Packet<DungeonReward> {
                uint32_t experience;
                uint32_t gold;
                uint32_t skill_points;
            };
            constexpr uint32_t Packet<DungeonReward>::STATIC_HEADER = GAME_SMSG_DUNGEON_REWARD;

            struct ScreenShake : Packet<ScreenShake> {
                uint32_t unk1;
                uint32_t unk2;
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<ScreenShake>::STATIC_HEADER = GAME_SMSG_SCREEN_SHAKE;

            struct AgentUnk2 : Packet<AgentUnk2> {
                uint32_t agent_id;
                uint32_t unk1; // 1 = minipet, 2 = Ally?, 3 = summon
                uint32_t unk2; // always 0
            };
            constexpr uint32_t Packet<AgentUnk2>::STATIC_HEADER = GAME_SMSG_NPC_UPDATE_WEAPONS;

            struct MercenaryHeroInfo : Packet<MercenaryHeroInfo> {
                uint32_t hero_id;
                uint32_t level;
                uint32_t primary;
                uint32_t secondary;
                uint32_t unknown[15]; // Appearance etc
                wchar_t name[20];
            };
            constexpr uint32_t Packet<MercenaryHeroInfo>::STATIC_HEADER = GAME_SMSG_MERCENARY_INFO;

            struct DialogButton : Packet<DialogButton> {
                uint32_t button_icon; // byte
                wchar_t message[128];
                uint32_t dialog_id;
                uint32_t skill_id; // Default 0xFFFFFFF
            };
            constexpr uint32_t Packet<DialogButton>::STATIC_HEADER = GAME_SMSG_DIALOG_BUTTON;

            struct DialogBody : Packet<DialogBody> {
                wchar_t message[122];
            };
            constexpr uint32_t Packet<DialogBody>::STATIC_HEADER = GAME_SMSG_DIALOG_BODY;

            struct DialogSender : Packet<DialogSender> {
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<DialogSender>::STATIC_HEADER = GAME_SMSG_DIALOG_SENDER;

            struct DataWindow : Packet<DataWindow> {
                uint32_t agent;
                uint32_t type; // 0=storage, 1=tournament, 2=records, 3=stylist
                uint32_t data;
            };
            constexpr uint32_t Packet<DataWindow>::STATIC_HEADER = GAME_SMSG_WINDOW_OPEN;

            struct WindowItems : Packet<WindowItems> {
                uint32_t count;
                uint32_t item_ids[16];
            };
            constexpr uint32_t Packet<WindowItems>::STATIC_HEADER = GAME_SMSG_WINDOW_ADD_ITEMS;

            struct WindowItemsEnd : Packet<WindowItemsEnd> {
                uint32_t unk1;
            };
            constexpr uint32_t Packet<WindowItemsEnd>::STATIC_HEADER = GAME_SMSG_WINDOW_ITEMS_END;

            struct ItemStreamEnd : Packet<ItemStreamEnd> { // AKA ItemPricesEnd
                uint32_t unk1;
            };
            constexpr uint32_t Packet<ItemStreamEnd>::STATIC_HEADER = GAME_SMSG_WINDOW_ITEM_STREAM_END;

            // Pings and drawing in compass
            struct CompassEvent : Packet<CompassEvent> {
                uint32_t Player;    // player who sent the ping (PlayerNumber)
                uint32_t SessionID; // Changes for different pings/lines/curves
                uint32_t NumberPts; // Number of points in the data, between 1 and 8
                struct {
                    short x;     // world coordinates divided by 10
                    short y;     // same
                } points[8];
                // there *might* be another 8 uint32_ts, but they look like noise and they are not relayed by the server to other players
            };
            constexpr uint32_t Packet<CompassEvent>::STATIC_HEADER = GAME_SMSG_COMPASS_DRAWING;

            struct MapsUnlocked : Packet<MapsUnlocked> {
                uint32_t missions_bonus_length;
                uint32_t missions_bonus[32];
                uint32_t missions_completed_length;
                uint32_t missions_completed[32];
                uint32_t missions_bonus_hm_length;
                uint32_t missions_bonus_hm[32];
                uint32_t missions_completed_hm_length;
                uint32_t missions_completed_hm[32];
                uint32_t unlocked_map_length;
                uint32_t unlocked_map[32];
            };
            constexpr uint32_t Packet<MapsUnlocked>::STATIC_HEADER = GAME_SMSG_MAPS_UNLOCKED;

            struct CartographyData : Packet<CartographyData> {
                uint32_t data_length;
                uint32_t data[64];
            };
            constexpr uint32_t Packet<CartographyData>::STATIC_HEADER = GAME_SMSG_CARTOGRAPHY_DATA;

            struct AgentScale : Packet<AgentScale> {
                uint32_t agent_id;
                uint32_t scale;
            };
            constexpr uint32_t Packet<AgentScale>::STATIC_HEADER = GAME_SMSG_AGENT_UPDATE_SCALE;

            struct AgentName : Packet<AgentName> {
                uint32_t agent_id;
                wchar_t name_enc[40];
            };
            constexpr uint32_t Packet<AgentName>::STATIC_HEADER = GAME_SMSG_AGENT_UPDATE_NPC_NAME;

            struct DisplayDialogue : Packet<DisplayDialogue> {
                uint32_t agent_id;
                wchar_t name[32];
                uint32_t type;
                wchar_t message[122];
            };
            constexpr uint32_t Packet<DisplayDialogue>::STATIC_HEADER = GAME_SMSG_AGENT_DISPLAY_DIALOG;

            // agent animation lock (and probably something else)
            struct GenericValue : Packet<GenericValue> {
                uint32_t value_id;
                uint32_t agent_id;
                uint32_t value;
            };
            constexpr uint32_t Packet<GenericValue>::STATIC_HEADER = GAME_SMSG_AGENT_ATTR_UPDATE_INT;

            // Update Target Generic Value
            struct GenericValueTarget : Packet<GenericValueTarget> {
                uint32_t Value_id;
                uint32_t target; // agent id
                uint32_t caster; // agent id
                uint32_t value;
            };
            constexpr uint32_t Packet<GenericValueTarget>::STATIC_HEADER = GAME_SMSG_AGENT_ATTR_UPDATE_INT_TARGET;

            // Update Target Generic Value
            struct PlayEffect : Packet<PlayEffect> {
                Vec2f coords;
                uint32_t plane;
                uint32_t agent_id;
                uint32_t effect_id;
                uint32_t data5;
                uint32_t data6;
            };
            constexpr uint32_t Packet<PlayEffect>::STATIC_HEADER = GAME_SMSG_AGENT_ATTR_PLAY_EFFECT;

            // Update Target Generic Value
            struct GenericFloat : Packet<GenericFloat> {
                uint32_t type;
                uint32_t agent_id;
                float value;
            };
            constexpr uint32_t Packet<GenericFloat>::STATIC_HEADER = GAME_SMSG_AGENT_ATTR_UPDATE_FLOAT;

            // damage or healing done packet, but also has other purposes.
            // to be investigated further.
            // all types have their value in the float field 'value'.
            // in all types the value is in percentage, unless otherwise specified.
            // the value can be negative (e.g. damage, sacrifice)
            // or positive (e.g. heal, lifesteal).
            struct GenericModifier : Packet<GenericModifier> {
                uint32_t type;         // type as specified above in P156_Type
                uint32_t target_id;    // agent id of who is affected by the change
                uint32_t cause_id;     // agent id of who caused the change
                float value;           // value, often in percentage (e.g. %hp)
            };
            constexpr uint32_t Packet<GenericModifier>::STATIC_HEADER = GAME_SMSG_AGENT_ATTR_UPDATE_FLOAT_TARGET;

            // Projectile launched from an agent
            // Can be from a martial weapon (spear, bow), or projectile-launching skill
            // also acts as an attack_finished packet for ranged weapons, if `is_attack == 1`
            struct AgentProjectileLaunched : Packet<AgentProjectileLaunched> {
                uint32_t agent_id;
                Vec2f destination;
                uint32_t unk1;          // word : 0 ?
                uint32_t unk2;          // dword: changes with projectile animation model
                uint32_t unk3;          // dword: value (143) for all martial weapons (?), n values for n skills (?)
                uint32_t unk4;          // dword: 1 ?
                uint32_t is_attack;     // byte
            };
            constexpr uint32_t Packet<AgentProjectileLaunched>::STATIC_HEADER = GAME_SMSG_AGENT_PROJECTILE_LAUNCHED;

            // agent text above head
            struct SpeechBubble : Packet<SpeechBubble> {
                uint32_t agent_id;
                wchar_t message[122];
            };
            constexpr uint32_t Packet<SpeechBubble>::STATIC_HEADER = GAME_SMSG_SPEECH_BUBBLE;

            struct PartyAllyAdd : Packet<PartyAllyAdd> { // When an NPC is added as an ally to your party.
                uint32_t agent_id;
                uint32_t allegiance_bits;
                uint32_t agent_type; // Bitwise field. 0x20000000 = NPC | PlayerNumber, 0x30000000 = Player | PlayerNumber, 0x00000000 = Signpost
            };
            constexpr uint32_t Packet<PartyAllyAdd>::STATIC_HEADER = GAME_SMSG_AGENT_CREATE_NPC;

            // agent change model
            struct AgentModel : Packet<AgentModel> {
                uint32_t agent_id;
                uint32_t model_id;
            };
            constexpr uint32_t Packet<AgentModel>::STATIC_HEADER = GAME_SMSG_UPDATE_AGENT_MODEL;

            // Changes the number above the player's head when leading a party
            struct PartyUpdateSize : Packet<PartyUpdateSize> {
                uint32_t player_id;
                uint32_t size;
            };
            constexpr uint32_t Packet<PartyUpdateSize>::STATIC_HEADER = GAME_SMSG_UPDATE_AGENT_PARTYSIZE;

            struct ObjectiveAdd : Packet<ObjectiveAdd> {
                uint32_t objective_id;
                uint32_t type;
                wchar_t name[128];
            };
            constexpr uint32_t Packet<ObjectiveAdd>::STATIC_HEADER = GAME_SMSG_MISSION_OBJECTIVE_ADD;

            struct ObjectiveDone : Packet<ObjectiveDone> {
                uint32_t objective_id;
            };
            constexpr uint32_t Packet<ObjectiveDone>::STATIC_HEADER = GAME_SMSG_MISSION_OBJECTIVE_COMPLETE;

            struct ObjectiveUpdateName : Packet<ObjectiveUpdateName> {
                uint32_t objective_id;
                wchar_t objective_name[128];
            };
            constexpr uint32_t Packet<ObjectiveUpdateName>::STATIC_HEADER = GAME_SMSG_MISSION_OBJECTIVE_UPDATE_STRING;

            struct OpenMerchantWindow : Packet<OpenMerchantWindow> {
                uint32_t type;
                uint32_t unk;
            };
            constexpr uint32_t Packet<OpenMerchantWindow>::STATIC_HEADER = GAME_SMSG_WINDOW_MERCHANT;

            struct WindowOwner : Packet<WindowOwner> {
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<WindowOwner>::STATIC_HEADER = GAME_SMSG_WINDOW_OWNER;

            struct TransactionDone : Packet<TransactionDone> {
                uint32_t unk1;
            };
            constexpr uint32_t Packet<TransactionDone>::STATIC_HEADER = GAME_SMSG_TRANSACTION_DONE;
            struct UpdateSkillbarSkill : Packet<UpdateSkillbarSkill> {
                uint32_t agent_id;
                uint32_t skill_slot;
                uint32_t skill_id;
            };
            constexpr uint32_t Packet<UpdateSkillbarSkill>::STATIC_HEADER = GAME_SMSG_SKILLBAR_UPDATE_SKILL;

            // Used to duplicate skills in skill window i.e. more than 1 signet of capture. Packet is received after map is loaded
            struct UpdateSkillCountAfterMapLoad : Packet<UpdateSkillCountAfterMapLoad> {
                uint32_t skill_id;
                uint32_t count;
            };
            constexpr uint32_t Packet<UpdateSkillCountAfterMapLoad>::STATIC_HEADER = GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1;

            // Used to duplicate skills in skill window i.e. more than 1 signet of capture. Packet is received during initial character load
            struct UpdateSkillCountPreMapLoad : Packet<UpdateSkillCountPreMapLoad> {
                uint32_t skill_id;
                uint32_t count;
            };
            constexpr uint32_t Packet<UpdateSkillCountPreMapLoad>::STATIC_HEADER = GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_2;

            // Skill Activate (begin casting)
            struct SkillActivate : Packet<SkillActivate> {
                uint32_t agent_id;
                uint32_t skill_id;
                uint32_t skill_instance;
            };
            constexpr uint32_t Packet<SkillActivate>::STATIC_HEADER = GAME_SMSG_SKILL_ACTIVATE;

            struct SkillRecharge : Packet<SkillRecharge> {
                uint32_t agent_id;
                uint32_t skill_id;
                uint32_t skill_instance;
                uint32_t recharge;
            };
            constexpr uint32_t Packet<SkillRecharge>::STATIC_HEADER = GAME_SMSG_SKILL_RECHARGE;

            // update agent state
            struct AgentState : Packet<AgentState> {
                uint32_t agent_id;
                uint32_t state; // bitmap of agent states (0 neutral, 2 condition, 16 dead, 128 enchanted, 1024 degen?, 2048 hexed, 8192 sitting, etc)
            };
            constexpr uint32_t Packet<AgentState>::STATIC_HEADER = GAME_SMSG_AGENT_UPDATE_EFFECTS;

            struct MapLoaded : Packet<MapLoaded> {
                // uint32_t
            };
            constexpr uint32_t Packet<MapLoaded>::STATIC_HEADER = GAME_SMSG_INSTANCE_LOADED;

            struct QuotedItemPrice : Packet<QuotedItemPrice> {
                uint32_t itemid;
                uint32_t price;
            };
            constexpr uint32_t Packet<QuotedItemPrice>::STATIC_HEADER = GAME_SMSG_ITEM_PRICE_QUOTE;

            struct WindowPrices : Packet<WindowPrices> {
                uint32_t count;
                uint32_t item_ids[16];
            };
            constexpr uint32_t Packet<WindowPrices>::STATIC_HEADER = GAME_SMSG_ITEM_PRICES;

            struct VanquishProgress : Packet<VanquishProgress> {
                uint32_t foes_killed;
                uint32_t foes_remaining;
            };
            constexpr uint32_t Packet<VanquishProgress>::STATIC_HEADER = GAME_SMSG_VANQUISH_PROGRESS;

            struct VanquishComplete : Packet<VanquishComplete> {
                uint32_t map_id;
                uint32_t experience;
                uint32_t gold;
            };
            constexpr uint32_t Packet<VanquishComplete>::STATIC_HEADER = GAME_SMSG_VANQUISH_COMPLETE;

            struct CinematicPlay : Packet<CinematicPlay> {
                uint32_t play;
            };
            constexpr uint32_t Packet<CinematicPlay>::STATIC_HEADER = GAME_SMSG_CINEMATIC_START;

            // e.g. map doors start opening or closing. AKA "update object animation"
            struct ManipulateMapObject : Packet<ManipulateMapObject> {
                uint32_t object_id; // Door ID
                uint32_t animation_type; // (3 = door closing, 9 = ???, 16 = door opening)
                uint32_t animation_stage; // (2 = start, 3 = stop)
            };
            constexpr uint32_t Packet<ManipulateMapObject>::STATIC_HEADER = GAME_SMSG_MANIPULATE_MAP_OBJECT;

            // e.g. map doors stop opening or closing. "update object state"
            struct ManipulateMapObject2 : Packet<ManipulateMapObject2> {
                uint32_t object_id; // Door ID
                uint32_t unk1; //
                uint32_t state; // Open = 1, Closed = 0
            };
            constexpr uint32_t Packet<ManipulateMapObject2>::STATIC_HEADER = GAME_SMSG_MANIPULATE_MAP_OBJECT2;

            struct TownAllianceObject : Packet<TownAllianceObject> {
                uint32_t map_id;
                uint32_t rank;
                uint32_t allegiance;
                uint32_t faction;
                wchar_t name[32];
                wchar_t tag[5];
                uint32_t cape_bg_color;
                uint32_t cape_detail_color;
                uint32_t cape_emblem_color;
                uint32_t cape_shape;
                uint32_t cape_detail;
                uint32_t cape_emblem;
                uint32_t cape_trim;
            };
            constexpr uint32_t Packet<TownAllianceObject>::STATIC_HEADER = GAME_SMSG_TOWN_ALLIANCE_OBJECT;

            // Info about any guilds applicable to current outpost.
            // NOTE: When entering a guild hall, that guild will always be the first added (local_id = 1).
            struct GuildGeneral : Packet<GuildGeneral> {
                uint32_t local_id;
                uint32_t ghkey[4]; // blob[16]
                wchar_t name[32];
                wchar_t tag[5];
                uint32_t cape_bg_color;
                uint32_t cape_detail_color;
                uint32_t cape_emblem_color;
                uint32_t cape_shape;
                uint32_t cape_detail;
                uint32_t cape_emblem;
                uint32_t cape_trim;
                uint32_t unk1; // Word
                uint32_t unk2; // byte
                uint32_t unk3; // byte
                uint32_t faction;
                uint32_t unk4; // dword
                uint32_t rank;
                uint32_t allegiance;
                uint32_t unk5; // byte
            };
            constexpr uint32_t Packet<GuildGeneral>::STATIC_HEADER = GAME_SMSG_GUILD_GENERAL_INFO;

            struct GuildPlayerInfo : Packet<GuildPlayerInfo> {
                wchar_t invited_name[20];
                wchar_t current_name[20];
                wchar_t invited_by[20];
                wchar_t context_info[64];
                uint32_t unk; // Something to do with promoted date?
                uint32_t minutes_since_login;
                uint32_t join_date; // This is seconds, but since when? 1471089600 ?
                uint32_t status;
                uint32_t member_type;
            };
            constexpr uint32_t Packet<GuildPlayerInfo>::STATIC_HEADER = GAME_SMSG_GUILD_PLAYER_INFO;

            struct ItemUpdateOwner : Packet<ItemUpdateOwner> {
                GW::ItemID  item_id;
                GW::AgentID owner_agent_id;
                float       seconds_reserved;
            };
            constexpr uint32_t Packet<ItemUpdateOwner>::STATIC_HEADER = GAME_SMSG_ITEM_UPDATE_OWNER;

            struct ItemCustomisedForPlayer : Packet<ItemCustomisedForPlayer> {
                uint32_t item_id;
                wchar_t player_name[32];
            };
            constexpr uint32_t Packet<ItemCustomisedForPlayer>::STATIC_HEADER = GAME_SMSG_ITEM_UPDATE_NAME;

            // Gold added to inventory
            struct CharacterAddGold : Packet<CharacterAddGold> {
                uint32_t unk;
                uint32_t gold;
            };
            constexpr uint32_t Packet<CharacterAddGold>::STATIC_HEADER = GAME_SMSG_GOLD_CHARACTER_ADD;

            struct SalvageConsumeItem : Packet<SalvageConsumeItem> {
                uint32_t salvage_session_id;
                uint32_t item_id;
            };
            constexpr uint32_t Packet<SalvageConsumeItem>::STATIC_HEADER = GAME_SMSG_ITEM_REMOVE;

            // Gold removed from inventory
            struct CharacterRemoveGold : Packet<CharacterRemoveGold> {
                uint32_t unk; // some kind of id? but neither agentid nor playerid
                uint32_t gold;
            };
            constexpr uint32_t Packet<CharacterRemoveGold>::STATIC_HEADER = GAME_SMSG_GOLD_CHARACTER_REMOVE;

            struct ItemGeneral : Packet<ItemGeneral> {
                uint32_t item_id;
                uint32_t model_file_id;
                uint32_t type;
                uint32_t unk1;
                uint32_t extra_id; // Dye color
                uint32_t materials;
                uint32_t unk2;
                uint32_t interaction; // Flags
                uint32_t price;
                uint32_t model_id;
                uint32_t quantity;
                wchar_t* enc_name;
                uint32_t mod_struct_size;
                uint32_t* mod_struct;
            };
            constexpr uint32_t Packet<ItemGeneral>::STATIC_HEADER = GAME_SMSG_ITEM_GENERAL_INFO;

            struct SalvageSession : Packet<SalvageSession> {
                uint32_t salvage_session_id;
                uint32_t salvage_item_id;
                uint32_t salvagable_count;
                uint32_t salvagable_item_ids[3];
            };
            constexpr uint32_t Packet<SalvageSession>::STATIC_HEADER = GAME_SMSG_ITEM_SALVAGE_SESSION_START;

            struct SalvageSessionCancel : Packet<SalvageSessionCancel> {
            };
            constexpr uint32_t Packet<SalvageSessionCancel>::STATIC_HEADER = GAME_SMSG_ITEM_SALVAGE_SESSION_CANCEL;

            struct SalvageSessionDone : Packet<SalvageSessionDone> {
            };
            constexpr uint32_t Packet<SalvageSessionDone>::STATIC_HEADER = GAME_SMSG_ITEM_SALVAGE_SESSION_DONE;

            struct SalvageSessionItemKept : Packet<SalvageSessionItemKept> {
            };
            constexpr uint32_t Packet<SalvageSessionItemKept>::STATIC_HEADER = GAME_SMSG_ITEM_SALVAGE_SESSION_ITEM_KEPT;

            struct SalvageSessionSuccess : Packet<SalvageSessionSuccess> {
            };
            constexpr uint32_t Packet<SalvageSessionSuccess>::STATIC_HEADER = GAME_SMSG_ITEM_SALVAGE_SESSION_SUCCESS;

            // JumboMessage represents a message strewn across the center of the screen in big red or green characters.
            // Things like moral boosts, flag captures, victory, defeat...
            struct JumboMessage : GW::Packet::StoC::Packet<JumboMessage> {
                uint8_t type;   // JumboMessageType
                uint32_t value; // JumboMessageValue
            };
            const uint32_t GW::Packet::StoC::Packet<JumboMessage>::STATIC_HEADER = GAME_SMSG_JUMBO_MESSAGE;

            struct InstanceLoadFile : Packet<InstanceLoadFile> {
                uint32_t map_fileID;
                Vec2f spawn_point;
                uint16_t spawn_plane;
                uint8_t unk1;
                uint8_t unk2;
                uint8_t unk3[8];
            };
            constexpr uint32_t Packet<InstanceLoadFile>::STATIC_HEADER = GAME_SMSG_INSTANCE_LOAD_SPAWN_POINT;

            struct InstanceLoadInfo : Packet<InstanceLoadInfo> {
                uint32_t agent_id;
                uint32_t map_id;
                uint32_t is_explorable;
                uint32_t district;
                uint32_t language;
                uint32_t is_observer;
            };
            constexpr uint32_t Packet<InstanceLoadInfo>::STATIC_HEADER = GAME_SMSG_INSTANCE_LOAD_INFO;

            struct CreateMissionProgress : Packet<CreateMissionProgress> {
                uint8_t id;
                wchar_t unk1[122];
                wchar_t unk2[122];
                uint8_t unk3;
                int32_t pips;         // probably only signed char used
                float   regeneration; // 0 ... 1
                float   filled;       // 0 ... 1
                uint8_t color[4];     // RGBA, not including border
            };
            constexpr uint32_t Packet<CreateMissionProgress>::STATIC_HEADER = GAME_SMSG_CREATE_MISSION_PROGRESS;

            struct UpdateMissionProgress : Packet<UpdateMissionProgress> {
                uint8_t id;
                uint8_t unk1;
                int32_t pips;         // only signed char used
                float   regeneration; // 0 ... 1
                float   filled;       // 0 ... 1
                uint8_t color[4];     // RGBA, not including border
            };
            constexpr uint32_t Packet<UpdateMissionProgress>::STATIC_HEADER = GAME_SMSG_UPDATE_MISSION_PROGRESS;

            struct GameSrvTransfer : Packet<GameSrvTransfer> {
                uint8_t host[24]; // ip of the game server
                uint32_t token1; // world id
                uint32_t region; // uint8_t
                uint32_t map_id; // uint8_t
                uint32_t is_explorable; // uint8_t
                uint32_t token2; // player id
            };
            constexpr uint32_t Packet<GameSrvTransfer>::STATIC_HEADER = GAME_SMSG_TRANSFER_GAME_SERVER_INFO;

            struct DoACompleteZone : Packet<DoACompleteZone> {
                wchar_t message[122];
            };
            constexpr uint32_t Packet<DoACompleteZone>::STATIC_HEADER = GAME_SMSG_DOA_COMPLETE_ZONE;

            struct ErrorMessage : Packet<ErrorMessage> {
                uint32_t message_id;
            };
            constexpr uint32_t Packet<ErrorMessage>::STATIC_HEADER = GAME_SMSG_INSTANCE_TRAVEL_TIMER;

            /* Party Invites */
            struct PartyHenchmanAdd : Packet<PartyHenchmanAdd> {
                uint32_t party_id;
                uint32_t agent_id;
                wchar_t name_enc[8];
                uint32_t profession;
                uint32_t level;
            };
            constexpr uint32_t Packet<PartyHenchmanAdd>::STATIC_HEADER = GAME_SMSG_PARTY_HENCHMAN_ADD;

            struct PartyHenchmanRemove : Packet<PartyHenchmanRemove> {
                uint32_t party_id;
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<PartyHenchmanRemove>::STATIC_HEADER = GAME_SMSG_PARTY_HENCHMAN_REMOVE;

            struct PartyHeroAdd : Packet<PartyHeroAdd> {
                uint32_t party_id;
                uint32_t owner_player_number;
                uint32_t agent_id;
                uint32_t hero_id;
                uint32_t level;
            };
            constexpr uint32_t Packet<PartyHeroAdd>::STATIC_HEADER = GAME_SMSG_PARTY_HERO_ADD;

            struct PartyHeroRemove : Packet<PartyHeroRemove> {
                uint32_t party_id;
                uint32_t owner_player_number;
                uint32_t agent_id;
            };
            constexpr uint32_t Packet<PartyHeroRemove>::STATIC_HEADER = GAME_SMSG_PARTY_HERO_REMOVE;


            // Invite sent to another party
            struct PartyInviteSent_Create : Packet<PartyInviteSent_Create> {
                uint32_t target_party_id;
            };
            constexpr uint32_t Packet<PartyInviteSent_Create>::STATIC_HEADER = GAME_SMSG_PARTY_INVITE_ADD;
            // Invite received from another party
            struct PartyInviteReceived_Create : Packet<PartyInviteReceived_Create> {
                uint32_t target_party_id; // word
            };
            constexpr uint32_t Packet<PartyInviteReceived_Create>::STATIC_HEADER = GAME_SMSG_PARTY_JOIN_REQUEST;

            // Remove a sent party invite. Invitation has been cancelled.
            struct PartyInviteSent_Cancel : Packet<PartyInviteSent_Cancel> {
                uint32_t target_party_id; // word
            };
            constexpr uint32_t Packet<PartyInviteSent_Cancel>::STATIC_HEADER = GAME_SMSG_PARTY_INVITE_CANCEL;

            // Remove a received party invite. Invitation has been cancelled.
            struct PartyInviteReceived_Cancel : Packet<PartyInviteReceived_Cancel> {
                uint32_t target_party_id; // word
            };
            constexpr uint32_t Packet<PartyInviteReceived_Cancel>::STATIC_HEADER = GAME_SMSG_PARTY_REQUEST_CANCEL;

            // Remove a received party invite. Invitation has been either accepted or rejected (we dont know atm)
            struct PartyInviteSent_Response : Packet<PartyInviteSent_Response> {
                uint32_t target_party_id; // word
            };
            constexpr uint32_t Packet<PartyInviteSent_Response>::STATIC_HEADER = GAME_SMSG_PARTY_REQUEST_RESPONSE;

            // Remove a sent party invite. Invitation has been either accepted or rejected (we dont know atm)
            struct PartyInviteReceived_Response : Packet<PartyInviteReceived_Response> {
                uint32_t target_party_id; // word
            };
            constexpr uint32_t Packet<PartyInviteReceived_Response>::STATIC_HEADER = GAME_SMSG_PARTY_INVITE_RESPONSE;

            // A player in a party has been updated or added. Not necessarily yours.
            struct PartyPlayerAdd : Packet<PartyPlayerAdd> {
                uint32_t party_id;
                uint32_t player_id;
                uint32_t state; // 3 = Invited
            };
            constexpr uint32_t Packet<PartyPlayerAdd>::STATIC_HEADER = GAME_SMSG_PARTY_PLAYER_ADD;
            // Player has left party. Not necessarily yours.
            struct PartyPlayerRemove : Packet<PartyPlayerRemove> {
                uint32_t party_id;
                uint32_t player_id;
            };
            constexpr uint32_t Packet<PartyPlayerRemove>::STATIC_HEADER = GAME_SMSG_PARTY_PLAYER_REMOVE;
            // Player in party has toggled ready. Not necessarily yours.
            struct PartyPlayerReady : Packet<PartyPlayerReady> {
                uint32_t party_id;
                uint32_t player_id;
                uint32_t is_ready;
            };
            constexpr uint32_t Packet<PartyPlayerReady>::STATIC_HEADER = GAME_SMSG_PARTY_PLAYER_READY;

            // When a new party is created:
            // 1.   PartyPlayerStreamStart packet is sent
            // 2.   PartyPlayerAdd packet per member
            //      PartyHeroAdd packet per hero
            //      PartyHenchmanAdd packer per henchman
            // 3. PartyPlayerStreamEnd packet is sent
            struct PartyPlayerStreamStart : Packet<PartyPlayerStreamStart> {
                uint32_t party_id; // word
            };
            constexpr uint32_t Packet<PartyPlayerStreamStart>::STATIC_HEADER = GAME_SMSG_PARTY_CREATE;
            struct PartyPlayerStreamEnd : Packet<PartyPlayerStreamEnd> {
                uint32_t party_id; // word
            };
            constexpr uint32_t Packet<PartyPlayerStreamEnd>::STATIC_HEADER = GAME_SMSG_PARTY_MEMBER_STREAM_END;

            struct PartyDefeated : Packet<PartyDefeated> {
            };
            constexpr uint32_t Packet<PartyDefeated>::STATIC_HEADER = GAME_SMSG_PARTY_DEFEATED;

            struct PartyLock : Packet<PartyLock> { // Sent when party window is locked/unlocked e.g. pending mission entry
                uint32_t unk1; // 2 = locked?
                uint32_t unk2; // 1 = locked?
                wchar_t unk3[8];
            };
            constexpr uint32_t Packet<PartyLock>::STATIC_HEADER = GAME_SMSG_PARTY_LOCK;

            struct CapeVisibility : Packet<CapeVisibility> {
                uint32_t agent_id;
                uint32_t visible; // 0 or 1
            };
            constexpr uint32_t Packet<CapeVisibility>::STATIC_HEADER = GAME_SMSG_AGENT_DISPLAY_CAPE;

            struct InstanceLoadStart : Packet<InstanceLoadStart> {
                uint32_t unk1;
                uint32_t unk2;
                uint32_t unk3;
                uint32_t unk4;
            };
            constexpr uint32_t Packet<InstanceLoadStart>::STATIC_HEADER = GAME_SMSG_INSTANCE_LOAD_HEAD;

            struct ItemGeneral_FirstID : Packet<ItemGeneral_FirstID> {
                uint32_t item_id;
                uint32_t model_file_id;
                uint32_t type;
                uint32_t unk1;
                uint32_t extra_id;
                uint32_t materials;
                uint32_t unk2;
                uint32_t interaction;
                uint32_t price;
                uint32_t model_id;
                uint32_t quantity;
                wchar_t enc_name[64];
                uint32_t mod_struct_size;
                uint32_t mod_struct[64];
            };
            constexpr uint32_t Packet<ItemGeneral_FirstID>::STATIC_HEADER = GAME_SMSG_ITEM_GENERAL_INFO;

            struct ItemGeneral_ReuseID : ItemGeneral_FirstID {};
            constexpr uint32_t Packet<ItemGeneral_ReuseID>::STATIC_HEADER = GAME_SMSG_ITEM_REUSE_ID;

            struct PlayerIsPartyLeader : Packet<PlayerIsPartyLeader> {
                uint32_t is_leader; // bool
            };
            constexpr uint32_t Packet<PlayerIsPartyLeader>::STATIC_HEADER = GAME_SMSG_PARTY_YOU_ARE_LEADER;

            struct RemoveEffect : Packet<RemoveEffect> {
                GW::AgentID agent_id;
                unsigned effect_id; // not GW::SkillID
            };
            constexpr uint32_t Packet<RemoveEffect>::STATIC_HEADER = GAME_SMSG_EFFECT_REMOVED;

            struct SkillRecharged : Packet<SkillRecharged> {
                uint32_t agent_id;
                uint32_t skill_id;
                uint32_t skill_instance;
            };
            constexpr uint32_t Packet<SkillRecharged>::STATIC_HEADER = GAME_SMSG_SKILL_RECHARGED;

            // Corrected version of GuildGeneral
            struct UpdateGuildInfo : Packet<UpdateGuildInfo> {
                uint32_t guild_id;
                uint32_t gh_key[4];
                wchar_t  guild_name[32];
                wchar_t  guild_tag[6]; // client accepts only 4 + \0
                uint32_t guild_features;
                uint32_t territory;
                uint32_t cape_background_color; // & 0xF0 = hue, & 0x0F = brightness
                uint32_t cape_detail_color;     // & 0xF0 = hue, & 0x0F = brightness
                uint32_t cape_emblem_color;     // & 0xF0 = hue, & 0x0F = brightness
                uint32_t cape_shape;            // 0 -   8
                uint32_t cape_detail;           // 0 -  31
                uint32_t cape_emblem;           // 0 - 173
                uint32_t cape_trim;             // 0 -  13
                uint32_t faction;
                uint32_t factions_count;
                uint32_t qualifier_points;
                uint32_t rating;
                uint32_t rank;
                uint32_t unk;
            };
            constexpr uint32_t Packet<UpdateGuildInfo>::STATIC_HEADER = GAME_SMSG_GUILD_GENERAL_INFO;

            struct UpdateTitle : Packet<UpdateTitle> {
                uint32_t title_id;
                uint32_t new_value;
            };
            constexpr uint32_t Packet<UpdateTitle>::STATIC_HEADER = GAME_SMSG_TITLE_UPDATE;

            struct TitleInfo : Packet<TitleInfo> {
                uint32_t title_id;
                uint32_t unk_flags;
                uint32_t value;
                uint32_t h000c; // Maybe an id for current rank
                uint32_t current_rank_minimum_value;
                uint32_t h0014; // Maybe an id for next rank
                uint32_t next_rank_minimum_value;
                uint32_t num_ranks;
                uint32_t h0020; // Maybe an id for e.g. base rank?  Matches h000c for wisdom on my r1 wisdom alt acc
                wchar_t progress_display_enc_string[8];
                wchar_t mouseover_hint_enc_string[8];
            };
            constexpr uint32_t Packet<TitleInfo>::STATIC_HEADER = GAME_SMSG_TITLE_TRACK_INFO;

            enum class PlayerAttrId {
                Experience = 0,
                Kurzick_Faction,
                Kurzick_Faction_Total,
                Luxon_Faction,
                Luxon_Faction_Total,
                Imperial_Faction,
                Imperial_Faction_Total,
                Hero_Skill_Points,
                Hero_Skill_Points_Total,
                Level,
                Morale_Percent,
                Balthazar_Faction,
                Balthazar_Faction_Total,
                Skill_Points,
                Skill_Points_Total
            };

            struct PlayerAttrUpdate : Packet<PlayerAttrUpdate> {
                uint32_t attr_id;
                int32_t modifier;
            };
            constexpr uint32_t Packet<PlayerAttrUpdate>::STATIC_HEADER = GAME_SMSG_PLAYER_ATTR_UPDATE;

            struct PlayerAttrSet : Packet<PlayerAttrSet> {
                uint32_t experience;
                uint32_t kurzick_faction;
                uint32_t kurzick_faction_total;
                uint32_t luxon_faction;
                uint32_t luxon_faction_total;
                uint32_t imperial_faction;
                uint32_t imperial_faction_total;
                uint32_t hero_skill_points;
                uint32_t hero_skill_points_total;
                uint32_t level;
                uint32_t morale_percent; // Percentage of health/energy available due to morale/death penalty.  Ranges from 40 to 110.
                uint32_t balthazar_faction;
                uint32_t balthazar_faction_total;
                uint32_t skill_points;
                uint32_t skill_points_total;
            };
            constexpr uint32_t Packet<PlayerAttrSet>::STATIC_HEADER = GAME_SMSG_PLAYER_ATTR_SET;
        }
    }
}
