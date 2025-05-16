#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MerchantMgr.h>

namespace GW {

    struct Module;
    extern Module UIModule;

    struct Effect;

    enum class CallTargetType : uint32_t;
    enum class WorldActionId : uint32_t;
    typedef uint32_t AgentID;

    namespace Merchant {
        enum class TransactionType : uint32_t;
    }
    namespace Constants {
        enum class Language;
        enum class MapID : uint32_t;
        enum class QuestID : uint32_t;
        enum class SkillID : uint32_t;
    }
    namespace Chat {
        enum Channel : int;
        typedef uint32_t Color;
    }
    namespace SkillbarMgr {
        struct SkillTemplate;
    }
    namespace UI {
        struct TooltipInfo;
        typedef GW::Array<unsigned char> ArrayByte;

        enum class UIMessage : uint32_t;

        struct CompassPoint {
            CompassPoint() : x(0), y(0) {}
            CompassPoint(int _x, int _y) : x(_x), y(_y) {}
            int x;
            int y;
        };

        typedef void(__cdecl* DecodeStr_Callback)(void* param, const wchar_t* s);

        struct ChatTemplate {
            uint32_t        agent_id;
            uint32_t        type; // 0 = build, 1 = equipment
            Array<wchar_t>  code;
            wchar_t        *name;
        };

        struct UIChatMessage {
            uint32_t channel;
            wchar_t* message;
            uint32_t channel2;
        };

        struct InteractionMessage {
            uint32_t frame_id;
            UI::UIMessage message_id; // Same as UIMessage from UIMgr, but includes things like mouse move, click etc
            void** wParam;
        };

        typedef void(__cdecl* UIInteractionCallback)(InteractionMessage* message, void* wParam, void* lParam);


        struct Frame;

        struct FrameRelation {
            FrameRelation* parent;
            uint32_t field67_0x124;
            uint32_t field68_0x128;
            uint32_t frame_hash_id;
            TList<FrameRelation> siblings;
            GWCA_API Frame* GetFrame();
            GWCA_API Frame* GetParent() const;
        };

        static_assert(sizeof(FrameRelation) == 0x1c);

        struct FramePosition {
            uint32_t flags;
            float left;
            float bottom;
            float right;
            float top;

            float content_left;
            float content_bottom;
            float content_right;
            float content_top;

            float unk;
            float scale_factor; // Depends on UI scale
            float viewport_width; // Width in px of available screen height; this may sometimes be scaled down, too!
            float viewport_height; // Height in px of available screen height; this may sometimes be scaled down, too!

            float screen_left;
            float screen_bottom;
            float screen_right;
            float screen_top;

            GWCA_API [[nodiscard]] GW::Vec2f GetTopLeftOnScreen(const Frame* frame = nullptr) const;
            GWCA_API [[nodiscard]] GW::Vec2f GetBottomRightOnScreen(const Frame* frame = nullptr) const;
            GWCA_API [[nodiscard]] GW::Vec2f GetContentTopLeft(const Frame* frame = nullptr) const;
            GWCA_API [[nodiscard]] GW::Vec2f GetContentBottomRight(const Frame* frame = nullptr) const;
            GWCA_API [[nodiscard]] GW::Vec2f GetSizeOnScreen(const Frame* frame = nullptr) const;
            GWCA_API [[nodiscard]] GW::Vec2f GetViewportScale(const Frame* frame = nullptr) const;
        };

        struct FrameInteractionCallback {
            UIInteractionCallback callback;
            void* uictl_context;
            uint32_t h0008;
        };

        struct Frame {
            uint32_t field1_0x0;
            uint32_t field2_0x4;
            RECT layout_position;
            uint32_t visibility_flags;
            uint32_t field7_0x1c;
            uint32_t type;
            uint32_t template_type;
            uint32_t field10_0x28;
            uint32_t field11_0x2c;
            uint32_t field12_0x30;
            uint32_t field13_0x34;
            uint32_t field14_0x38;
            uint32_t field15_0x3c;
            uint32_t field16_0x40;
            uint32_t field17_0x44;
            uint32_t field18_0x48;
            uint32_t field19_0x4c;
            uint32_t field20_0x50;
            uint32_t field21_0x54;
            uint32_t field22_0x58;
            uint32_t field23_0x5c;
            uint32_t field24_0x60;
            uint32_t field25_0x64;
            uint32_t field26_0x68;
            uint32_t field27_0x6c;
            uint32_t field28_0x70;
            uint32_t field29_0x74;
            uint32_t field30_0x78;
            GW::Array<void*> field31_0x7c;
            uint32_t field32_0x8c;
            uint32_t field33_0x90;
            uint32_t field34_0x94;
            uint32_t field35_0x98;
            uint32_t field36_0x9c;
            GW::Array<FrameInteractionCallback> frame_callbacks;
            uint32_t child_offset_id; // Offset of this child in relation to its parent
            uint32_t frame_id; // Offset in the global frame array
            uint32_t field40_0xb8;
            uint32_t field41_0xbc;
            uint32_t field42_0xc0;
            uint32_t field43_0xc4;
            uint32_t field44_0xc8;
            uint32_t field45_0xcc;
            FramePosition position;
            uint32_t field63_0x114;
            uint32_t field64_0x118;
            uint32_t field65_0x11c;
            FrameRelation relation;
            uint32_t field73_0x13c;
            uint32_t field74_0x140;
            uint32_t field75_0x144;
            uint32_t field76_0x148;
            uint32_t field77_0x14c;
            uint32_t field78_0x150;
            uint32_t field79_0x154;
            uint32_t field80_0x158;
            uint32_t field81_0x15c;
            uint32_t field82_0x160;
            uint32_t field83_0x164;
            uint32_t field84_0x168;
            uint32_t field85_0x16c;
            uint32_t field86_0x170;
            uint32_t field87_0x174;
            uint32_t field88_0x178;
            uint32_t field89_0x17c;
            uint32_t field90_0x180;
            uint32_t field91_0x184;
            uint32_t field92_0x188;
            uint32_t field93_0x18c;
            uint32_t field94_0x190;
            uint32_t field95_0x194;
            uint32_t field96_0x198;
            uint32_t field97_0x19c;
            uint32_t field98_0x1a0;
            TooltipInfo* tooltip_info;
            uint32_t field100_0x1a8;

