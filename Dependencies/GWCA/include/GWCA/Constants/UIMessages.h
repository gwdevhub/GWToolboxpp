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
            kToggleButtonDown,             // 0x2c
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
            kAgentUpdate        = 0x10000007,        // 0x10000007, wparam = uint32_t agent_id
            kAgentDestroy       = 0x10000008,        // 0x10000008, wparam = uint32_t agent_id
            kUpdateAgentEffects = 0x10000009,        // 0x10000009
            kAgentSpeechBubble = 0x10000017,         // 0x10000017
            kShowAgentNameTag = 0x10000019,          // 0x10000019, wparam = AgentNameTagInfo*
            kHideAgentNameTag = 0x1000001A,          // 0x1000001A
            kSetAgentNameTagAttribs = 0x1000001B,    // 0x1000001B, wparam = AgentNameTagInfo*
            kSetAgentProfession = 0x1000001D,        // 0x1000001D, wparam = UIPacket::kSetAgentProfession*
            kChangeTarget = 0x10000020,              // 0x10000020, wparam = UIPacket::kChangeTarget*

            kAgentSkillActivated = 0x10000024, // kAgentSkillPacket
            kAgentSkillActivatedInstantly = 0x10000025, // kAgentSkillPacket
            kAgentSkillCancelled = 0x10000026,       // kAgentSkillPacket
            kAgentSkillStartedCast = 0x10000027,     // 0x10000027, wparam = UIPacket::kAgentStartCasting*

            kShowMapEntryMessage = 0x10000029,       // 0x10000029, wparam = { wchar_t* title, wchar_t* subtitle }
            kSetCurrentPlayerData = 0x1000002A,      // 0x1000002A, fired after setting the worldcontext player name
            kPostProcessingEffect = 0x10000034,      // 0x10000034, Triggered when drunk. wparam = UIPacket::kPostProcessingEffect
            kHeroAgentAdded = 0x10000038,            // 0x10000038, hero assigned to agent/inventory/ai mode
            kHeroDataAdded = 0x10000039,             // 0x10000039, hero info received from server (name, level etc)
            kShowXunlaiChest = 0x10000040,           // 0x10000040
            kMinionCountUpdated = 0x10000046,        // 0x10000046
            kMoraleChange = 0x10000047,              // 0x10000047, wparam = {agent id, morale percent }
            kLoginStateChanged = 0x10000050,         // 0x10000050, wparam = {bool is_logged_in, bool unk }
            kEffectAdd = 0x10000055,                 // 0x10000055, wparam = {agent_id, GW::Effect*}
            kEffectRenew = 0x10000056,               // 0x10000056, wparam = GW::Effect*
            kEffectRemove = 0x10000057,              // 0x10000057, wparam = effect id
            kSkillActivated = 0x1000005b,            // 0x1000005b, wparam ={ uint32_t agent_id , uint32_t skill_id }
            kUpdateSkillbar = 0x1000005E,            // 0x1000005E, wparam ={ uint32_t agent_id , ... }
            kUpdateSkillsAvailable = 0x1000005f,     // 0x1000005f, Triggered on a skill unlock, profession change or map load
            kPlayerTitleChanged = 0x10000064,        // 0x10000064, wparam = { uint32_t player_id, uint32_t title_id }
            kTitleProgressUpdated = 0x10000065,      // 0x10000065, wparam = title_id
            kExperienceGained = 0x10000066,          // 0x10000066, wparam = experience amount
            kWriteToChatLog = 0x1000007F,                // 0x1000007F, wparam = UIPacket::kWriteToChatLog*
            kWriteToChatLogWithSender = 0x10000080,      // 0x10000080, wparam = UIPacket::kWriteToChatLogWithSender*
            kAllyOrGuildMessage = 0x10000081,            // 0x10000081, wparam = UIPacket::kAllyOrGuildMessage*
            kPlayerChatMessage = 0x10000082,             // 0x10000082, wparam = UIPacket::kPlayerChatMessage*
            kFloatingWindowMoved = 0x10000084,           // 0x10000084, wparam = frame_id

            kFriendUpdated = 0x1000008B,                 // 0x1000008B, wparam = { GW::Friend*, ... }
            kMapLoaded = 0x1000008C,                     // 0x1000008C
            kOpenWhisper = 0x10000092,                   // 0x10000092, wparam = wchar* name
            kLoadMapContext = 0x10000098,                // 0x10000098, wparam = UIPacket::kLoadMapContext
            kLogout = 0x1000009D,                        // 0x1000009D, wparam = { bool unknown, bool character_select }
            kCompassDraw = 0x1000009E,                   // 0x1000009E, wparam = UIPacket::kCompassDraw*
            kOnScreenMessage = 0x100000A2,               // 0x100000A2, wparam = wchar_** encoded_string
            kDialogButton = 0x100000A3,                  // 0x100000A3, wparam = DialogButtonInfo*
            kDialogBody = 0x100000A6,                    // 0x100000A6, wparam = DialogBodyInfo*
            kTargetNPCPartyMember = 0x100000B3,          // 0x100000B3, wparam = { uint32_t unk, uint32_t agent_id }
            kTargetPlayerPartyMember = 0x100000B4,       // 0x100000B4, wparam = { uint32_t unk, uint32_t player_number }
            kVendorWindow = 0x100000B5,                  // 0x100000B5, wparam = UIPacket::kVendorWindow
            kVendorItems = 0x100000B9,                   // 0x100000B9, wparam = UIPacket::kVendorItems
            kVendorTransComplete = 0x100000BB,           // 0x100000BB, wparam = *TransactionType
            kVendorQuote = 0x100000BD,                   // 0x100000BD, wparam = UIPacket::kVendorQuote
            kStartMapLoad = 0x100000C2,                  // 0x100000C2, wparam = { uint32_t map_id, ...}
            kWorldMapUpdated = 0x100000C7,               // 0x100000C7, Triggered when an area in the world map has been discovered/updated
            kGuildMemberUpdated = 0x100000DA,            // 0x100000DA, wparam = { GuildPlayer::name_ptr }
            kShowHint = 0x100000E1,                      // 0x100000E1, wparam = { uint32_t icon_type, wchar_t* message_enc }
            kWeaponSetSwapComplete = 0x100000E9,         // 0x100000E9, wparam = UIPacket::kWeaponSwap*
            kWeaponSetSwapCancel = 0x100000EA,           // 0x100000EA
            kWeaponSetUpdated = 0x100000EB,              // 0x100000EB
            kUpdateGoldCharacter = 0x100000EC,           // 0x100000EC, wparam = { uint32_t unk, uint32_t gold_character }
            kUpdateGoldStorage = 0x100000ED,             // 0x100000ED, wparam = { uint32_t unk, uint32_t gold_storage }
            kInventorySlotUpdated = 0x100000EE,          // 0x100000EE, Triggered when an item is moved into a slot
            kEquipmentSlotUpdated = 0x100000EF,          // 0x100000EF, Triggered when an item is moved into a slot
            kInventorySlotCleared = 0x100000F1,          // 0x100000F1, Triggered when an item has been removed from a slot
            kEquipmentSlotCleared = 0x100000F2,          // 0x100000F2, Triggered when an item has been removed from a slot
            kPvPWindowContent = 0x100000FA,              // 0x100000FA
            kPreStartSalvage = 0x10000102,               // 0x10000102, { uint32_t item_id, uint32_t kit_id }
            kTomeSkillSelection = 0x10000103,            // 0x10000103, wparam = UIPacket::kTomeSkillSelection*
            kTradePlayerUpdated = 0x10000105,            // 0x10000105, wparam = GW::TraderPlayer*
            kItemUpdated = 0x10000106,                   // 0x10000106, wparam = UIPacket::kItemUpdated*
            kMapChange = 0x10000111,                     // 0x10000111, wparam = map id
            kCalledTargetChange = 0x10000115,            // 0x10000115, wparam = { player_number, target_id }
            kErrorMessage = 0x10000119,                  // 0x10000119, wparam = { int error_index, wchar_t* error_encoded_string }
            kPartyHardModeChanged = 0x1000011A,          // 0x1000011A, wparam = { int is_hard_mode }
            kPartyAddHenchman = 0x1000011B,              // 0x1000011B
            kPartyRemoveHenchman = 0x1000011C,           // 0x1000011C
            kPartyAddHero = 0x1000011E,                  // 0x1000011E
            kPartyRemoveHero = 0x1000011F,               // 0x1000011F
            kPartyAddPlayer = 0x10000124,                // 0x10000124
            kPartyRemovePlayer = 0x10000126,             // 0x10000126
            kDisableEnterMissionBtn = 0x1000012A,        // 0x1000012A, wparam = boolean (1 = disabled, 0 = enabled)
            kShowCancelEnterMissionBtn = 0x1000012D,     // 0x1000012D
            kPartyDefeated = 0x1000012F,                 // 0x1000012F
            kPartySearchInviteReceived = 0x10000137,     // 0x10000137, wparam = UIPacket::kPartySearchInviteReceived*
            kPartySearchInviteSent = 0x10000139,         // 0x10000139
            kPartyShowConfirmDialog = 0x1000013A,        // 0x1000013A, wparam = UIPacket::kPartyShowConfirmDialog

            kPreferenceEnumChanged = 0x10000140,         // 0x10000140, wparam = UiPacket::kPreferenceEnumChanged
            kPreferenceFlagChanged = 0x10000141,         // 0x10000141, wparam = UiPacket::kPreferenceFlagChanged
            kPreferenceValueChanged = 0x10000142,        // 0x10000142, wparam = UiPacket::kPreferenceValueChanged

            kUIPositionChanged = 0x10000143,             // 0x10000143, wparam = UIPacket::kUIPositionChanged
            kPreBuildLoginScene = 0x10000144,            // 0x10000144, Called with no args right before login scene is drawn
            kQuestAdded = 0x1000014E,                    // 0x1000014E, wparam = { quest_id, ... }
            kQuestDetailsChanged = 0x1000014F,           // 0x1000014F, wparam = { quest_id, ... }
            kQuestRemoved = 0x10000150,                  // 0x10000150, wparam = { quest_id, ... }
            kClientActiveQuestChanged = 0x10000151,      // 0x10000151, wparam = { quest_id, ... }. Triggered when the game requests the current quest to change
            kServerActiveQuestChanged = 0x10000153,      // 0x10000153, wparam = UIPacket::kServerActiveQuestChanged*. Triggered when the server requests the current quest to change
            kUnknownQuestRelated = 0x10000154,           // 0x10000154
            kDungeonComplete = 0x10000156,               // 0x10000156
            kMissionComplete = 0x10000157,               // 0x10000157
            kVanquishComplete = 0x10000159,              // 0x10000159
            kObjectiveAdd = 0x1000015A,                  // 0x1000015A, wparam = UIPacket::kObjectiveAdd*
            kObjectiveComplete = 0x1000015B,             // 0x1000015B, wparam = UIPacket::kObjectiveComplete*
            kObjectiveUpdated = 0x1000015C,              // 0x1000015C, wparam = UIPacket::kObjectiveUpdated*
            kTradeSessionStart = 0x10000162,             // 0x10000162, wparam = { trade_state, player_number }
            kTradeSessionUpdated = 0x10000168,           // 0x10000168, no args
            kTriggerLogoutPrompt = 0x1000016E,           // 0x1000016E, no args
            kToggleOptionsWindow = 0x1000016F,           // 0x1000016F, no args
            kCheckUIState = 0x10000172,                  // 0x10000172
            kRedrawItem = 0x10000174,                    // 0x10000174, wparam = uint32_t item_id
            kCloseSettings = 0x10000176,                 // 0x10000176
            kChangeSettingsTab = 0x10000177,             // 0x10000177, wparam = uint32_t is_interface_tab

            kGuildHall = 0x1000017C,                     // 0x1000017C, wparam = gh key (uint32_t[4])
            kLeaveGuildHall = 0x1000017E,                // 0x1000017E
            kTravel = 0x1000017F,                        // 0x1000017F
            kOpenWikiUrl = 0x10000180,                   // 0x10000180, wparam = char* url
            kAppendMessageToChat = 0x1000018E,           // 0x1000018E, wparam = wchar_t* message
            kHideHeroPanel = 0x1000019C,                 // 0x1000019C, wparam = hero_id
            kShowHeroPanel = 0x1000019D,                 // 0x1000019D, wparam = hero_id
            kGetInventoryAgentId = 0x100001A1,           // 0x100001A1, wparam = 0, lparam = uint32_t* agent_id_out. Used to fetch which agent is selected
            kEquipItem = 0x100001A2,                     // 0x100001A2, wparam = { item_id, agent_id }
            kMoveItem = 0x100001A3,                      // 0x100001A3, wparam = { item_id, to_bag, to_slot, bool prompt }
            kInitiateTrade = 0x100001A5,                 // 0x100001A5
            kInventoryAgentChanged = 0x100001B5,         // 0x100001B5, Triggered when inventory needs updating due to agent change; no args
            kOpenTemplate = 0x100001BE,                  // 0x100001BE, wparam = GW::UI::ChatTemplate*

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
