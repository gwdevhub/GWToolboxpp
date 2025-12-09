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
        enum class FlagPreference: uint32_t;
        enum class NumberPreference : uint32_t;
        enum class StringPreference : uint32_t;
        enum class EnumPreference : uint32_t;
        struct CompassPoint;

        enum class UIMessage : uint32_t {
            kNone = 0x0,
            kFrameMessage_0x1,
            kFrameMessage_0x2,
            kFrameMessage_0x3,
            kFrameMessage_0x4,
            kFrameMessage_0x5,
            kFrameMessage_0x6,
            kFrameMessage_0x7,
            kResize,                        // 0x8
            kInitFrame,                     // 0x9
            kFrameMessage_0xa,
            kDestroyFrame,                  // 0xb
            kFrameMessage_0xc,
            kFrameMessage_0xd,
            kFrameMessage_0xe,
            kFrameMessage_0xf,
            kFrameMessage_0x10,
            kFrameMessage_0x11,
            kFrameMessage_0x12,
            kFrameMessage_0x13,
            kFrameMessage_0x14,
            kFrameMessage_0x15,
            kFrameMessage_0x16,
            kFrameMessage_0x17,
            kFrameMessage_0x18,
            kFrameMessage_0x19,
            kFrameMessage_0x1a,
            kFrameMessage_0x1b,
            kFrameMessage_0x1c,
            kFrameMessage_0x1d,
            kKeyDown = 0x20,                       // wparam = UIPacket::kKeyAction* - Updated from 0x1e to 0x20
            kSetFocus,                      // 0x1f, wparam = 1 or 0
            kKeyUp,                         // 0x20, wparam = UIPacket::kKeyAction*
            kFrameMessage_0x21,             // 0x21
            kMouseClick,                    // 0x22, wparam = UIPacket::kMouseClick*
            kFrameMessage_0x23,             // 0x23
            kMouseCoordsClick,              // 0x24, wparam = UIPacket::kMouseCoordsClick*
            kFrameMessage_0x25,             // 0x25
            kMouseUp,                       // 0x26, wparam = UIPacket::kMouseCoordsClick*
            kFrameMessage_0x27,             // 0x27
            kFrameMessage_0x28,             // 0x28
            kFrameMessage_0x29,             // 0x29
            kFrameMessage_0x2a,             // 0x2a
            kFrameMessage_0x2b,             // 0x2b
            kFrameMessage_0x2c,             // 0x2c
            kFrameMessage_0x2d,             // 0x2d
            kMouseClick2 = 0x31,                   // 0x2e, wparam = UIPacket::kMouseAction*
            kMouseAction,                   // 0x2f, wparam = UIPacket::kMouseAction*
            kRenderFrame_0x30,              // 0x30
            kRenderFrame_0x31 = 0x35,              // 0x31
            kRenderFrame_0x32,              // 0x32
            kSetLayout,                     // 0x33
            kMeasureContent,                // 0x34
            kFrameMessage_0x35,             // 0x35
            kFrameMessage_0x36,             // 0x36
            kRefreshContent,                // 0x37
            kFrameMessage_0x38,
            kFrameMessage_0x39,
            kFrameMessage_0x3a,
            kFrameMessage_0x3b,
            kFrameMessage_0x3c = 0x44,
            kFrameMessage_0x3d,
            kFrameMessage_0x3e,
            kFrameMessage_0x3f,
            kFrameMessage_0x40,
            kFrameMessage_0x41,
            kFrameMessage_0x42,
            kRenderFrame_0x43,              // 0x43
            kFrameMessage_0x44 = 0x51,
            kFrameMessage_0x45,
            kFrameMessage_0x46,
            kFrameMessage_0x47 = 0x55,             // 0x47, Multiple uses depending on frame
            kFrameMessage_0x48,             // 0x48, Multiple uses depending on frame
            kFrameMessage_0x49,             // 0x49, Multiple uses depending on frame
            kFrameMessage_0x4a,             // 0x4a, Multiple uses depending on frame
            kFrameMessage_0x4b,             // 0x4b, Multiple uses depending on frame
            kFrameMessage_0x4c,             // 0x4c, Multiple uses depending on frame
            kFrameMessage_0x4d,             // 0x4d, Multiple uses depending on frame
            kFrameMessage_0x4e,             // 0x4e, Multiple uses depending on frame
            kFrameMessage_0x4f,             // 0x4f, Multiple uses depending on frame
            kFrameMessage_0x50,             // 0x50, Multiple uses depending on frame
            kFrameMessage_0x51,             // 0x51, Multiple uses depending on frame
            kFrameMessage_0x52,             // 0x52, Multiple uses depending on frame
            kFrameMessage_0x53,             // 0x53, Multiple uses depending on frame
            kFrameMessage_0x54,             // 0x54, Multiple uses depending on frame
            kFrameMessage_0x55,             // 0x55, Multiple uses depending on frame
            kFrameMessage_0x56,             // 0x56, Multiple uses depending on frame
            kFrameMessage_0x57,             // 0x57, Multiple uses depending on frame

            // High bit messages start at 0x10000000
            kHighBitBase = 0x10000000,
            kFrameMessage_0x10000001,
            kFrameMessage_0x10000002,
            kFrameMessage_0x10000003,
            kFrameMessage_0x10000004,
            kFrameMessage_0x10000005,
            kFrameMessage_0x10000006,
            kRerenderAgentModel,            // 0x10000007, wparam = uint32_t agent_id
            kFrameMessage_0x10000008,
            kUpdateAgentEffects,            // 0x10000009
            kFrameMessage_0x1000000a,
            kFrameMessage_0x1000000b,
            kFrameMessage_0x1000000c,
            kFrameMessage_0x1000000d,
            kFrameMessage_0x1000000e,
            kFrameMessage_0x1000000f,
            kFrameMessage_0x10000010,
            kFrameMessage_0x10000011,
            kFrameMessage_0x10000012,
            kFrameMessage_0x10000013,
            kFrameMessage_0x10000014,
            kFrameMessage_0x10000015,
            kFrameMessage_0x10000016,
            kAgentSpeechBubble,             // 0x10000017
            kFrameMessage_0x10000018,
            kShowAgentNameTag,              // 0x10000019, wparam = AgentNameTagInfo*
            kHideAgentNameTag,              // 0x1000001A
            kSetAgentNameTagAttribs,        // 0x1000001B, wparam = AgentNameTagInfo*
            kFrameMessage_0x1000001c,
            kSetAgentProfession,            // 0x1000001D, wparam = UIPacket::kSetAgentProfession*
            kFrameMessage_0x1000001e,
            kFrameMessage_0x1000001f,
            kChangeTarget,                  // 0x10000020, wparam = UIPacket::kChangeTarget*
            kFrameMessage_0x10000021,
            kFrameMessage_0x10000022,
            kFrameMessage_0x10000023,
            kFrameMessage_0x10000024,
            kFrameMessage_0x10000025,
            kFrameMessage_0x10000026,
            kAgentStartCasting,             // 0x10000027, wparam = UIPacket::kAgentStartCasting*
            kFrameMessage_0x10000028,
            kShowMapEntryMessage,           // 0x10000029, wparam = { wchar_t* title, wchar_t* subtitle }
            kSetCurrentPlayerData,          // 0x1000002A, fired after setting the worldcontext player name
            kFrameMessage_0x1000002b,
            kFrameMessage_0x1000002c,
            kFrameMessage_0x1000002d,
            kFrameMessage_0x1000002e,
            kFrameMessage_0x1000002f,
            kFrameMessage_0x10000030,
            kFrameMessage_0x10000031,
            kFrameMessage_0x10000032,
            kFrameMessage_0x10000033,
            kPostProcessingEffect,          // 0x10000034, Triggered when drunk. wparam = UIPacket::kPostProcessingEffect
            kFrameMessage_0x10000035,
            kFrameMessage_0x10000036,
            kFrameMessage_0x10000037,
            kHeroAgentAdded,                // 0x10000038, hero assigned to agent/inventory/ai mode
            kHeroDataAdded,                 // 0x10000039, hero info received from server (name, level etc)
            kFrameMessage_0x1000003a,
            kFrameMessage_0x1000003b,
            kFrameMessage_0x1000003c,
            kFrameMessage_0x1000003d,
            kFrameMessage_0x1000003e,
            kFrameMessage_0x1000003f,
            kShowXunlaiChest,               // 0x10000040
            kFrameMessage_0x10000041,
            kFrameMessage_0x10000042,
            kFrameMessage_0x10000043,
            kFrameMessage_0x10000044,
            kFrameMessage_0x10000045,
            kMinionCountUpdated,            // 0x10000046
            kMoraleChange,                  // 0x10000047, wparam = {agent id, morale percent }
            kFrameMessage_0x10000048,
            kFrameMessage_0x10000049,
            kFrameMessage_0x1000004a,
            kFrameMessage_0x1000004b,
            kFrameMessage_0x1000004c,
            kFrameMessage_0x1000004d,
            kFrameMessage_0x1000004e,
            kFrameMessage_0x1000004f,
            kLoginStateChanged,             // 0x10000050, wparam = {bool is_logged_in, bool unk }
            kFrameMessage_0x10000051,
            kFrameMessage_0x10000052,
            kFrameMessage_0x10000053,
            kFrameMessage_0x10000054,
            kEffectAdd,                     // 0x10000055, wparam = {agent_id, GW::Effect*}
            kEffectRenew,                   // 0x10000056, wparam = GW::Effect*
            kEffectRemove,                  // 0x10000057, wparam = effect id
            kFrameMessage_0x10000058,
            kFrameMessage_0x10000059,
            kFrameMessage_0x1000005a,
            kSkillActivated,                // 0x1000005b, wparam ={ uint32_t agent_id , uint32_t skill_id }
            kFrameMessage_0x1000005c,
            kFrameMessage_0x1000005d,
            kUpdateSkillbar,                // 0x1000005E, wparam ={ uint32_t agent_id , ... }
            kUpdateSkillsAvailable,         // 0x1000005f, Triggered on a skill unlock, profession change or map load
            kFrameMessage_0x10000060,
            kFrameMessage_0x10000061,
            kFrameMessage_0x10000062,
            kFrameMessage_0x10000063,
            kPlayerTitleChanged,            // 0x10000064, wparam = { uint32_t player_id, uint32_t title_id }
            kTitleProgressUpdated,          // 0x10000065, wparam = title_id
            kExperienceGained,              // 0x10000066, wparam = experience amount
            kFrameMessage_0x10000067,
            kFrameMessage_0x10000068,
            kFrameMessage_0x10000069,
            kFrameMessage_0x1000006a,
            kFrameMessage_0x1000006b,
            kFrameMessage_0x1000006c,
            kFrameMessage_0x1000006d,
            kFrameMessage_0x1000006e,
            kFrameMessage_0x1000006f,
            kFrameMessage_0x10000070,
            kFrameMessage_0x10000071,
            kFrameMessage_0x10000072,
            kFrameMessage_0x10000073,
            kFrameMessage_0x10000074,
            kFrameMessage_0x10000075,
            kFrameMessage_0x10000076,
            kFrameMessage_0x10000077,
            kFrameMessage_0x10000078,
            kFrameMessage_0x10000079,
            kFrameMessage_0x1000007a,
            kFrameMessage_0x1000007b,
            kFrameMessage_0x1000007c,
            kFrameMessage_0x1000007d,
            kWriteToChatLog,                // 0x1000007E, wparam = UIPacket::kWriteToChatLog*
            kWriteToChatLogWithSender,      // 0x1000007f, wparam = UIPacket::kWriteToChatLogWithSender*
            kAllyOrGuildMessage,            // 0x10000080, wparam = UIPacket::kAllyOrGuildMessage*
            kPlayerChatMessage,             // 0x10000081, wparam = UIPacket::kPlayerChatMessage*
            kFrameMessage_0x10000082,
            kFloatingWindowMoved,           // 0x10000083, wparam = frame_id
            kFrameMessage_0x10000084,
            kFrameMessage_0x10000085,
            kFrameMessage_0x10000086,
            kFrameMessage_0x10000087,
            kFrameMessage_0x10000088,
            kFriendUpdated,                 // 0x10000089, wparam = { GW::Friend*, ... }
            kMapLoaded,                     // 0x1000008A
            kFrameMessage_0x1000008b,
            kFrameMessage_0x1000008c,
            kFrameMessage_0x1000008d,
            kFrameMessage_0x1000008e,
            kFrameMessage_0x1000008f,
            kOpenWhisper,                   // 0x10000090, wparam = wchar* name
            kFrameMessage_0x10000091,
            kFrameMessage_0x10000092,
            kFrameMessage_0x10000093,
            kFrameMessage_0x10000094,
            kFrameMessage_0x10000095,
            kLoadMapContext,               // wparam = UIPacket::kLoadMapContext
            kFrameMessage_0x10000097,
            kFrameMessage_0x10000098,
            kFrameMessage_0x10000099,
            kFrameMessage_0x1000009a,
            kLogout,                        // 0x1000009b, wparam = { bool unknown, bool character_select }
            kCompassDraw,                   // 0x1000009c, wparam = UIPacket::kCompassDraw*
            kFrameMessage_0x1000009d,
            kFrameMessage_0x1000009e,
            kFrameMessage_0x1000009f,
            kOnScreenMessage,               // 0x100000A0, wparam = wchar_** encoded_string
            kDialogButton,                  // 0x100000A1, wparam = DialogButtonInfo*
            kFrameMessage_0x100000a2,
            kFrameMessage_0x100000a3,
            kDialogBody,                    // 0x100000A4, wparam = DialogBodyInfo*
            kFrameMessage_0x100000a5,
            kFrameMessage_0x100000a6,
            kFrameMessage_0x100000a7,
            kFrameMessage_0x100000a8,
            kFrameMessage_0x100000a9,
            kFrameMessage_0x100000aa,
            kFrameMessage_0x100000ab,
            kFrameMessage_0x100000ac,
            kFrameMessage_0x100000ad,
            kFrameMessage_0x100000ae,
            kFrameMessage_0x100000af,
            kFrameMessage_0x100000b0,
            kTargetNPCPartyMember,          // 0x100000B1, wparam = { uint32_t unk, uint32_t agent_id }
            kTargetPlayerPartyMember,       // 0x100000B2, wparam = { uint32_t unk, uint32_t player_number }
            kVendorWindow,                  // 0x100000B3, wparam = UIPacket::kVendorWindow
            kFrameMessage_0x100000b4,
            kFrameMessage_0x100000b5,
            kFrameMessage_0x100000b6,
            kVendorItems,                   // 0x100000B7, wparam = UIPacket::kVendorItems
            kFrameMessage_0x100000b8,
            kVendorTransComplete,           // 0x100000B9, wparam = *TransactionType
            kFrameMessage_0x100000ba,
            kVendorQuote,                   // 0x100000BB, wparam = UIPacket::kVendorQuote
            kFrameMessage_0x100000bc,
            kFrameMessage_0x100000bd,
            kFrameMessage_0x100000be,
            kFrameMessage_0x100000bf,
            kStartMapLoad,                  // 0x100000C0, wparam = { uint32_t map_id, ...}
            kFrameMessage_0x100000c1,
            kFrameMessage_0x100000c2,
            kFrameMessage_0x100000c3,
            kFrameMessage_0x100000c4,
            kWorldMapUpdated,               // 0x100000C5, Triggered when an area in the world map has been discovered/updated
            kFrameMessage_0x100000c6,
            kFrameMessage_0x100000c7,
            kFrameMessage_0x100000c8,
            kFrameMessage_0x100000c9,
            kFrameMessage_0x100000ca,
            kFrameMessage_0x100000cb,
            kFrameMessage_0x100000cc,
            kFrameMessage_0x100000cd,
            kFrameMessage_0x100000ce,
            kFrameMessage_0x100000cf,
            kFrameMessage_0x100000d0,
            kFrameMessage_0x100000d1,
            kFrameMessage_0x100000d2,
            kFrameMessage_0x100000d3,
            kFrameMessage_0x100000d4,
            kFrameMessage_0x100000d5,
            kFrameMessage_0x100000d6,
            kFrameMessage_0x100000d7,
            kGuildMemberUpdated,            // 0x100000D8, wparam = { GuildPlayer::name_ptr }
            kFrameMessage_0x100000d9,
            kFrameMessage_0x100000da,
            kFrameMessage_0x100000db,
            kFrameMessage_0x100000dc,
            kFrameMessage_0x100000dd,
            kFrameMessage_0x100000de,
            kShowHint,                      // 0x100000DF, wparam = { uint32_t icon_type, wchar_t* message_enc }
            kFrameMessage_0x100000e0,
            kFrameMessage_0x100000e1,
            kFrameMessage_0x100000e2,
            kFrameMessage_0x100000e3,
            kFrameMessage_0x100000e4,
            kFrameMessage_0x100000e5,
            kFrameMessage_0x100000e6,
            kWeaponSetSwapComplete,         // 0x100000E7, wparam = UIPacket::kWeaponSwap*
            kWeaponSetSwapCancel,           // 0x100000E8
            kWeaponSetUpdated,              // 0x100000E9
            kUpdateGoldCharacter,           // 0x100000EA, wparam = { uint32_t unk, uint32_t gold_character }
            kUpdateGoldStorage,             // 0x100000EB, wparam = { uint32_t unk, uint32_t gold_storage }
            kInventorySlotUpdated,          // 0x100000EC, Triggered when an item is moved into a slot
            kEquipmentSlotUpdated,          // 0x100000ED, Triggered when an item is moved into a slot
            kFrameMessage_0x100000ee,
            kInventorySlotCleared,          // 0x100000EF, Triggered when an item has been removed from a slot
            kEquipmentSlotCleared,          // 0x100000F0, Triggered when an item has been removed from a slot
            kFrameMessage_0x100000f1,
            kFrameMessage_0x100000f2,
            kFrameMessage_0x100000f3,
            kFrameMessage_0x100000f4,
            kFrameMessage_0x100000f5,
            kFrameMessage_0x100000f6,
            kFrameMessage_0x100000f7,
            kPvPWindowContent,              // 0x100000F8
            kFrameMessage_0x100000f9,
            kFrameMessage_0x100000fa,
            kFrameMessage_0x100000fb,
            kFrameMessage_0x100000fc,
            kFrameMessage_0x100000fd,
            kFrameMessage_0x100000fe,
            kFrameMessage_0x100000ff,
            kPreStartSalvage,               // 0x10000100, { uint32_t item_id, uint32_t kit_id }
            kTomeSkillSelection,            // 0x10000101, wparam = UIPacket::kTomeSkillSelection*
            kFrameMessage_0x10000102,
            kTradePlayerUpdated,            // 0x10000103, wparam = GW::TraderPlayer*
            kItemUpdated,                   // 0x10000104, wparam = UIPacket::kItemUpdated*
            kFrameMessage_0x10000105,
            kFrameMessage_0x10000106,
            kFrameMessage_0x10000107,
            kFrameMessage_0x10000108,
            kFrameMessage_0x10000109,
            kFrameMessage_0x1000010a,
            kFrameMessage_0x1000010b,
            kFrameMessage_0x1000010c,
            kFrameMessage_0x1000010d,
            kFrameMessage_0x1000010e,
            kMapChange,                     // 0x1000010F, wparam = map id
            kFrameMessage_0x10000110,
            kFrameMessage_0x10000111,
            kFrameMessage_0x10000112,
            kCalledTargetChange,            // 0x10000113, wparam = { player_number, target_id }
            kFrameMessage_0x10000114,
            kFrameMessage_0x10000115,
            kFrameMessage_0x10000116,
            kErrorMessage,                  // 0x10000117, wparam = { int error_index, wchar_t* error_encoded_string }
            kPartyHardModeChanged,          // 0x10000118, wparam = { int is_hard_mode }
            kPartyAddHenchman,              // 0x10000119
            kPartyRemoveHenchman,           // 0x1000011a
            kFrameMessage_0x1000011b,
            kPartyAddHero,                  // 0x1000011c
            kPartyRemoveHero,               // 0x1000011d
            kFrameMessage_0x1000011e,
            kFrameMessage_0x1000011f,
            kFrameMessage_0x10000120,
            kFrameMessage_0x10000121,
            kPartyAddPlayer,                // 0x10000122
            kFrameMessage_0x10000123,
            kPartyRemovePlayer,             // 0x10000124
            kFrameMessage_0x10000125,
            kFrameMessage_0x10000126,
            kFrameMessage_0x10000127,
            kDisableEnterMissionBtn,        // 0x10000128, wparam = boolean (1 = disabled, 0 = enabled)
            kFrameMessage_0x10000129,
            kFrameMessage_0x1000012a,
            kShowCancelEnterMissionBtn,     // 0x1000012b
            kFrameMessage_0x1000012c,
            kPartyDefeated,                 // 0x1000012d
            kFrameMessage_0x1000012e,
            kFrameMessage_0x1000012f,
            kFrameMessage_0x10000130,
            kFrameMessage_0x10000131,
            kFrameMessage_0x10000132,
            kFrameMessage_0x10000133,
            kFrameMessage_0x10000134,
            kPartySearchInviteReceived,     // 0x10000135, wparam = UIPacket::kPartySearchInviteReceived*
            kFrameMessage_0x10000136,
            kPartySearchInviteSent,         // 0x10000137
            kPartyShowConfirmDialog,        // 0x10000138, wparam = UIPacket::kPartyShowConfirmDialog
            kFrameMessage_0x10000139,
            kFrameMessage_0x1000013a,
            kFrameMessage_0x1000013b,
            kFrameMessage_0x1000013c,
            kFrameMessage_0x1000013d,
            kPreferenceEnumChanged,         // 0x1000013E, wparam = UiPacket::kPreferenceEnumChanged
            kPreferenceFlagChanged,         // 0x1000013F, wparam = UiPacket::kPreferenceFlagChanged
            kPreferenceValueChanged,        // 0x10000140, wparam = UiPacket::kPreferenceValueChanged
            kUIPositionChanged,             // 0x10000141, wparam = UIPacket::kUIPositionChanged
            kPreBuildLoginScene,            // 0x10000142, Called with no args right before login scene is drawn
            kFrameMessage_0x10000143,
            kFrameMessage_0x10000144,
            kFrameMessage_0x10000145    = 0x10000147,
            kFrameMessage_0x10000146,
            kFrameMessage_0x10000147,
            kFrameMessage_0x10000148,
            kQuestAdded                 = 0x1000014c,                    // 0x10000149, wparam = { quest_id, ... }
            kQuestDetailsChanged,           // 0x1000014A, wparam = { quest_id, ... }
            kQuestRemoved,                  // 0x1000014B, wparam = { quest_id, ... }
            kClientActiveQuestChanged,      // 0x1000014C, wparam = { quest_id, ... }. Triggered when the game requests the current quest to change
            kFrameMessage_0x1000014d,
            kServerActiveQuestChanged,      // 0x1000014E, wparam = UIPacket::kServerActiveQuestChanged*. Triggered when the server requests the current quest to change
            kUnknownQuestRelated,           // 0x1000014F
            kFrameMessage_0x10000150,
            kDungeonComplete,               // 0x10000151
            kMissionComplete,               // 0x10000152
            kFrameMessage_0x10000153,
            kVanquishComplete,              // 0x10000154
            kObjectiveAdd,                  // 0x10000155, wparam = UIPacket::kObjectiveAdd*
            kObjectiveComplete,             // 0x10000156, wparam = UIPacket::kObjectiveComplete*
            kObjectiveUpdated,              // 0x10000157, wparam = UIPacket::kObjectiveUpdated*
            kFrameMessage_0x10000158,
            kFrameMessage_0x10000159,
            kFrameMessage_0x1000015a,
            kFrameMessage_0x1000015b,
            kFrameMessage_0x1000015c,
            kFrameMessage_0x1000015d,
            kFrameMessage_0x1000015e,
            kFrameMessage_0x1000015f,
            kTradeSessionStart,             // 0x10000160, wparam = { trade_state, player_number }
            kFrameMessage_0x10000161,
            kFrameMessage_0x10000162,
            kFrameMessage_0x10000163,
            kFrameMessage_0x10000164,
            kFrameMessage_0x10000165,
            kTradeSessionUpdated,           // 0x10000166, no args
            kFrameMessage_0x10000167,
            kFrameMessage_0x10000168,
            kFrameMessage_0x10000169,
            kFrameMessage_0x1000016a,
            kFrameMessage_0x1000016b,
            kTriggerLogoutPrompt,           // 0x1000016C, no args
            kToggleOptionsWindow,           // 0x1000016D, no args
            kFrameMessage_0x1000016e,
            kFrameMessage_0x1000016f,
            kCheckUIState,                  // 0x10000170
            kFrameMessage_0x10000171,
            kRedrawItem,                    // 0x10000172, wparam = uint32_t item_id
            kFrameMessage_0x10000173,
            kCloseSettings,                 // 0x10000174
            kChangeSettingsTab,             // 0x10000175, wparam = uint32_t is_interface_tab
            kFrameMessage_0x10000176,
            kGuildHall,                     // 0x10000177, wparam = gh key (uint32_t[4])
            kFrameMessage_0x10000178,
            kLeaveGuildHall,                // 0x10000179
            kTravel,                        // 0x1000017A
            kOpenWikiUrl,                   // 0x1000017B, wparam = char* url
            kFrameMessage_0x1000017c,
            kFrameMessage_0x1000017d,
            kFrameMessage_0x1000017e,
            kFrameMessage_0x1000017f,
            kFrameMessage_0x10000180,
            kFrameMessage_0x10000181,
            kFrameMessage_0x10000182,
            kFrameMessage_0x10000183,
            kFrameMessage_0x10000184,
            kFrameMessage_0x10000185,
            kFrameMessage_0x10000186,
            kFrameMessage_0x10000187,
            kFrameMessage_0x10000188,
            kAppendMessageToChat,           // 0x10000189, wparam = wchar_t* message
            kFrameMessage_0x1000018a,
            kFrameMessage_0x1000018b,
            kFrameMessage_0x1000018c,
            kFrameMessage_0x1000018d,
            kFrameMessage_0x1000018e,
            kFrameMessage_0x1000018f,
            kFrameMessage_0x10000190,
            kFrameMessage_0x10000191,
            kFrameMessage_0x10000192,
            kFrameMessage_0x10000193,
            kFrameMessage_0x10000194,
            kFrameMessage_0x10000195,
            kFrameMessage_0x10000196,
            kHideHeroPanel,                 // 0x10000197, wparam = hero_id
            kShowHeroPanel,                 // 0x10000198, wparam = hero_id
            kFrameMessage_0x10000199,
            kFrameMessage_0x1000019a,
            kFrameMessage_0x1000019b,
            kGetInventoryAgentId,           // 0x1000019c, wparam = 0, lparam = uint32_t* agent_id_out. Used to fetch which agent is selected
            kEquipItem,                     // 0x1000019d, wparam = { item_id, agent_id }
            kMoveItem,                      // 0x1000019e, wparam = { item_id, to_bag, to_slot, bool prompt }
            kFrameMessage_0x1000019f,
            kInitiateTrade,                 // 0x100001A0
            kFrameMessage_0x100001a1,
            kFrameMessage_0x100001a2,
            kFrameMessage_0x100001a3,
            kFrameMessage_0x100001a4,
            kFrameMessage_0x100001a5,
            kFrameMessage_0x100001a6,
            kFrameMessage_0x100001a7,
            kFrameMessage_0x100001a8,
            kFrameMessage_0x100001a9,
            kFrameMessage_0x100001aa,
            kFrameMessage_0x100001ab,
            kFrameMessage_0x100001ac,
            kFrameMessage_0x100001ad,
            kFrameMessage_0x100001ae,
            kFrameMessage_0x100001af,
            kInventoryAgentChanged,         // 0x100001b0, Triggered when inventory needs updating due to agent change; no args
            kFrameMessage_0x100001b1,
            kFrameMessage_0x100001b2,
            kFrameMessage_0x100001b3,
            kFrameMessage_0x100001b4,
            kFrameMessage_0x100001b5,
            kFrameMessage_0x100001b6,
            kFrameMessage_0x100001b7,
            kFrameMessage_0x100001b8,
            kOpenTemplate,                  // 0x100001B9, wparam = GW::UI::ChatTemplate*

            // GWCA Client to Server commands. Only added the ones that are used for hooks, everything else goes straight into GW

            kSendLoadSkillTemplate = 0x30000000 | 0x3,  // wparam = SkillbarMgr::SkillTemplate*
            kSendPingWeaponSet = 0x30000000 | 0x4,  // wparam = UIPacket::kSendPingWeaponSet*
            kSendMoveItem = 0x30000000 | 0x5,  // wparam = UIPacket::kSendMoveItem*
            kSendMerchantRequestQuote = 0x30000000 | 0x6,  // wparam = UIPacket::kSendMerchantRequestQuote*
            kSendMerchantTransactItem = 0x30000000 | 0x7,  // wparam = UIPacket::kSendMerchantTransactItem*
            kSendUseItem = 0x30000000 | 0x8,  // wparam = UIPacket::kSendUseItem*
            kSendSetActiveQuest = 0x30000000 | 0x9,  // wparam = uint32_t quest_id
            kSendAbandonQuest = 0x30000000 | 0xA, // wparam = uint32_t quest_id
            kSendChangeTarget = 0x30000000 | 0xB, // wparam = UIPacket::kSendChangeTarget* // e.g. tell the gw client to focus on a different target
            kSendCallTarget = 0x30000000 | 0x13, // wparam = { uint32_t call_type, uint32_t agent_id } // also used to broadcast morale, death penalty, "I'm following X", etc
            kSendDialog = 0x30000000 | 0x16, // wparam = dialog_id // internal use


            kStartWhisper = 0x30000000 | 0x17, // wparam = UIPacket::kStartWhisper*
            kGetSenderColor = 0x30000000 | 0x18, // wparam = UIPacket::kGetColor* // Get chat sender color depending on channel, output object passed by reference
            kGetMessageColor = 0x30000000 | 0x19, // wparam = UIPacket::kGetColor* // Get chat message color depending on channel, output object passed by reference
            kSendChatMessage = 0x30000000 | 0x1B, // wparam = UIPacket::kSendChatMessage*
            kLogChatMessage = 0x30000000 | 0x1D, // wparam = UIPacket::kLogChatMessage*. Triggered when a message wants to be added to the persistent chat log.
            kRecvWhisper = 0x30000000 | 0x1E, // wparam = UIPacket::kRecvWhisper*
            kPrintChatMessage = 0x30000000 | 0x1F, // wparam = UIPacket::kPrintChatMessage*. Triggered when a message wants to be added to the in-game chat window.
            kSendWorldAction = 0x30000000 | 0x20, // wparam = UIPacket::kSendWorldAction*
            kSetRendererValue = 0x30000000 | 0x21, // wparam = UIPacket::kSetRendererValue
            kIdentifyItem = 0x30000000 | 0x22  // wparam = UIPacket::kIdentifyItem
        };

        namespace UIPacket {
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
            struct kIdentifyItem {
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
            struct kAgentStartCasting {
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
                uint32_t h0004 = 0x4000;
                uint32_t h0008 = 6;
            };
            struct kMouseClick {
                uint32_t mouse_button; // 0x0 = left, 0x1 = middle, 0x2 = right
                uint32_t is_doubleclick;
                uint32_t unknown_type_screen_pos;
                uint32_t h000c;
                uint32_t h0010;
            };
            enum ActionState : uint32_t {
                MouseDown = 0x6,
                MouseUp = 0x7,
                MouseClick = 0x8,
                MouseDoubleClick = 0x9,
                DragRelease = 0xb,
                KeyDown = 0xe
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