            bool IsCreated() const {
                return (field91_0x184 & 0x4) != 0;
            }
            bool IsVisible() const {
                return !IsHidden();
            }
            bool IsHidden() const {
                return (field91_0x184 & 0x200) != 0;
            }

            bool IsDisabled() const {
                return (field91_0x184 & 0x10) != 0;
            }
        };
        static_assert(sizeof(Frame) == 0x1ac);

        static_assert(offsetof(Frame, relation) == 0x120);

        struct AgentNameTagInfo {
            /* +h0000 */ uint32_t agent_id;
            /* +h0004 */ uint32_t h0002;
            /* +h0008 */ uint32_t h0003;
            /* +h000C */ wchar_t* name_enc;
            /* +h0010 */ uint8_t h0010;
            /* +h0011 */ uint8_t h0012;
            /* +h0012 */ uint8_t h0013;
            /* +h0013 */ uint8_t background_alpha; // ARGB, NB: Actual color is ignored, only alpha is used
            /* +h0014 */ uint32_t text_color; // ARGB
            /* +h0014 */ uint32_t label_attributes; // bold/size etc
            /* +h001C */ uint8_t font_style; // Text style (bitmask) / bold | 0x1 / strikthrough | 0x80
            /* +h001D */ uint8_t underline; // Text underline (bool) = 0x01 - 0xFF
            /* +h001E */ uint8_t h001E;
            /* +h001F */ uint8_t h001F;
            /* +h0020 */ wchar_t* extra_info_enc; // Title etc
        };

        // Note: some windows are affected by UI scale (e.g. party members), others are not (e.g. compass)
        struct WindowPosition {
            uint32_t state; // & 0x1 == visible
            Vec2f p1;
            Vec2f p2;
            bool visible() const { return (state & 0x1) != 0; }
            // Returns vector of from X coord, to X coord.
            GWCA_API Vec2f xAxis(float multiplier = 1.f, bool clamp_position = true) const;
            // Returns vector of from Y coord, to Y coord.
            GWCA_API Vec2f yAxis(float multiplier = 1.f, bool clamp_position = true) const;
            float left(float multiplier = 1.f, bool clamp_position = true) const { return xAxis(multiplier, clamp_position).x; }
            float right(float multiplier = 1.f, bool clamp_position = true) const { return xAxis(multiplier, clamp_position).y; }
            float top(float multiplier = 1.f, bool clamp_position = true) const { return yAxis(multiplier, clamp_position).x; }
            float bottom(float multiplier = 1.f, bool clamp_position = true) const { return yAxis(multiplier, clamp_position).y; }
            float width(float multiplier = 1.f) const { return right(multiplier, false) - left(multiplier, false); }
            float height(float multiplier = 1.f) const { return bottom(multiplier, false) - top(multiplier, false); }
        };

        struct MapEntryMessage {
            wchar_t* title;
            wchar_t* subtitle;
        };

        struct DialogBodyInfo {
            uint32_t type;
            uint32_t agent_id;
            wchar_t* message_enc;
        };
        struct DialogButtonInfo {
            uint32_t button_icon; // byte
            wchar_t* message;
            uint32_t dialog_id;
            uint32_t skill_id; // Default 0xFFFFFFF
        };

        struct DecodingString {
            std::wstring encoded;
            std::wstring decoded;
            void* original_callback;
            void* original_param;
            void* ecx;
            void* edx;
        };

