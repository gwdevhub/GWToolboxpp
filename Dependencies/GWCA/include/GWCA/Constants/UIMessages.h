namespace GW {
    enum class CallTargetType : uint32_t;
    enum class WorldActionId : uint32_t;
    struct GamePos;
    struct Effect;
    struct Vec2f;
    typedef uint32_t AgentID;

    namespace Merchant {
        enum class TransactionType : uint32_t;
    }
    namespace Constants {
        enum class Language;
        enum class MapID : uint32_t;
        enum class InstanceType;
        enum class QuestID : uint32_t;
        enum class SkillID : uint32_t;
    }
    namespace Chat {
        enum Channel : int;
        typedef uint32_t Color;
    }

    namespace Chat {
        enum Channel : int;
    }
    namespace SkillbarMgr {
        struct SkillTemplate;
    }
    namespace UI {
        struct WindowPosition;
        enum class FlagPreference : uint32_t;
        enum class NumberPreference : uint32_t;
        enum class StringPreference : uint32_t;
        enum class EnumPreference : uint32_t;
        struct CompassPoint;

        enum class UIMessage : uint32_t {
            kNone = 0x0,                    // 0x0
            kFrameMessage_0x1,              // 0x1
            kFrameMessage_0x2,              // 0x2
            kFrameMessage_0x3,              // 0x3
            kFrameMessage_0x4,              // 0x4
            kFrameMessage_0x5,              // 0x5
            kFrameMessage_0x6,              // 0x6
            kFrameMessage_0x7,              // 0x7
            kResize,                        // 0x8
            kInitFrame,                     // 0x9
            kFrameMessage_0xa,              // 0xa
            kDestroyFrame,                  // 0xb
            kFrameDisabledChange,           // 0xc, wparam = is_enabled
            kFrameMessage_0xd,              // 0xd
            kFrameMessage_0xe,              // 0xe
            kFrameMessage_0xf,              // 0xf
            kFrameMessage_0x10,             // 0x10
            kFrameMessage_0x11,             // 0x11
            kFrameMessage_0x12,             // 0x12
            kFrameMessage_0x13,             // 0x13
            kFrameMessage_0x14,             // 0x14
            kFrameMessage_0x15,             // 0x15
            kFrameMessage_0x16,             // 0x16
            kFrameMessage_0x17,             // 0x17
            kFrameMessage_0x18,             // 0x18
            kFrameMessage_0x19,             // 0x19
            kFrameMessage_0x1a,             // 0x1a
            kFrameMessage_0x1b,             // 0x1b
            kFrameMessage_0x1c,             // 0x1c
            kFrameMessage_0x1d,             // 0x1d
            kKeyDown = 0x20,                // 0x20, wparam = UIPacket::kKeyAction* - Updated from 0x1e to 0x20
            kSetFocus,                      // 0x21, wparam = 1 or 0
            kKeyUp,                         // 0x22, wparam = UIPacket::kKeyAction*
            kFrameMessage_0x21,             // 0x23
            kMouseClick,                    // 0x24, wparam = UIPacket::kMouseClick*
            kFrameMessage_0x23,             // 0x25
            kMouseCoordsClick,              // 0x26, wparam = UIPacket::kMouseCoordsClick*
            kFrameMessage_0x25,             // 0x27
            kMouseUp,                       // 0x28, wparam = UIPacket::kMouseCoordsClick*
            kFrameMessage_0x27,             // 0x29
            kFrameMessage_0x28,             // 0x2a
            kFrameMessage_0x29,             // 0x2b
            kFrameMessage_0x2a,             // 0x2c
            kFrameMessage_0x2b,             // 0x2d
            kToggleButtonDown,              // 0x2e
            kFrameMessage_0x2d,             // 0x2f
            kMouseClick2 = 0x31,            // 0x31, wparam = UIPacket::kMouseAction*
            kMouseAction,                   // 0x32, wparam = UIPacket::kMouseAction*
            kRenderFrame_0x30,              // 0x33
            kRenderFrame_0x31 = 0x35,       // 0x35
            kFrameVisibilityChanged,        // 0x36, wparam = is_visible
            kSetLayout,                     // 0x37
            kMeasureContent,                // 0x38
            kFrameMessage_0x35,             // 0x39
            kFrameMessage_0x36,             // 0x3a
            kRefreshContent,                // 0x3b
            kFrameMessage_0x38,             // 0x3c
            kFrameMessage_0x39,             // 0x3d
            kFrameMessage_0x3a,             // 0x3e
            kFrameMessage_0x3b,             // 0x3f
            kFrameMessage_0x3c = 0x44,      // 0x44
            kFrameMessage_0x3d,             // 0x45
            kFrameMessage_0x3e,             // 0x46
            kFrameMessage_0x3f,             // 0x47
            kFrameMessage_0x40,             // 0x48
            kFrameMessage_0x41,             // 0x49
            kFrameMessage_0x42,             // 0x4a
            kRenderFrame_0x43,              // 0x4b
            kFrameMessage_0x44 = 0x52,      // 0x52
            kFrameMessage_0x45,             // 0x53
            kFrameMessage_0x46 = 0x56,      // 0x56
            kFrameMessage_0x47,             // 0x57, Multiple uses depending on frame
            kFrameMessage_0x48,             // 0x58, Multiple uses depending on frame
            kFrameMessage_0x49,             // 0x59, Multiple uses depending on frame
            kFrameMessage_0x4a,             // 0x5a, Multiple uses depending on frame
            kFrameMessage_0x4b,             // 0x5b, Multiple uses depending on frame
            kFrameMessage_0x4c,             // 0x5c, Multiple uses depending on frame
            kFrameMessage_0x4d,             // 0x5d, Multiple uses depending on frame
            kFrameMessage_0x4e,             // 0x5e, Multiple uses depending on frame
            kFrameMessage_0x4f,             // 0x5f, Multiple uses depending on frame
            kFrameMessage_0x50,             // 0x60, Multiple uses depending on frame
            kFrameMessage_0x51,             // 0x61, Multiple uses depending on frame
            kFrameMessage_0x52,             // 0x62, Multiple uses depending on frame
            kFrameMessage_0x53,             // 0x63, Multiple uses depending on frame
            kFrameMessage_0x54,             // 0x64, Multiple uses depending on frame
            kFrameMessage_0x55,             // 0x65, Multiple uses depending on frame
            kFrameMessage_0x56,             // 0x66, Multiple uses depending on frame
            kFrameMessage_0x57,             // 0x67, Multiple uses depending on frame

            // High bit messages start at 0x10000000
            kMessage_0x10000000 = 0x10000000,  // 0x10000000
            kMessage_0x10000001,            // 0x10000001
            kMessage_0x10000002,            // 0x10000002
            kMessage_0x10000003,            // 0x10000003
            kMessage_0x10000004,            // 0x10000004
            kMessage_0x10000005,            // 0x10000005
            kMessage_0x10000006,            // 0x10000006
            kAgentUpdate,                   // 0x10000007, wparam = uint32_t agent_id
            kAgentDestroy,                  // 0x10000008, wparam = uint32_t agent_id
            kUpdateAgentEffects,            // 0x10000009
            kMessage_0x1000000a,            // 0x1000000a
            kMessage_0x1000000b,            // 0x1000000b
            kDialogueMessage,               // 0x1000000c
            kMessage_0x1000000d,            // 0x1000000d
            kMessage_0x1000000e,            // 0x1000000e
            kMessage_0x1000000f,            // 0x1000000f
            kMessage_0x10000010,            // 0x10000010
            kMessage_0x10000011,            // 0x10000011
            kMessage_0x10000012,            // 0x10000012
            kMessage_0x10000013,            // 0x10000013
            kMessage_0x10000014,            // 0x10000014
            kMessage_0x10000015,            // 0x10000015
            kMessage_0x10000016,            // 0x10000016
            kAgentSpeechBubble,             // 0x10000017
            kMessage_0x10000018,            // 0x10000018
            kShowAgentNameTag,              // 0x10000019, wparam = AgentNameTagInfo*
            kHideAgentNameTag,              // 0x1000001a
            kSetAgentNameTagAttribs,        // 0x1000001b, wparam = AgentNameTagInfo*
            kMessage_0x1000001c,            // 0x1000001c
            kSetAgentProfession,            // 0x1000001d, wparam = UIPacket::kSetAgentProfession*
            kMessage_0x1000001e,            // 0x1000001e
            kMessage_0x1000001f,            // 0x1000001f
            kChangeTarget,                  // 0x10000020, wparam = UIPacket::kChangeTarget*
            kMessage_0x10000021,            // 0x10000021
            kMessage_0x10000022,            // 0x10000022
            kMessage_0x10000023,            // 0x10000023
            kAgentSkillActivated,           // 0x10000024, kAgentSkillPacket
            kAgentSkillActivatedInstantly,  // 0x10000025, kAgentSkillPacket
            kAgentSkillCancelled,           // 0x10000026, kAgentSkillPacket
            kAgentSkillStartedCast,         // 0x10000027, wparam = UIPacket::kAgentStartCasting*
            kMessage_0x10000028,            // 0x10000028
            kShowMapEntryMessage,           // 0x10000029, wparam = { wchar_t* title, wchar_t* subtitle }
            kSetCurrentPlayerData,          // 0x1000002a, fired after setting the worldcontext player name
            kMessage_0x1000002b,            // 0x1000002b
            kMessage_0x1000002c,            // 0x1000002c
            kMessage_0x1000002d,            // 0x1000002d
            kMessage_0x1000002e,            // 0x1000002e
            kMessage_0x1000002f,            // 0x1000002f
            kMessage_0x10000030,            // 0x10000030
            kMessage_0x10000031,            // 0x10000031
            kMessage_0x10000032,            // 0x10000032
            kMessage_0x10000033,            // 0x10000033
            kPostProcessingEffect,          // 0x10000034, Triggered when drunk. wparam = UIPacket::kPostProcessingEffect
            kMessage_0x10000035,            // 0x10000035
            kMessage_0x10000036,            // 0x10000036
            kMessage_0x10000037,            // 0x10000037
            kHeroAgentAdded,                // 0x10000038, hero assigned to agent/inventory/ai mode
            kHeroDataAdded,                 // 0x10000039, hero info received from server (name, level etc)
            kMessage_0x1000003a,            // 0x1000003a
            kMessage_0x1000003b,            // 0x1000003b
            kMessage_0x1000003c,            // 0x1000003c
            kMessage_0x1000003d,            // 0x1000003d
            kMessage_0x1000003e,            // 0x1000003e
            kMessage_0x1000003f,            // 0x1000003f
            kShowXunlaiChest,               // 0x10000040
            kMessage_0x10000041,            // 0x10000041
            kMessage_0x10000042,            // 0x10000042
            kMessage_0x10000043,            // 0x10000043
            kMessage_0x10000044,            // 0x10000044
            kMessage_0x10000045,            // 0x10000045
            kMinionCountUpdated,            // 0x10000046
            kMoraleChange,                  // 0x10000047, wparam = {agent id, morale percent }
            kMessage_0x10000048,            // 0x10000048
            kMessage_0x10000049,            // 0x10000049
            kMessage_0x1000004a,            // 0x1000004a
            kMessage_0x1000004b,            // 0x1000004b
            kMessage_0x1000004c,            // 0x1000004c
            kMessage_0x1000004d,            // 0x1000004d
            kMessage_0x1000004e,            // 0x1000004e
            kMessage_0x1000004f,            // 0x1000004f
            kLoginStateChanged,             // 0x10000050, wparam = {bool is_logged_in, bool unk }
            kMessage_0x10000051,            // 0x10000051
            kMessage_0x10000052,            // 0x10000052
            kMessage_0x10000053,            // 0x10000053
            kMessage_0x10000054,            // 0x10000054
            kEffectAdd,                     // 0x10000055, wparam = {agent_id, GW::Effect*}
            kEffectRenew,                   // 0x10000056, wparam = GW::Effect*
            kEffectRemove,                  // 0x10000057, wparam = effect id
            kMessage_0x10000058,            // 0x10000058
            kMessage_0x10000059,            // 0x10000059
            kMessage_0x1000005a,            // 0x1000005a
            kSkillActivated,                // 0x1000005b, wparam ={ uint32_t agent_id , uint32_t skill_id }
            kMessage_0x1000005c,            // 0x1000005c
            kMessage_0x1000005d,            // 0x1000005d
            kUpdateSkillbar,                // 0x1000005e, wparam ={ uint32_t agent_id , ... }
            kUpdateSkillsAvailable,         // 0x1000005f, Triggered on a skill unlock, profession change or map load
            kMessage_0x10000060,            // 0x10000060
            kMessage_0x10000061,            // 0x10000061
            kMessage_0x10000062,            // 0x10000062
            kMessage_0x10000063,            // 0x10000063
            kPlayerTitleChanged,            // 0x10000064, wparam = { uint32_t player_id, uint32_t title_id }
            kTitleProgressUpdated,          // 0x10000065, wparam = title_id
            kExperienceGained,              // 0x10000066, wparam = experience amount
            kMessage_0x10000067,            // 0x10000067
            kMessage_0x10000068,            // 0x10000068
            kMessage_0x10000069,            // 0x10000069
            kMessage_0x1000006a,            // 0x1000006a
            kMessage_0x1000006b,            // 0x1000006b
            kMessage_0x1000006c,            // 0x1000006c
            kMessage_0x1000006d,            // 0x1000006d
            kMessage_0x1000006e,            // 0x1000006e
            kMessage_0x1000006f,            // 0x1000006f
            kMessage_0x10000070,            // 0x10000070
            kMessage_0x10000071,            // 0x10000071
            kMessage_0x10000072,            // 0x10000072
            kMessage_0x10000073,            // 0x10000073
            kMessage_0x10000074,            // 0x10000074
            kMessage_0x10000075,            // 0x10000075
            kMessage_0x10000076,            // 0x10000076
            kMessage_0x10000077,            // 0x10000077
            kMessage_0x10000078,            // 0x10000078
            kMessage_0x10000079,            // 0x10000079
            kMessage_0x1000007a,            // 0x1000007a
            kMessage_0x1000007b,            // 0x1000007b
            kMessage_0x1000007c,            // 0x1000007c
            kMessage_0x1000007d,            // 0x1000007d
            kMessage_0x1000007e,            // 0x1000007e
            kWriteToChatLog,                // 0x1000007f, wparam = UIPacket::kWriteToChatLog*
            kWriteToChatLogWithSender,      // 0x10000080, wparam = UIPacket::kWriteToChatLogWithSender*
            kAllyOrGuildMessage,            // 0x10000081, wparam = UIPacket::kAllyOrGuildMessage*
            kPlayerChatMessage,             // 0x10000082, wparam = UIPacket::kPlayerChatMessage*
            kMessage_0x10000083,            // 0x10000083
            kFloatingWindowMoved,           // 0x10000084, wparam = frame_id
            kMessage_0x10000085,            // 0x10000085
            kMessage_0x10000086,            // 0x10000086
            kMessage_0x10000087,            // 0x10000087
            kMessage_0x10000088,            // 0x10000088
            kMessage_0x10000089,            // 0x10000089
            kMessage_0x1000008a,            // 0x1000008a
            kFriendUpdated,                 // 0x1000008b, wparam = { GW::Friend*, ... }
            kMapLoaded,                     // 0x1000008c
            kMessage_0x1000008d,            // 0x1000008d
            kMessage_0x1000008e,            // 0x1000008e
            kMessage_0x1000008f,            // 0x1000008f
            kMessage_0x10000090,            // 0x10000090
            kMessage_0x10000091,            // 0x10000091
            kOpenWhisper,                   // 0x10000092, wparam = wchar* name
            kMessage_0x10000093,            // 0x10000093
            kMessage_0x10000094,            // 0x10000094
            kMessage_0x10000095,            // 0x10000095
            kMessage_0x10000096,            // 0x10000096
            kMessage_0x10000097,            // 0x10000097
            kLoadMapContext,                // 0x10000098, wparam = UIPacket::kLoadMapContext
            kMessage_0x10000099,            // 0x10000099
            kMessage_0x1000009a,            // 0x1000009a
            kMessage_0x1000009b,            // 0x1000009b
            kDialogueMessageUpdated,        // 0x1000009c
            kLogout,                        // 0x1000009d, wparam = { bool unknown, bool character_select }
            kCompassDraw,                   // 0x1000009e, wparam = UIPacket::kCompassDraw*
            kMessage_0x1000009f,            // 0x1000009f
            kMessage_0x100000a0,            // 0x100000a0
            kMessage_0x100000a1,            // 0x100000a1
            kOnScreenMessage,               // 0x100000a2, wparam = wchar_** encoded_string
            kDialogButton,                  // 0x100000a3, wparam = DialogButtonInfo*
            kMessage_0x100000a4,            // 0x100000a4
            kMessage_0x100000a5,            // 0x100000a5
            kDialogBody,                    // 0x100000a6, wparam = DialogBodyInfo*
            kMessage_0x100000a7,            // 0x100000a7
            kMessage_0x100000a8,            // 0x100000a8
            kMessage_0x100000a9,            // 0x100000a9
            kMessage_0x100000aa,            // 0x100000aa
            kMessage_0x100000ab,            // 0x100000ab
            kMessage_0x100000ac,            // 0x100000ac
            kMessage_0x100000ad,            // 0x100000ad
            kMessage_0x100000ae,            // 0x100000ae
            kMessage_0x100000af,            // 0x100000af
            kMessage_0x100000b0,            // 0x100000b0
            kMessage_0x100000b1,            // 0x100000b1
            kMessage_0x100000b2,            // 0x100000b2
            kTargetNPCPartyMember,          // 0x100000b3, wparam = { uint32_t unk, uint32_t agent_id }
            kTargetPlayerPartyMember,       // 0x100000b4, wparam = { uint32_t unk, uint32_t player_number }
            kVendorWindow,                  // 0x100000b5, wparam = UIPacket::kVendorWindow
            kMessage_0x100000b6,            // 0x100000b6
            kMessage_0x100000b7,            // 0x100000b7
            kMessage_0x100000b8,            // 0x100000b8
            kVendorItems,                   // 0x100000b9, wparam = UIPacket::kVendorItems
            kMessage_0x100000ba,            // 0x100000ba
            kVendorTransComplete,           // 0x100000bb, wparam = *TransactionType
            kMessage_0x100000bc,            // 0x100000bc
            kVendorQuote,                   // 0x100000bd, wparam = UIPacket::kVendorQuote
            kMessage_0x100000be,            // 0x100000be
            kMessage_0x100000bf,            // 0x100000bf
            kMessage_0x100000c0,            // 0x100000c0
            kMessage_0x100000c1,            // 0x100000c1
            kStartMapLoad,                  // 0x100000c2, wparam = { uint32_t map_id, ...}
            kMessage_0x100000c3,            // 0x100000c3
            kMessage_0x100000c4,            // 0x100000c4
            kMessage_0x100000c5,            // 0x100000c5
            kMessage_0x100000c6,            // 0x100000c6
            kWorldMapUpdated,               // 0x100000c7, Triggered when an area in the world map has been discovered/updated
            kMessage_0x100000c8,            // 0x100000c8
            kMessage_0x100000c9,            // 0x100000c9
            kMessage_0x100000ca,            // 0x100000ca
            kMessage_0x100000cb,            // 0x100000cb
            kMessage_0x100000cc,            // 0x100000cc
            kMessage_0x100000cd,            // 0x100000cd
            kMessage_0x100000ce,            // 0x100000ce
            kMessage_0x100000cf,            // 0x100000cf
            kMessage_0x100000d0,            // 0x100000d0
            kMessage_0x100000d1,            // 0x100000d1
            kMessage_0x100000d2,            // 0x100000d2
            kMessage_0x100000d3,            // 0x100000d3
            kMessage_0x100000d4,            // 0x100000d4
            kMessage_0x100000d5,            // 0x100000d5
            kMessage_0x100000d6,            // 0x100000d6
            kMessage_0x100000d7,            // 0x100000d7
            kMessage_0x100000d8,            // 0x100000d8
            kMessage_0x100000d9,            // 0x100000d9
            kGuildMemberUpdated,            // 0x100000da, wparam = { GuildPlayer::name_ptr }
            kMessage_0x100000db,            // 0x100000db
            kMessage_0x100000dc,            // 0x100000dc
            kMessage_0x100000dd,            // 0x100000dd
            kMessage_0x100000de,            // 0x100000de
            kMessage_0x100000df,            // 0x100000df
            kMessage_0x100000e0,            // 0x100000e0
            kShowHint,                      // 0x100000e1, wparam = { uint32_t icon_type, wchar_t* message_enc }
            kMessage_0x100000e2,            // 0x100000e2
            kMessage_0x100000e3,            // 0x100000e3
            kMessage_0x100000e4,            // 0x100000e4
            kMessage_0x100000e5,            // 0x100000e5
            kMessage_0x100000e6,            // 0x100000e6
            kMessage_0x100000e7,            // 0x100000e7
            kMessage_0x100000e8,            // 0x100000e8
            kWeaponSetSwapComplete,         // 0x100000e9, wparam = UIPacket::kWeaponSwap*
            kWeaponSetSwapCancel,           // 0x100000ea
            kWeaponSetUpdated,              // 0x100000eb
            kUpdateGoldCharacter,           // 0x100000ec, wparam = { uint32_t unk, uint32_t gold_character }
            kUpdateGoldStorage,             // 0x100000ed, wparam = { uint32_t unk, uint32_t gold_storage }
            kInventorySlotUpdated,          // 0x100000ee, Triggered when an item is moved into a slot
            kEquipmentSlotUpdated,          // 0x100000ef, Triggered when an item is moved into a slot
            kMessage_0x100000f0,            // 0x100000f0
            kInventorySlotCleared,          // 0x100000f1, Triggered when an item has been removed from a slot
            kEquipmentSlotCleared,          // 0x100000f2, Triggered when an item has been removed from a slot
            kMessage_0x100000f3,            // 0x100000f3
            kMessage_0x100000f4,            // 0x100000f4
            kMessage_0x100000f5,            // 0x100000f5
            kMessage_0x100000f6,            // 0x100000f6
            kMessage_0x100000f7,            // 0x100000f7
            kMessage_0x100000f8,            // 0x100000f8
            kMessage_0x100000f9,            // 0x100000f9
            kPvPWindowContent,              // 0x100000fa
            kMessage_0x100000fb,            // 0x100000fb
            kMessage_0x100000fc,            // 0x100000fc
            kMessage_0x100000fd,            // 0x100000fd
            kMessage_0x100000fe,            // 0x100000fe
            kMessage_0x100000ff,            // 0x100000ff
            kMessage_0x10000100,            // 0x10000100
            kMessage_0x10000101,            // 0x10000101
            kPreStartSalvage,               // 0x10000102, { uint32_t item_id, uint32_t kit_id }
            kTomeSkillSelection,            // 0x10000103, wparam = UIPacket::kTomeSkillSelection*
            kMessage_0x10000104,            // 0x10000104
            kTradePlayerUpdated,            // 0x10000105, wparam = GW::TraderPlayer*
            kItemUpdated,                   // 0x10000106, wparam = UIPacket::kItemUpdated*
            kMessage_0x10000107,            // 0x10000107
            kMessage_0x10000108,            // 0x10000108
            kMessage_0x10000109,            // 0x10000109
            kMessage_0x1000010a,            // 0x1000010a
            kMessage_0x1000010b,            // 0x1000010b
            kMessage_0x1000010c,            // 0x1000010c
            kMessage_0x1000010d,            // 0x1000010d
            kMessage_0x1000010e,            // 0x1000010e
            kMessage_0x1000010f,            // 0x1000010f
            kMessage_0x10000110,            // 0x10000110
            kMapChange,                     // 0x10000111, wparam = map id
            kMessage_0x10000112,            // 0x10000112
            kMessage_0x10000113,            // 0x10000113
            kMessage_0x10000114,            // 0x10000114
            kCalledTargetChange,            // 0x10000115, wparam = { player_number, target_id }
            kMessage_0x10000116,            // 0x10000116
            kMessage_0x10000117,            // 0x10000117
            kMessage_0x10000118,            // 0x10000118
            kErrorMessage,                  // 0x10000119, wparam = { int error_index, wchar_t* error_encoded_string }
            kPartyHardModeChanged,          // 0x1000011a, wparam = { int is_hard_mode }
            kPartyAddHenchman,              // 0x1000011b
            kPartyRemoveHenchman,           // 0x1000011c
            kMessage_0x1000011d,            // 0x1000011d
            kPartyAddHero,                  // 0x1000011e
            kPartyRemoveHero,               // 0x1000011f
            kMessage_0x10000120,            // 0x10000120
            kMessage_0x10000121,            // 0x10000121
            kMessage_0x10000122,            // 0x10000122
            kMessage_0x10000123,            // 0x10000123
            kPartyAddPlayer,                // 0x10000124
            kMessage_0x10000125,            // 0x10000125
            kPartyRemovePlayer,             // 0x10000126
            kMessage_0x10000127,            // 0x10000127
            kMessage_0x10000128,            // 0x10000128
            kMessage_0x10000129,            // 0x10000129
            kDisableEnterMissionBtn,        // 0x1000012a, wparam = boolean (1 = disabled, 0 = enabled)
            kMessage_0x1000012b,            // 0x1000012b
            kMessage_0x1000012c,            // 0x1000012c
            kShowCancelEnterMissionBtn,     // 0x1000012d
            kMessage_0x1000012e,            // 0x1000012e
            kPartyDefeated,                 // 0x1000012f
            kMessage_0x10000130,            // 0x10000130
            kMessage_0x10000131,            // 0x10000131
            kMessage_0x10000132,            // 0x10000132
            kPartySearchCreated,            // 0x10000133, wparam = GW::PartySearch*
            kPartySearchIdChanged,          // 0x10000134, wparam = uint32_t* party_search_id
            kPartySearchRemoved,            // 0x10000135, wparam = uint32_t* party_search_id
            kPartySearchUpdated,            // 0x10000136, wparam = GW::PartySearch*
            kPartySearchInviteReceived,     // 0x10000137, wparam = UIPacket::kPartySearchInviteReceived*
            kMessage_0x10000138,            // 0x10000138
            kPartySearchInviteSent,         // 0x10000139
            kPartyShowConfirmDialog,        // 0x1000013a, wparam = UIPacket::kPartyShowConfirmDialog
            kMessage_0x1000013b,            // 0x1000013b
            kMessage_0x1000013c,            // 0x1000013c
            kMessage_0x1000013d,            // 0x1000013d
            kMessage_0x1000013e,            // 0x1000013e
            kMessage_0x1000013f,            // 0x1000013f
            kPreferenceEnumChanged,         // 0x10000140, wparam = UiPacket::kPreferenceEnumChanged
            kPreferenceFlagChanged,         // 0x10000141, wparam = UiPacket::kPreferenceFlagChanged
            kPreferenceValueChanged,        // 0x10000142, wparam = UiPacket::kPreferenceValueChanged
            kUIPositionChanged,             // 0x10000143, wparam = UIPacket::kUIPositionChanged
            kPreBuildLoginScene,            // 0x10000144, Called with no args right before login scene is drawn
            kMessage_0x10000145,            // 0x10000145
            kMessage_0x10000146,            // 0x10000146
            kMessage_0x10000147,            // 0x10000147
            kMessage_0x10000148,            // 0x10000148
            kMessage_0x10000149,            // 0x10000149
            kMessage_0x1000014a,            // 0x1000014a
            kMessage_0x1000014b,            // 0x1000014b
            kMessage_0x1000014c,            // 0x1000014c
            kMessage_0x1000014d,            // 0x1000014d
            kQuestAdded,                    // 0x1000014e, wparam = { quest_id, ... }
            kQuestDetailsChanged,           // 0x1000014f, wparam = { quest_id, ... }
            kQuestRemoved,                  // 0x10000150, wparam = { quest_id, ... }
            kClientActiveQuestChanged,      // 0x10000151, wparam = { quest_id, ... }. Triggered when the game requests the current quest to change
            kMessage_0x10000152,            // 0x10000152
            kServerActiveQuestChanged,      // 0x10000153, wparam = UIPacket::kServerActiveQuestChanged*. Triggered when the server requests the current quest to change
            kUnknownQuestRelated,           // 0x10000154
            kMessage_0x10000155,            // 0x10000155
            kDungeonComplete,               // 0x10000156
            kMissionComplete,               // 0x10000157
            kMessage_0x10000158,            // 0x10000158
            kVanquishComplete,              // 0x10000159
            kObjectiveAdd,                  // 0x1000015a, wparam = UIPacket::kObjectiveAdd*
            kObjectiveComplete,             // 0x1000015b, wparam = UIPacket::kObjectiveComplete*
            kObjectiveUpdated,              // 0x1000015c, wparam = UIPacket::kObjectiveUpdated*
            kMessage_0x1000015d,            // 0x1000015d
            kMessage_0x1000015e,            // 0x1000015e
            kMessage_0x1000015f,            // 0x1000015f
            kMessage_0x10000160,            // 0x10000160
            kMessage_0x10000161,            // 0x10000161
            kMessage_0x10000162,            // 0x10000162
            kMessage_0x10000163,            // 0x10000163
            kMessage_0x10000164,            // 0x10000164
            kTradeSessionStart,             // 0x10000165, wparam = { trade_state, player_number }
            kMessage_0x10000166,            // 0x10000166
            kMessage_0x10000167,            // 0x10000167
            kMessage_0x10000168,            // 0x10000168
            kMessage_0x10000169,            // 0x10000169
            kMessage_0x1000016a,            // 0x1000016a
            kTradeSessionUpdated,           // 0x1000016b, no args
            kMessage_0x1000016c,            // 0x1000016c
            kMessage_0x1000016d,            // 0x1000016d
            kMessage_0x1000016e,            // 0x1000016e
            kMessage_0x1000016e_1,          // 0x1000016f, added to GW 2026-04-28
            kMessage_0x1000016f,            // 0x10000170
            kMessage_0x10000170,            // 0x10000171
            kMessage_0x10000171,            // 0x10000172
            kMessage_0x10000172,            // 0x10000173
            kMessage_0x10000173,            // 0x10000174
            kMessage_0x10000174,            // 0x10000175
            kCheckUIState,                  // 0x10000176
            kMessage_0x10000176,            // 0x10000177
            kMessage_0x10000177,            // 0x10000178
            kMessage_0x10000178,            // 0x10000179
            kMessage_0x10000178_1,          // 0x1000017a, added to GW 2026-02-26
            kMessage_0x10000178_2,          // 0x1000017b, added to GW 2026-02-26
            kMessage_0x10000178_3,          // 0x1000017c, added to GW 2026-02-26
            kDestroyUIPositionOverlay,      // 0x1000017d
            kEnableUIPositionOverlay,       // 0x1000017e, wparam = uint32_t enable
            kMessage_0x1000017b,            // 0x1000017f
            kGuildHall,                     // 0x10000180, wparam = gh key (uint32_t[4])
            kMessage_0x1000017d,            // 0x10000181
            kLeaveGuildHall,                // 0x10000182
            kTravel,                        // 0x10000183
            kOpenWikiUrl,                   // 0x10000184, wparam = char* url
            kMessage_0x10000181,            // 0x10000185
            kMessage_0x10000182,            // 0x10000186
            kSetPreGameContext_Value0,      // 0x10000187, wparam = uint32_t value
            kMessage_0x10000184,            // 0x10000188
            kGetPreGameContext_Value0,      // 0x10000189, lparam = *uint32_t value_out
            kSetPreGameContext_Value1,      // 0x1000018a, wparam = uint32_t value     , added to GW 2026-02-06
            kGetPreGameContext_Value1,      // 0x1000018b, lparam = *uint32_t value_out, added to GW 2026-02-06
            kMessage_0x10000186,            // 0x1000018c
            kMessage_0x10000187,            // 0x1000018d
            kMessage_0x10000188,            // 0x1000018e
            kMessage_0x10000189,            // 0x1000018f
            kMessage_0x1000018a,            // 0x10000190
            kMessage_0x1000018b,            // 0x10000191
            kMessage_0x1000018c,            // 0x10000192
            kMessage_0x1000018d,            // 0x10000193
            kAppendMessageToChat,           // 0x10000194, wparam = wchar_t* message
            kMessage_0x1000018f,            // 0x10000195
            kMessage_0x10000190,            // 0x10000196
            kMessage_0x10000191,            // 0x10000197
            kMessage_0x10000192,            // 0x10000198
            kMessage_0x10000193,            // 0x10000199
            kMessage_0x10000194,            // 0x1000019a
            kMessage_0x10000195,            // 0x1000019b
            kMessage_0x10000196,            // 0x1000019c
            kMessage_0x10000197,            // 0x1000019d
            kMessage_0x10000198,            // 0x1000019e
            kMessage_0x10000199,            // 0x1000019f
            kMessage_0x1000019a,            // 0x100001a0
            kMessage_0x1000019b,            // 0x100001a1
            kHideHeroPanel,                 // 0x100001a2, wparam = hero_id
            kShowHeroPanel,                 // 0x100001a3, wparam = hero_id
            kMessage_0x1000019e,            // 0x100001a4
            kMessage_0x1000019f,            // 0x100001a5
            kMessage_0x100001a0,            // 0x100001a6
            kGetInventoryAgentId,           // 0x100001a7, wparam = 0, lparam = uint32_t* agent_id_out. Used to fetch which agent is selected
            kInventoryRelated1,             // 0x100001a8, added to GW 2026-02-26
            kInventoryRelated2,             // 0x100001a9, added to GW 2026-02-26
            kInventoryRelated3,             // 0x100001aa, added to GW 2026-02-26
            kEquipItem,                     // 0x100001ab, wparam = { item_id, agent_id }
            kMoveItem,                      // 0x100001ac, wparam = { item_id, to_bag, to_slot, bool prompt }
            kItemRelated_1,                 // 0x100001ad
            kItemTooltip,                   // 0x100001ae
            kItemRelated_3,                 // 0x100001af, added to GW 2026-02-26
            kItemRelated_4,                 // 0x100001b0, added to GW 2026-02-26
            kInitiateTrade,                 // 0x100001b1
            kMessage_0x100001a7,            // 0x100001b2
            kMessage_0x100001a8,            // 0x100001b3
            kMessage_0x100001a9,            // 0x100001b4
            kMessage_0x100001aa,            // 0x100001b5
            kMessage_0x100001ab,            // 0x100001b6
            kMessage_0x100001ac,            // 0x100001b7
            kMessage_0x100001ad,            // 0x100001b8
            kMessage_0x100001ae,            // 0x100001b9
            kMessage_0x100001af,            // 0x100001ba
            kMessage_0x100001b0,            // 0x100001bb
            kMessage_0x100001b1,            // 0x100001bc
            kMessage_0x100001b2,            // 0x100001bd
            kMessage_0x100001b3,            // 0x100001be
            kMessage_0x100001b4,            // 0x100001bf
            kMessage_0x100001b5,            // 0x100001c0
            kInventoryAgentChanged,         // 0x100001c1, Triggered when inventory needs updating due to agent change; no args
            kInventoryRelated_1,            // 0x100001c2
            kInventoryRelated_2,            // 0x100001c3
            kMissionStatusRelated,          // 0x100001c4
            kUnused_1c2,                    // 0x100001c5
            kCollapseExpandSkillListSection,  // 0x100001c6
            kTemplateRelated_1,             // 0x100001c7
            kTemplateRelated_2,             // 0x100001c8
            kPromptSaveTemplate,            // 0x100001c9
            kOpenTemplate,                  // 0x100001ca, wparam = GW::UI::ChatTemplate*
            kTemplateRelated_3,             // 0x100001cb
            kTemplateRelated_4,             // 0x100001cc

            // GWCA Client to Server commands. Only added the ones that are used for hooks, everything else goes straight into GW

            kSendLoadSkillTemplate = 0x30000000 | 0x3,  // 0x30000003, wparam = SkillbarMgr::SkillTemplate*
            kSendPingWeaponSet = 0x30000000 | 0x4,  // 0x30000004, wparam = UIPacket::kSendPingWeaponSet*
            kSendMoveItem = 0x30000000 | 0x5,  // 0x30000005, wparam = UIPacket::kSendMoveItem*
            kSendMerchantRequestQuote = 0x30000000 | 0x6,  // 0x30000006, wparam = UIPacket::kSendMerchantRequestQuote*
            kSendMerchantTransactItem = 0x30000000 | 0x7,  // 0x30000007, wparam = UIPacket::kSendMerchantTransactItem*
            kSendUseItem = 0x30000000 | 0x8,  // 0x30000008, wparam = UIPacket::kSendUseItem*
            kSendSetActiveQuest = 0x30000000 | 0x9,  // 0x30000009, wparam = uint32_t quest_id
            kSendAbandonQuest = 0x30000000 | 0xA,  // 0x3000000a, wparam = uint32_t quest_id
            kSendChangeTarget = 0x30000000 | 0xB,  // 0x3000000b, wparam = UIPacket::kSendChangeTarget* // e.g. tell the gw client to focus on a different target
            kSendCallTarget = 0x30000000 | 0x13,  // 0x30000013, wparam = { uint32_t call_type, uint32_t agent_id } // also used to broadcast morale, death penalty, "I'm following X", etc
            kSendDialog = 0x30000000 | 0x16,  // 0x30000016, wparam = dialog_id // internal use


            kStartWhisper = 0x30000000 | 0x17,  // 0x30000017, wparam = UIPacket::kStartWhisper*
            kGetSenderColor = 0x30000000 | 0x18,  // 0x30000018, wparam = UIPacket::kGetColor* // Get chat sender color depending on channel, output object passed by reference
            kGetMessageColor = 0x30000000 | 0x19,  // 0x30000019, wparam = UIPacket::kGetColor* // Get chat message color depending on channel, output object passed by reference
            kSendChatMessage = 0x30000000 | 0x1B,  // 0x3000001b, wparam = UIPacket::kSendChatMessage*
            kLogChatMessage = 0x30000000 | 0x1D,  // 0x3000001d, wparam = UIPacket::kLogChatMessage*. Triggered when a message wants to be added to the persistent chat log.
            kRecvWhisper = 0x30000000 | 0x1E,  // 0x3000001e, wparam = UIPacket::kRecvWhisper*
            kPrintChatMessage = 0x30000000 | 0x1F,  // 0x3000001f, wparam = UIPacket::kPrintChatMessage*. Triggered when a message wants to be added to the in-game chat window.
            kSendWorldAction = 0x30000000 | 0x20,  // 0x30000020, wparam = UIPacket::kSendWorldAction*
            kSetRendererValue = 0x30000000 | 0x21,  // 0x30000021, wparam = UIPacket::kSetRendererValue
            kIdentifyItem = 0x30000000 | 0x22,  // 0x30000022, wparam = UIPacket::kUseKitOnItem
            kSalvageItem = 0x30000000 | 0x23  // 0x30000023, wparam = UIPacket::kUseKitOnItem
        };

        //static_assert(GW::UI::UIMessage::kOpenTemplate == (GW::UI::UIMessage)0x100001c4);

        namespace UIPacket {
            struct kDialogueMessage {
                uint32_t agent_id;
                wchar_t* sender;
                wchar_t* message;
                uint32_t duration;
                uint32_t is_dialogue_1_or_2;
            };
            struct kErrorMessage {
                uint32_t error_id;
                wchar_t* message;
            };
            struct kAgentSkillPacket {
                uint32_t agent_id;
                GW::Constants::SkillID skill_id;
            };
            struct kLoadMapContext {
                const wchar_t* file_name;
                Constants::MapID map_id;
                Constants::InstanceType map_type;
                Vec2f spawn_point;
                uint32_t h0014;
                uint32_t h0018;
                bool* success;
            };
            static_assert(sizeof(kLoadMapContext) == 0x20);
            struct kMouseCoordsClick {
                float offset_x;
                float offset_y;
                uint32_t h0008;
                uint32_t h000c;
                uint32_t* h0010;
                uint32_t h0014;
            };
            struct kUseKitOnItem {
                uint32_t item_id;
                uint32_t kit_id;
            };
            struct kShowXunlaiChest {
                uint32_t h0000 = 0;
                bool storage_pane_unlocked = true;
                bool anniversary_pane_unlocked = true;
            };
            struct kMoveItem {
                uint32_t item_id;
                uint32_t to_bag_index;
                uint32_t to_slot;
                uint32_t prompt;
            };
            struct kResize {
                uint32_t h0000 = 0xffffffff;
                float content_width;
                float content_height;
                float h000c = 0;
                float h0010 = 0;
                float content_width2;
                float content_height2;
                float margin_x;
                float margin_y;
                // ...
            };
            struct kTomeSkillSelection {
                uint32_t item_id;
                uint32_t h0004;
                uint32_t h0008;
            };
            struct kMeasureContent {
                float max_width;        // Maximum width constraint
                float max_height;       // Maximum height constraint
                float* size_output;     // Pointer to output buffer for calculated size
                uint32_t flags;         // Layout flags (similar to the 0x100 flag we saw)
            };
            struct kSetLayout {
                float field_0x0;
                float field_0x4;
                float field_0x8;
                float field_0xc;
                float available_width;
                float available_height;
            };
            struct kSetAgentProfession {
                AgentID agent_id;
                uint32_t primary;
                uint32_t secondary;
            };
            struct kWeaponSwap {
                uint32_t weapon_bar_frame_id;
                uint32_t weapon_set_id;
            };
            struct kWeaponSetChanged {
                uint32_t h0000;
                uint32_t h0004;
                uint32_t h0008;
                uint32_t h000c;
            };
            struct kChangeTarget {
                uint32_t evaluated_target_id;
                bool has_evaluated_target_changed;
                uint32_t auto_target_id;
                bool has_auto_target_changed;
                uint32_t manual_target_id;
                bool has_manual_target_changed;
            };
            struct kSendLoadSkillTemplate {
                uint32_t agent_id;
                SkillbarMgr::SkillTemplate* skill_template;
            };
            struct kVendorWindow {
                Merchant::TransactionType transaction_type;
                uint32_t unk;
                uint32_t merchant_agent_id;
                uint32_t is_pending;
            };
            struct kVendorQuote {
                uint32_t item_id;
                uint32_t price;
            };
            struct kVendorItems {
                Merchant::TransactionType transaction_type;
                uint32_t item_ids_count;
                uint32_t* item_ids_buffer1; // world->merchant_items.buffer
                uint32_t* item_ids_buffer2; // world->merchant_items2.buffer
            };
            struct kSetRendererValue {
                uint32_t renderer_mode; // 0 for window, 2 for full screen
                uint32_t metric_id; // TODO: Enum this!
                uint32_t value;
            };
            struct kEffectAdd {
                uint32_t agent_id;
                Effect* effect;
            };
            struct kAgentSpeechBubble {
                uint32_t agent_id;
                wchar_t* message;
                uint32_t h0008;
                uint32_t h000c;
            };
            struct kAgentSkillStartedCast {
                uint32_t agent_id;
                Constants::SkillID skill_id;
                float duration;
                uint32_t h000c;
            };
            struct kPreStartSalvage {
                uint32_t item_id;
                uint32_t kit_id;
            };
            struct kServerActiveQuestChanged {
                GW::Constants::QuestID quest_id;
                GW::GamePos marker;
                uint32_t h0024;
                GW::Constants::MapID map_id;
                uint32_t log_state;
            };
            struct kPrintChatMessage {
                GW::Chat::Channel channel;
                wchar_t* message;
                FILETIME timestamp;
                uint32_t is_reprint;
            };
            struct kPartyShowConfirmDialog {
                uint32_t ui_message_to_send_to_party_frame;
                uint32_t prompt_identitifier;
                wchar_t* prompt_enc_str;
            };
            struct kUIPositionChanged {
                uint32_t window_id;
                UI::WindowPosition* position;
            };
            struct kPreferenceFlagChanged {
                UI::FlagPreference preference_id;
                uint32_t new_value;
            };
            struct kPreferenceValueChanged {
                UI::NumberPreference preference_id;
                uint32_t new_value;
            };
            struct kPreferenceEnumChanged {
                UI::EnumPreference preference_id;
                uint32_t enum_index;
            };
            struct kPartySearchInvite {
                uint32_t source_party_search_id;
                uint32_t dest_party_search_id;
            };
            struct kPostProcessingEffect {
                uint32_t tint;
                float amount;
            };
            struct kLogout {
                uint32_t unknown;
                uint32_t character_select;
            };
            static_assert(sizeof(kLogout) == 0x8);

            struct kKeyAction {
                uint32_t gw_key;
                uint32_t modifiers = 0;
                uint32_t state_flags = 0; // shift held = 0x4, ctrl = 0x2, alt = 0x1
            };
            struct kMouseClick {
                uint32_t mouse_button; // 0x0 = left, 0x1 = middle, 0x2 = right
                uint32_t is_doubleclick;
                uint32_t unknown_type_screen_pos;
                uint32_t h000c;
                uint32_t h0010;
            };
            enum ActionState : uint32_t {
                MouseDown = 0x6,            // 0x6
                MouseUp = 0x7,              // 0x7
                MouseClick = 0x8,           // 0x8
                MouseDoubleClick = 0x9,     // 0x9
                DragRelease = 0xb,          // 0xb
                KeyDown = 0xe               // 0xe
            };
            struct kMouseAction {
                uint32_t frame_id;
                uint32_t child_offset_id;
                ActionState current_state;
                void* wparam = 0;
                void* lparam = 0;
            };
            struct kWriteToChatLog {
                GW::Chat::Channel channel;
                wchar_t* message;
                GW::Chat::Channel channel_dupe;
            };
            struct kPlayerChatMessage {
                GW::Chat::Channel channel;
                wchar_t* message;
                uint32_t player_number;
            };

            struct kInteractAgent {
                uint32_t agent_id;
                bool call_target;
            };

            struct kSendChangeTarget {
                uint32_t target_id;
                uint32_t auto_target_id;
            };

            struct kSendCallTarget {
                CallTargetType call_type;
                uint32_t agent_id;
            };

            struct kGetColor {
                Chat::Color* color;
                GW::Chat::Channel channel;
            };

            struct kWriteToChatLogWithSender {
                uint32_t channel;
                wchar_t* message;
                wchar_t* sender_enc;
            };

            struct kSendPingWeaponSet {
                uint32_t agent_id;
                uint32_t weapon_item_id;
                uint32_t offhand_item_id;
            };
            struct kSendMoveItem {
                uint32_t item_id;
                uint32_t quantity;
                uint32_t bag_index;
                uint32_t slot;
            };
            struct kSendMerchantRequestQuote {
                Merchant::TransactionType type;
                uint32_t item_id;
            };
            struct kSendMerchantTransactItem {
                Merchant::TransactionType type;
                uint32_t h0004;
                Merchant::QuoteInfo give;
                uint32_t gold_recv;
                Merchant::QuoteInfo recv;
            };
            struct kSendUseItem {
                uint32_t item_id;
                uint16_t quantity; // Unused, but would be cool
            };
            struct kSendChatMessage {
                wchar_t* message;
                uint32_t agent_id;
            };
            struct kLogChatMessage {
                wchar_t* message;
                GW::Chat::Channel channel;
            };
            struct kRecvWhisper {
                uint32_t transaction_id;
                wchar_t* from;
                wchar_t* message;
            };
            struct kStartWhisper {
                wchar_t* player_name;
            };
            struct kCompassDraw {
                uint32_t player_number;
                uint32_t session_id;
                uint32_t number_of_points;
                CompassPoint* points;
            };
            struct kObjectiveAdd {
                uint32_t objective_id;
                wchar_t* name;
                uint32_t type;
            };
            struct kObjectiveComplete {
                uint32_t objective_id;
            };
            struct kObjectiveUpdated {
                uint32_t objective_id;
            };
            // Straight passthru of GW::Packets::StoC::ItemGeneral
            struct kItemUpdated {
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
            struct kInventorySlotUpdated {
                uint32_t unk;
                uint32_t item_id;
                uint32_t bag_index;
                uint32_t slot_id;
            };
            struct kSendWorldAction {
                WorldActionId action_id;
                GW::AgentID agent_id;
                bool suppress_call_target; // 1 to block "I'm targetting X", but will also only trigger if the key thing is down
            };
            struct kAllyOrGuildMessage {
                GW::Chat::Channel channel;
                wchar_t* message;
                wchar_t* sender;
                wchar_t* guild_tag;
            };
        }
    }
}