        enum class UIMessage : uint32_t {
            kNone = 0x0,
            kResize                     = 0x8,
            kInitFrame                  = 0x9,
            kDestroyFrame               = 0xb,
            kKeyDown                    = 0x1e, // wparam = UIPacket::kKeyAction*
            kKeyUp                      = 0x20, // wparam = UIPacket::kKeyAction*
            kMouseClick                 = 0x22, // wparam = UIPacket::kMouseClick*
            kMouseClick2                = 0x2e, // wparam = UIPacket::kMouseAction*
            kMouseAction                = 0x2f, // wparam = UIPacket::kMouseAction*
            kSetLayout                  = 0x33,
            kFrameMessage_0x47          = 0x47, // Multiple uses depending on frame
            kUpdateAgentEffects         = 0x10000000 | 0x9,
            kRerenderAgentModel         = 0x10000000 | 0x7, // wparam = uint32_t agent_id
            kAgentSpeechBubble          = 0x10000000 | 0x17, 
            kShowAgentNameTag           = 0x10000000 | 0x19, // wparam = AgentNameTagInfo*
            kHideAgentNameTag           = 0x10000000 | 0x1A,
            kSetAgentNameTagAttribs     = 0x10000000 | 0x1B, // wparam = AgentNameTagInfo*
            kChangeTarget               = 0x10000000 | 0x20, // wparam = UIPacket::kChangeTarget*
            kAgentStartCasting          = 0x10000000 | 0x27, // wparam = UIPacket::kAgentStartCasting*
            kShowMapEntryMessage        = 0x10000000 | 0x29, // wparam = { wchar_t* title, wchar_t* subtitle }
            kSetCurrentPlayerData       = 0x10000000 | 0x2A, // fired after setting the worldcontext player name
            kPostProcessingEffect       = 0x10000000 | 0x34, // Triggered when drunk. wparam = UIPacket::kPostProcessingEffect
            kHeroAgentAdded             = 0x10000000 | 0x38, // hero assigned to agent/inventory/ai mode
            kHeroDataAdded              = 0x10000000 | 0x39, // hero info received from server (name, level etc)
            kShowXunlaiChest            = 0x10000000 | 0x40,
            kMinionCountUpdated         = 0x10000000 | 0x46,
            kMoraleChange               = 0x10000000 | 0x47, // wparam = {agent id, morale percent }
            kLoginStateChanged          = 0x10000000 | 0x50, // wparam = {bool is_logged_in, bool unk }
            kEffectAdd                  = 0x10000000 | 0x55, // wparam = {agent_id, GW::Effect*}
            kEffectRenew                = 0x10000000 | 0x56, // wparam = GW::Effect*
            kEffectRemove               = 0x10000000 | 0x57, // wparam = effect id
            kSkillActivated             = 0x10000000 | 0x5b, // wparam ={ uint32_t agent_id , uint32_t skill_id }
            kUpdateSkillbar             = 0x10000000 | 0x5E, // wparam ={ uint32_t agent_id , ... }
            kUpdateSkillsAvailable      = 0x10000000 | 0x5f, // Triggered on a skill unlock, profession change or map load
            kTitleProgressUpdated       = 0x10000000 | 0x65, // wparam = title_id
            kExperienceGained           = 0x10000000 | 0x66, // wparam = experience amount
            kWriteToChatLog             = 0x10000000 | 0x7E, // wparam = UIPacket::kWriteToChatLog*. Triggered by the game when it wants to add a new message to chat.
            kWriteToChatLogWithSender   = 0x10000000 | 0x7f, // wparam = UIPacket::kWriteToChatLogWithSender*. Triggered by the game when it wants to add a new message to chat.
			kAllyOrGuildMessage         = 0x10000000 | 0x80, // wparam = UIPacket::kAllyOrGuildMessage*
            kPlayerChatMessage          = 0x10000000 | 0x81, // wparam = UIPacket::kPlayerChatMessage*
            kFloatingWindowMoved        = 0x10000000 | 0x83, // wparam = frame_id
            kFriendUpdated              = 0x10000000 | 0x89, // wparam = { GW::Friend*, ... }
            kMapLoaded                  = 0x10000000 | 0x8A,
            kOpenWhisper                = 0x10000000 | 0x90, // wparam = wchar* name
            kLogout                     = 0x10000000 | 0x9b, // wparam = { bool unknown, bool character_select }
            kCompassDraw                = 0x10000000 | 0x9c, // wparam = UIPacket::kCompassDraw*
            kOnScreenMessage            = 0x10000000 | 0xA0, // wparam = wchar_** encoded_string
            kDialogBody                 = 0x10000000 | 0xA4, // wparam = DialogBodyInfo*
            kDialogButton               = 0x10000000 | 0xA1, // wparam = DialogButtonInfo*
            kTargetNPCPartyMember       = 0x10000000 | 0xB1, // wparam = { uint32_t unk, uint32_t agent_id }
            kTargetPlayerPartyMember    = 0x10000000 | 0xB2, // wparam = { uint32_t unk, uint32_t player_number }
            kVendorWindow               = 0x10000000 | 0xB3, // wparam = UIPacket::kVendorWindow
            kVendorItems                = 0x10000000 | 0xB7, // wparam = UIPacket::kVendorItems
            kVendorTransComplete        = 0x10000000 | 0xB9, // wparam = *TransactionType
            kVendorQuote                = 0x10000000 | 0xBB, // wparam = UIPacket::kVendorQuote
            kStartMapLoad               = 0x10000000 | 0xC0, // wparam = { uint32_t map_id, ...}
            kWorldMapUpdated            = 0x10000000 | 0xC5, // Triggered when an area in the world map has been discovered/updated
            kGuildMemberUpdated         = 0x10000000 | 0xD8, // wparam = { GuildPlayer::name_ptr }
            kShowHint                   = 0x10000000 | 0xDF, // wparam = { uint32_t icon_type, wchar_t* message_enc }
            kWeaponSetSwapComplete      = 0x10000000 | 0xE7, // wparam = UIPacket::kWeaponSwap*
            kWeaponSetSwapCancel        = 0x10000000 | 0xE8, 
			kWeaponSetUpdated           = 0x10000000 | 0xE9,
            kUpdateGoldCharacter        = 0x10000000 | 0xEA, // wparam = { uint32_t unk, uint32_t gold_character }
            kUpdateGoldStorage          = 0x10000000 | 0xEB, // wparam = { uint32_t unk, uint32_t gold_storage }
            kInventorySlotUpdated       = 0x10000000 | 0xEC, // undocumented. Triggered when an item is moved into a slot
            kEquipmentSlotUpdated       = 0x10000000 | 0xED, // undocumented. Triggered when an item is moved into a slot
            kInventorySlotCleared       = 0x10000000 | 0xEF, // undocumented. Triggered when an item has been removed from a slot
            kEquipmentSlotCleared       = 0x10000000 | 0xF0, // undocumented. Triggered when an item has been removed from a slot
            kPvPWindowContent           = 0x10000000 | 0xF8,
            kPreStartSalvage            = 0x10000000 | 0x100, // { uint32_t item_id, uint32_t kit_id }
            kTradePlayerUpdated         = 0x10000000 | 0x103, // wparam = GW::TraderPlayer*
            kItemUpdated                = 0x10000000 | 0x104, // wparam = UIPacket::kItemUpdated*
            kMapChange                  = 0x10000000 | 0x10F, // wparam = map id
            kCalledTargetChange         = 0x10000000 | 0x113, // wparam = { player_number, target_id }
            kErrorMessage               = 0x10000000 | 0x117, // wparam = { int error_index, wchar_t* error_encoded_string }
            kPartyHardModeChanged       = 0x10000000 | 0x118, // wparam = { int is_hard_mode }
            kPartyAddHenchman           = 0x10000000 | 0x119,
            kPartyRemoveHenchman        = 0x10000000 | 0x11a,
            kPartyAddHero               = 0x10000000 | 0x11c,
            kPartyRemoveHero            = 0x10000000 | 0x11d,
            kPartyAddPlayer             = 0x10000000 | 0x122,
            kPartyRemovePlayer          = 0x10000000 | 0x124,
            kDisableEnterMissionBtn     = 0x10000000 | 0x128, // wparam = boolean (1 = disabled, 0 = enabled)
            kShowCancelEnterMissionBtn  = 0x10000000 | 0x12b,
            kPartyDefeated              = 0x10000000 | 0x12d,
            kPartySearchInviteReceived  = 0x10000000 | 0x135, // wparam = UIPacket::kPartySearchInviteReceived*
            kPartySearchInviteSent      = 0x10000000 | 0x137,
            kPartyShowConfirmDialog     = 0x10000000 | 0x138, // wparam = UIPacket::kPartyShowConfirmDialog
            kPreferenceEnumChanged      = 0x10000000 | 0x13E, // wparam = UiPacket::kPreferenceEnumChanged
            kPreferenceFlagChanged      = 0x10000000 | 0x13F, // wparam = UiPacket::kPreferenceFlagChanged
            kPreferenceValueChanged     = 0x10000000 | 0x140, // wparam = UiPacket::kPreferenceValueChanged
            kUIPositionChanged          = 0x10000000 | 0x141, // wparam = UIPacket::kUIPositionChanged
            kQuestAdded                 = 0x10000000 | 0x149, // wparam = { quest_id, ... }
            kQuestDetailsChanged        = 0x10000000 | 0x14A, // wparam = { quest_id, ... }
            kClientActiveQuestChanged   = 0x10000000 | 0x14C, // wparam = { quest_id, ... }. Triggered when the game requests the current quest to change
            kServerActiveQuestChanged   = 0x10000000 | 0x14E, // wparam = UIPacket::kServerActiveQuestChanged*. Triggered when the server requests the current quest to change
            kUnknownQuestRelated        = 0x10000000 | 0x14F, 
            kDungeonComplete            = 0x10000000 | 0x151, // undocumented 
            kMissionComplete            = 0x10000000 | 0x152, // undocumented
            kVanquishComplete           = 0x10000000 | 0x154, // undocumented
            kObjectiveAdd               = 0x10000000 | 0x155, // wparam = UIPacket::kObjectiveAdd*
            kObjectiveComplete          = 0x10000000 | 0x156, // wparam = UIPacket::kObjectiveComplete*
            kObjectiveUpdated           = 0x10000000 | 0x157, // wparam = UIPacket::kObjectiveUpdated*
            kTradeSessionStart          = 0x10000000 | 0x160, // wparam = { trade_state, player_number }
            kTradeSessionUpdated        = 0x10000000 | 0x166, // no args
            kTriggerLogoutPrompt        = 0x10000000 | 0x16C, // no args
			kToggleOptionsWindow        = 0x10000000 | 0x16D, // no args
            kCheckUIState               = 0x10000000 | 0x170, // Undocumented
            kRedrawItem                 = 0x10000000 | 0x172, // wparam = uint32_t item_id
            kCloseSettings              = 0x10000000 | 0x174, // Undocumented
            kChangeSettingsTab          = 0x10000000 | 0x175, // wparam = uint32_t is_interface_tab
            kGuildHall                  = 0x10000000 | 0x177, // wparam = gh key (uint32_t[4])
            kLeaveGuildHall             = 0x10000000 | 0x179,
            kTravel                     = 0x10000000 | 0x17A,
            kOpenWikiUrl                = 0x10000000 | 0x17B, // wparam = char* url
            kAppendMessageToChat        = 0x10000000 | 0x189, // wparam = wchar_t* message
            kHideHeroPanel              = 0x10000000 | 0x197, // wparam = hero_id
            kShowHeroPanel              = 0x10000000 | 0x198, // wparam = hero_id
            kGetInventoryAgentId        = 0x10000000 | 0x19c, // wparam = 0, lparam = uint32_t* agent_id_out. Used to fetch which agent is selected
            kEquipItem                  = 0x10000000 | 0x19d, // wparam = { item_id, agent_id }
            kMoveItem                   = 0x10000000 | 0x19e, // wparam = { item_id, to_bag, to_slot, bool prompt }
            kInitiateTrade              = 0x10000000 | 0x1A0,
			kInventoryAgentChanged      = 0x10000000 | 0x1b0, // Triggered when inventory needs updating due to agent change; no args
            kOpenTemplate               = 0x10000000 | 0x1B9, // wparam = GW::UI::ChatTemplate*

            // GWCA Client to Server commands. Only added the ones that are used for hooks, everything else goes straight into GW
            
            kSendLoadSkillTemplate      = 0x30000000 | 0x3,  // wparam = SkillbarMgr::SkillTemplate*
            kSendPingWeaponSet          = 0x30000000 | 0x4,  // wparam = UIPacket::kSendPingWeaponSet*
            kSendMoveItem               = 0x30000000 | 0x5,  // wparam = UIPacket::kSendMoveItem*
            kSendMerchantRequestQuote   = 0x30000000 | 0x6,  // wparam = UIPacket::kSendMerchantRequestQuote*
            kSendMerchantTransactItem   = 0x30000000 | 0x7,  // wparam = UIPacket::kSendMerchantTransactItem*
            kSendUseItem                = 0x30000000 | 0x8,  // wparam = UIPacket::kSendUseItem*
            kSendSetActiveQuest         = 0x30000000 | 0x9,  // wparam = uint32_t quest_id
            kSendAbandonQuest           = 0x30000000 | 0xA, // wparam = uint32_t quest_id
            kSendChangeTarget           = 0x30000000 | 0xB, // wparam = UIPacket::kSendChangeTarget* // e.g. tell the gw client to focus on a different target
            kSendCallTarget             = 0x30000000 | 0x13, // wparam = { uint32_t call_type, uint32_t agent_id } // also used to broadcast morale, death penalty, "I'm following X", etc
            kSendDialog                 = 0x30000000 | 0x16, // wparam = dialog_id // internal use


            kStartWhisper               = 0x30000000 | 0x17, // wparam = UIPacket::kStartWhisper*
            kGetSenderColor             = 0x30000000 | 0x18, // wparam = UIPacket::kGetColor* // Get chat sender color depending on channel, output object passed by reference
            kGetMessageColor            = 0x30000000 | 0x19, // wparam = UIPacket::kGetColor* // Get chat message color depending on channel, output object passed by reference
            kSendChatMessage            = 0x30000000 | 0x1B, // wparam = UIPacket::kSendChatMessage*
            kLogChatMessage             = 0x30000000 | 0x1D, // wparam = UIPacket::kLogChatMessage*. Triggered when a message wants to be added to the persistent chat log.
            kRecvWhisper                = 0x30000000 | 0x1E, // wparam = UIPacket::kRecvWhisper*
            kPrintChatMessage           = 0x30000000 | 0x1F, // wparam = UIPacket::kPrintChatMessage*. Triggered when a message wants to be added to the in-game chat window.
            kSendWorldAction            = 0x30000000 | 0x20, // wparam = UIPacket::kSendWorldAction*
            kSetRendererValue           = 0x30000000 | 0x21 // wparam = UIPacket::kSetRendererValue
        };
        enum class FlagPreference : uint32_t;
        enum class NumberPreference : uint32_t;
        enum class EnumPreference : uint32_t;



        namespace UIPacket {
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
            struct kResize {
                uint32_t h0000;
                float h0004[9];
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
            };
            struct kMouseAction {
                uint32_t frame_id;
                uint32_t child_offset_id;
                uint32_t current_state; // 0x5 = hovered, 0x6 = mouse down
                void* wparam = 0;
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
                uint32_t bag_id;
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

        enum class NumberCommandLineParameter : uint32_t {
            Unk1,
            Unk2,
            Unk3,
            FPS,
            Count
        };

        enum class EnumPreference : uint32_t {
            CharSortOrder,
            AntiAliasing, // multi sampling
            Reflections,
            ShaderQuality,
            ShadowQuality,
            TerrainQuality,
            InterfaceSize,
            FrameLimiter,
            Count = 0x8
        };
        enum class StringPreference : uint32_t {
            Unk1,
            Unk2,
            LastCharacterName,
            Count = 0x3
        };

        enum class NumberPreference : uint32_t {
            // number preferences
            AutoTournPartySort,
            ChatState, // 1 == showing chat window, 0 = not showing chat window
            ChatTab,
            DistrictLastVisitedLanguage,
            DistrictLastVisitedLanguage2,
            DistrictLastVisitedNonInternationalLanguage,
            DistrictLastVisitedNonInternationalLanguage2,
            DamageTextSize, // 0 to 100
            FullscreenGamma, // 0 to 100
            InventoryBag, // Selected bag in inventory window
            TextLanguage,
            AudioLanguage,
            ChatFilterLevel,
            RefreshRate,
            ScreenSizeX,
            ScreenSizeY,
            SkillListFilterRarity,
            SkillListSortMethod,
            SkillListViewMode,
            SoundQuality, // 0 to 100
            StorageBagPage,
            Territory,
            TextureQuality, // TextureLod
            UseBestTextureFiltering,
            EffectsVolume, // 0 to 100
            DialogVolume, // 0 to 100
            BackgroundVolume, // 0 to 100
            MusicVolume, // 0 to 100
            UIVolume, // 0 to 100
            Vote,
            WindowPosX,
            WindowPosY,
            WindowSizeX,
            WindowSizeY,
            SealedSeed, // Used in codex arena
            SealedCount, // Used in codex arena
            FieldOfView, // 0 to 100
            CameraRotationSpeed, // 0 to 100
            ScreenBorderless, // 0x1 = Windowed Borderless, 0x2 = Windowed Fullscreen
            MasterVolume, // 0 to 100
            ClockMode,
            Count = 0x29
        };
        enum class FlagPreference : uint32_t {
            // boolean preferences
            ChannelAlliance = 0x4,
            ChannelEmotes = 0x6,
            ChannelGuild,
            ChannelLocal,
            ChannelGroup,
            ChannelTrade,
            ShowTextInSkillFloaters = 0x11,
            ShowKRGBRatingsInGame,
            AutoHideUIOnLoginScreen = 0x14,
            DoubleClickToInteract,
            InvertMouseControlOfCamera,
            DisableMouseWalking,
            AutoCameraInObserveMode,
            AutoHideUIInObserveMode,
            RememberAccountName = 0x2D,
            IsWindowed,
            ShowSpendAttributesButton = 0x31, // The game uses this hacky method of showing the "spend attributes" button next to the exp bar.
            ConciseSkillDescriptions,
            DoNotShowSkillTipsOnEffectMonitor,
            DoNotShowSkillTipsOnSkillBars,
            MuteWhenGuildWarsIsInBackground = 0x37,
            AutoTargetFoes = 0x39,
            AutoTargetNPCs,
            AlwaysShowNearbyNamesPvP,
            FadeDistantNameTags,
            DoNotCloseWindowsOnEscape = 0x45,
            ShowMinimapOnWorldMap,
            WaitForVSync = 0x54,
            WhispersFromFriendsEtcOnly,
            ShowChatTimestamps,
            ShowCollapsedBags,
            ItemRarityBorder,
            AlwaysShowAllyNames,
            AlwaysShowFoeNames,
            LockCompassRotation = 0x5c,
            EnableGamepad = 0x5d,
            Count = 0x5e
        };
        // Used with GetWindowPosition
        enum WindowID : uint32_t {
            WindowID_Dialogue1 = 0x0,
            WindowID_Dialogue2 = 0x1,
            WindowID_MissionGoals = 0x2,
            WindowID_DropBundle = 0x3,
            WindowID_Chat = 0x4,
            WindowID_InGameClock = 0x6,
            WindowID_Compass = 0x7,
            WindowID_DamageMonitor = 0x8,
            WindowID_PerformanceMonitor = 0xB,
            WindowID_EffectsMonitor = 0xC,
            WindowID_Hints = 0xD,
            WindowID_MissionStatusAndScoreDisplay = 0xF,
            WindowID_Notifications = 0x11,
            WindowID_Skillbar = 0x14,
            WindowID_SkillMonitor = 0x15,
            WindowID_UpkeepMonitor = 0x17,
            WindowID_SkillWarmup = 0x18,
            WindowID_Menu = 0x1A,
            WindowID_EnergyBar = 0x1C,
            WindowID_ExperienceBar = 0x1D,
            WindowID_HealthBar = 0x1E,
            WindowID_TargetDisplay = 0x1F,
            WindowID_MissionProgress = 0xE,
            WindowID_TradeButton = 0x21,
            WindowID_WeaponBar = 0x22,
            WindowID_Hero1 = 0x33,
            WindowID_Hero2 = 0x34,
            WindowID_Hero3 = 0x35,
            WindowID_Hero = 0x36,
            WindowID_SkillsAndAttributes = 0x38,
            WindowID_Friends = 0x3A,
            WindowID_Guild = 0x3B,
            WindowID_Help = 0x3D,
            WindowID_Inventory = 0x3E,
            WindowID_VaultBox = 0x3F,
            WindowID_InventoryBags = 0x40,
            WindowID_MissionMap = 0x42,
            WindowID_Observe = 0x44,
            WindowID_Options = 0x45,
            WindowID_PartyWindow = 0x48, // NB: state flag is ignored for party window, but position is still good
            WindowID_PartySearch = 0x49,
            WindowID_QuestLog = 0x4F,
            WindowID_Merchant = 0x5C,
            WindowID_Hero4 = 0x5E,
            WindowID_Hero5 = 0x5F,
            WindowID_Hero6 = 0x60,
            WindowID_Hero7 = 0x61,
            WindowID_Count = 0x66
        };

        enum ControlAction : uint32_t {
            ControlAction_None = 0,
            ControlAction_Screenshot = 0xAE,
            // Panels
            ControlAction_CloseAllPanels = 0x85,
            ControlAction_ToggleInventoryWindow = 0x8B,
            ControlAction_OpenScoreChart = 0xBD,
            ControlAction_OpenTemplateManager = 0xD3,
            ControlAction_OpenSaveEquipmentTemplate = 0xD4,
            ControlAction_OpenSaveSkillTemplate = 0xD5,
            ControlAction_OpenParty = 0xBF,
            ControlAction_OpenGuild = 0xBA,
            ControlAction_OpenFriends = 0xB9,
            ControlAction_ToggleAllBags = 0xB8,
            ControlAction_OpenMissionMap = 0xB6,
            ControlAction_OpenBag2 = 0xB5,
            ControlAction_OpenBag1 = 0xB4,
            ControlAction_OpenBelt = 0xB3,
            ControlAction_OpenBackpack = 0xB2,
            ControlAction_OpenSkillsAndAttributes = 0x8F,
            ControlAction_OpenQuestLog = 0x8E,
            ControlAction_OpenWorldMap = 0x8C,
            ControlAction_OpenOptions = 0x8D,
            ControlAction_OpenHero = 0x8A,

            // Weapon sets
            ControlAction_CycleEquipment = 0x86,
            ControlAction_ActivateWeaponSet1 = 0x81,
            ControlAction_ActivateWeaponSet2,
            ControlAction_ActivateWeaponSet3,
            ControlAction_ActivateWeaponSet4,

            ControlAction_DropItem = 0xCD, // drops bundle item >> flags, ashes, etc

            // Chat
            ControlAction_CharReply = 0xBE,
            ControlAction_OpenChat = 0xA1,
            ControlAction_OpenAlliance = 0x88,

            ControlAction_ReverseCamera = 0x90,
            ControlAction_StrafeLeft = 0x91,
            ControlAction_StrafeRight = 0x92,
            ControlAction_TurnLeft = 0xA2,
            ControlAction_TurnRight = 0xA3,
            ControlAction_MoveBackward = 0xAC,
            ControlAction_MoveForward = 0xAD,
            ControlAction_CancelAction = 0xAF,
            ControlAction_Interact = 0x80,
            ControlAction_ReverseDirection = 0xB1,
            ControlAction_Autorun = 0xB7,
            ControlAction_Follow = 0xCC,

            // Targeting
            ControlAction_TargetPartyMember1 = 0x96,
            ControlAction_TargetPartyMember2,
            ControlAction_TargetPartyMember3,
            ControlAction_TargetPartyMember4,
            ControlAction_TargetPartyMember5,
            ControlAction_TargetPartyMember6,
            ControlAction_TargetPartyMember7,
            ControlAction_TargetPartyMember8,
            ControlAction_TargetPartyMember9 = 0xC6,
            ControlAction_TargetPartyMember10,
            ControlAction_TargetPartyMember11,
            ControlAction_TargetPartyMember12,

            ControlAction_TargetNearestItem = 0xC3,
            ControlAction_TargetNextItem = 0xC4,
            ControlAction_TargetPreviousItem = 0xC5,
            ControlAction_TargetPartyMemberNext = 0xCA,
            ControlAction_TargetPartyMemberPrevious = 0xCB,
            ControlAction_TargetAllyNearest = 0xBC,
            ControlAction_ClearTarget = 0xE3,
            ControlAction_TargetSelf = 0xA0, // also 0x96
            ControlAction_TargetPriorityTarget = 0x9F,
            ControlAction_TargetNearestEnemy = 0x93,
            ControlAction_TargetNextEnemy = 0x95,
            ControlAction_TargetPreviousEnemy = 0x9E,

            ControlAction_ShowOthers = 0x89,
            ControlAction_ShowTargets = 0x94,

            ControlAction_CameraZoomIn = 0xCE,
            ControlAction_CameraZoomOut = 0xCF,

            // Party/Hero commands
            ControlAction_ClearPartyCommands = 0xDB,
            ControlAction_CommandParty = 0xD6,
            ControlAction_CommandHero1,
            ControlAction_CommandHero2,
            ControlAction_CommandHero3,
            ControlAction_CommandHero4 = 0x102,
            ControlAction_CommandHero5,
            ControlAction_CommandHero6,
            ControlAction_CommandHero7,

            ControlAction_OpenHero1PetCommander = 0xE0,
            ControlAction_OpenHero2PetCommander,
            ControlAction_OpenHero3PetCommander,
            ControlAction_OpenHero4PetCommander = 0xFE,
            ControlAction_OpenHero5PetCommander,
            ControlAction_OpenHero6PetCommander,
            ControlAction_OpenHero7PetCommander,
            ControlAction_OpenHeroCommander1 = 0xDC,
            ControlAction_OpenHeroCommander2,
            ControlAction_OpenHeroCommander3,
            ControlAction_OpenPetCommander,
            ControlAction_OpenHeroCommander4 = 0x126,
            ControlAction_OpenHeroCommander5,
            ControlAction_OpenHeroCommander6,
            ControlAction_OpenHeroCommander7,

            ControlAction_Hero1Skill1 = 0xE5,
            ControlAction_Hero1Skill2,
            ControlAction_Hero1Skill3,
            ControlAction_Hero1Skill4,
            ControlAction_Hero1Skill5,
            ControlAction_Hero1Skill6,
            ControlAction_Hero1Skill7,
            ControlAction_Hero1Skill8,
            ControlAction_Hero2Skill1,
            ControlAction_Hero2Skill2,
            ControlAction_Hero2Skill3,
            ControlAction_Hero2Skill4,
            ControlAction_Hero2Skill5,
            ControlAction_Hero2Skill6,
            ControlAction_Hero2Skill7,
            ControlAction_Hero2Skill8,
            ControlAction_Hero3Skill1,
            ControlAction_Hero3Skill2,
            ControlAction_Hero3Skill3,
            ControlAction_Hero3Skill4,
            ControlAction_Hero3Skill5,
            ControlAction_Hero3Skill6,
            ControlAction_Hero3Skill7,
            ControlAction_Hero3Skill8,
            ControlAction_Hero4Skill1 = 0x106,
            ControlAction_Hero4Skill2,
            ControlAction_Hero4Skill3,
            ControlAction_Hero4Skill4,
            ControlAction_Hero4Skill5,
            ControlAction_Hero4Skill6,
            ControlAction_Hero4Skill7,
            ControlAction_Hero4Skill8,
            ControlAction_Hero5Skill1,
            ControlAction_Hero5Skill2,
            ControlAction_Hero5Skill3,
            ControlAction_Hero5Skill4,
            ControlAction_Hero5Skill5,
            ControlAction_Hero5Skill6,
            ControlAction_Hero5Skill7,
            ControlAction_Hero5Skill8,
            ControlAction_Hero6Skill1,
            ControlAction_Hero6Skill2,
            ControlAction_Hero6Skill3,
            ControlAction_Hero6Skill4,
            ControlAction_Hero6Skill5,
            ControlAction_Hero6Skill6,
            ControlAction_Hero6Skill7,
            ControlAction_Hero6Skill8,
            ControlAction_Hero7Skill1,
            ControlAction_Hero7Skill2,
            ControlAction_Hero7Skill3,
            ControlAction_Hero7Skill4,
            ControlAction_Hero7Skill5,
            ControlAction_Hero7Skill6,
            ControlAction_Hero7Skill7,
            ControlAction_Hero7Skill8,

            // Skills
            ControlAction_UseSkill1 = 0xA4,
            ControlAction_UseSkill2,
            ControlAction_UseSkill3,
            ControlAction_UseSkill4,
            ControlAction_UseSkill5,
            ControlAction_UseSkill6,
            ControlAction_UseSkill7,
            ControlAction_UseSkill8

        };
        struct FloatingWindow {
            void* unk1; // Some kind of function call
            wchar_t* name;
            uint32_t unk2;
            uint32_t unk3;
            uint32_t save_preference; // 1 or 0; if 1, will save to UI layout preferences.
            uint32_t unk4;
            uint32_t unk5;
            uint32_t unk6;
            uint32_t window_id; // Maps to window array
        };
        static_assert(sizeof(FloatingWindow) == 0x24);



        enum class TooltipType : uint32_t {
            None = 0x0,
            EncString1 = 0x4,
            EncString2 = 0x6,
            Item = 0x8,
            WeaponSet = 0xC,
            Skill = 0x14,
            Attribute = 0x4000
        };

        struct TooltipInfo {
            uint32_t bit_field;
            GW::UI::UIInteractionCallback* render; // Function that the game uses to draw the content
            uint32_t* payload; // uint32_t* for skill or item, wchar_t* for encoded string
            uint32_t payload_len; // Length in bytes of the payload
            uint32_t unk1;
            uint32_t unk2;
            uint32_t unk3;
            uint32_t unk4;
        };

        struct CreateUIComponentPacket {
            uint32_t frame_id;
            uint32_t component_flags;
            uint32_t tab_index;
            UI::UIInteractionCallback event_callback;
            void* wparam;
            wchar_t* component_label;
        };

        GWCA_API GW::Constants::Language GetTextLanguage();

        GWCA_API bool ButtonClick(Frame* btn_frame);

        GWCA_API uint32_t CreateUIComponent(uint32_t parent_frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, void* wparam, const wchar_t* component_label);

        GWCA_API bool DestroyUIComponent(Frame* frame);

        GWCA_API bool SelectDropdownOption(Frame* frame, uint32_t value);

        GWCA_API void* GetFrameContext(GW::UI::Frame* frame);

        GWCA_API Frame* GetRootFrame();

        GWCA_API Frame* GetChildFrame(Frame* parent, uint32_t child_offset);
        template<typename First, typename... Rest>
            requires (std::integral<First> && (std::integral<Rest> && ...))
        Frame* GetChildFrame(Frame* parent, First first, Rest... rest) {
            Frame* intermediate = GetChildFrame(parent, static_cast<uint32_t>(first));
            if constexpr (sizeof...(rest) > 0) {
                return GetChildFrame(intermediate, rest...);
            }
            else {
                return intermediate;
            }
        }

        GWCA_API bool SetFrameTitle(GW::UI::Frame*, const wchar_t* title);

        GWCA_API Frame* GetParentFrame(Frame* frame);
        GWCA_API Frame* GetFrameById(uint32_t frame_id);
        GWCA_API Frame* GetFrameByLabel(const wchar_t* frame_label);

        GWCA_API bool SendFrameUIMessage(UI::Frame* frame, UI::UIMessage message_id, void* wParam, void* lParam = nullptr);

        // SendMessage for Guild Wars UI messages, most UI interactions will use this. Returns true if not blocked
        GWCA_API bool SendUIMessage(UI::UIMessage msgid, void* wParam = nullptr, void* lParam = nullptr);

        GWCA_API bool Keydown(ControlAction key, Frame* target = nullptr);
        GWCA_API bool Keyup(ControlAction key, Frame* target = nullptr);
        GWCA_API bool Keypress(ControlAction key, Frame* target = nullptr);

        GWCA_API UI::WindowPosition* GetWindowPosition(UI::WindowID window_id);
        GWCA_API bool SetWindowVisible(UI::WindowID window_id, bool is_visible);
        GWCA_API bool SetWindowPosition(UI::WindowID window_id, UI::WindowPosition* info);

        GWCA_API bool DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint* pts);

        // Call from GameThread to be safe
        GWCA_API ArrayByte* GetSettings();

        GWCA_API bool GetIsUIDrawn();
        GWCA_API bool GetIsShiftScreenShot();
        GWCA_API bool GetIsWorldMapShowing();

        GWCA_API void AsyncDecodeStr(const wchar_t *enc_str, wchar_t *buffer, size_t size);
        GWCA_API void AsyncDecodeStr(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param = 0, GW::Constants::Language language_id = (GW::Constants::Language)0xff);

        GWCA_API bool IsValidEncStr(const wchar_t* enc_str);

        GWCA_API bool UInt32ToEncStr(uint32_t value, wchar_t *buffer, size_t count);
        GWCA_API uint32_t EncStrToUInt32(const wchar_t *enc_str);

        GWCA_API void SetOpenLinks(bool toggle);

        GWCA_API uint32_t GetPreference(EnumPreference pref);
        GWCA_API uint32_t GetPreferenceOptions(EnumPreference pref, uint32_t** options_out = 0);
        GWCA_API uint32_t GetPreference(NumberPreference pref);
        GWCA_API bool GetPreference(FlagPreference pref);
        GWCA_API wchar_t* GetPreference(StringPreference pref);
        GWCA_API bool SetPreference(EnumPreference pref, uint32_t value);
        GWCA_API bool SetPreference(NumberPreference pref, uint32_t value);
        GWCA_API bool SetPreference(FlagPreference pref, bool value);
        GWCA_API bool SetPreference(StringPreference pref, wchar_t* value);

        // Returns actual hard frame limit, factoring in vsync, monitor refresh rate and in-game preferences
        GWCA_API uint32_t GetFrameLimit();
        // Set a hard upper limit for frame rate. Actual limit may be lower (but not higher) depending on vsync/in-game preference
        GWCA_API bool SetFrameLimit(uint32_t value);

        //GWCA_API void SetPreference(Preference pref, uint32_t value);
        GWCA_API bool SetFrameVisible(UI::Frame* frame, bool flag);
        GWCA_API bool SetFrameDisabled(UI::Frame* frame, bool flag);

        GWCA_API bool AddFrameUIInteractionCallback(GW::UI::Frame*, UI::UIInteractionCallback callback, void* wparam);

        GWCA_API bool TriggerFrameRedraw(UI::Frame* frame);

        GWCA_API bool SetFramePosition(UI::Frame* frame, UI::FramePosition& position);

        typedef HookCallback<uint32_t> KeyCallback;
        // Listen for a gw hotkey press
        GWCA_API void RegisterKeydownCallback(
            HookEntry* entry,
            const KeyCallback& callback);
        GWCA_API void RemoveKeydownCallback(
            HookEntry* entry);
        // Listen for a gw hotkey release
        GWCA_API void RegisterKeyupCallback(
            HookEntry* entry,
            const KeyCallback& callback);
        GWCA_API void RemoveKeyupCallback(
            HookEntry* entry);

        typedef HookCallback<UIMessage, void *, void *> UIMessageCallback;

        // Add a listener for a broadcasted UI message. If blocked here, will not cascade to individual listening frames.
        GWCA_API void RegisterUIMessageCallback(
            HookEntry *entry,
            UIMessage message_id,
            const UIMessageCallback& callback,
            int altitude = -0x8000);

        GWCA_API void RemoveUIMessageCallback(
            HookEntry *entry, UIMessage message_id = UIMessage::kNone);

        typedef HookCallback<const Frame*, UIMessage, void *, void *> FrameUIMessageCallback;

        // Add a listener for every frame that receives a UI message. Triggered onces for every frame that is listening for this message id.
        GWCA_API void RegisterFrameUIMessageCallback(
            HookEntry *entry,
            UIMessage message_id,
            const FrameUIMessageCallback& callback,
            int altitude = -0x8000);

        GWCA_API void RemoveFrameUIMessageCallback(
            HookEntry *entry);



        GWCA_API TooltipInfo* GetCurrentTooltip();

        typedef std::function<void (CreateUIComponentPacket*)> CreateUIComponentCallback;
        GWCA_API void RegisterCreateUIComponentCallback(
            HookEntry *entry,
            const CreateUIComponentCallback& callback,
            int altitude = -0x8000);

        GWCA_API void RemoveCreateUIComponentCallback(
            HookEntry *entry);

    }
}
