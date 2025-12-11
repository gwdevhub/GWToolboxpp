#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MerchantMgr.h>

#include <GWCA/Constants/UIMessages.h>

namespace GW {

    struct Module;
    extern Module UIModule;

    struct Effect;
    struct Vec2f;

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
        typedef void(__fastcall* UICtlCallback)(void* frame_context, void* wParam, void* lParam);

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
            uint32_t frame_layout;
            uint32_t field3_0xc;
            uint32_t field4_0x10;
            uint32_t field5_0x14;
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
            uint32_t field24a_0x64;
            uint32_t field24b_0x68;
            uint32_t field25_0x6c;
            uint32_t field26_0x70;
            uint32_t field27_0x74;
            uint32_t field28_0x78;
            uint32_t field29_0x7c;
            uint32_t field30_0x80;
            GW::Array<void*> field31_0x84;
            uint32_t field32_0x94;
            uint32_t field33_0x98;
            uint32_t field34_0x9c;
            uint32_t field35_0xa0;
            uint32_t field36_0xa4;
            GW::Array<FrameInteractionCallback> frame_callbacks;
            uint32_t child_offset_id; // Offset of this child in relation to its parent
            uint32_t frame_id; // Offset in the global frame array
            uint32_t field40_0xc0;
            uint32_t field41_0xc4;
            uint32_t field42_0xc8;
            uint32_t field43_0xcc;
            uint32_t field44_0xd0;
            uint32_t field45_0xd4;
            FramePosition position;
            uint32_t field63_0x11c;
            uint32_t field64_0x120;
            uint32_t field65_0x124;
            FrameRelation relation;
            uint32_t field73_0x144;
            uint32_t field74_0x148;
            uint32_t field75_0x14c;
            uint32_t field76_0x150;
            uint32_t field77_0x154;
            uint32_t field78_0x158;
            uint32_t field79_0x15c;
            uint32_t field80_0x160;
            uint32_t field81_0x164;
            uint32_t field82_0x168;
            uint32_t field83_0x16c;
            uint32_t field84_0x170;
            uint32_t field85_0x174;
            uint32_t field86_0x178;
            uint32_t field87_0x17c;
            uint32_t field88_0x180;
            uint32_t field89_0x184;
            uint32_t field90_0x188;
            uint32_t frame_state;
            uint32_t field92_0x190;
            uint32_t field93_0x194;
            uint32_t field94_0x198;
            uint32_t field95_0x19c;
            uint32_t field96_0x1a0;
            uint32_t field97_0x1a4;
            uint32_t field98_0x1a8;
            TooltipInfo* tooltip_info;
            uint32_t field100_0x1b0;
            uint32_t field101_0x1b4;
            uint32_t field102_0x1b8;
            uint32_t field103_0x1bc;
            uint32_t field104_0x1c0;
            uint32_t field105_0x1c4;

            bool IsCreated() const {
                return (frame_state & 0x4) != 0;
            }
            bool IsVisible() const {
                return !IsHidden();
            }
            bool IsHidden() const {
                return (frame_state & 0x200) != 0;
            }
            bool IsDisabled() const {
                return (frame_state & 0x10) != 0;
            }
        };
        static_assert(sizeof(Frame) == 0x1c8);

        static_assert(offsetof(Frame, relation) == 0x128);

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
        enum class FlagPreference : uint32_t;
        enum class NumberPreference : uint32_t;
        enum class EnumPreference : uint32_t;

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
            AutoTournPartySort,
            ChatState,
            ChatTab,
            DistrictLastVisitedLanguage,
            DistrictLastVisitedLanguage2,
            DistrictLastVisitedNonInternationalLanguage,
            DistrictLastVisitedNonInternationalLanguage2,
            FloaterScale,
            FullscreenGamma,
            InventoryBag,
            Language,
            LanguageAudio,
            LastDevice,
            Refresh,
            ScreenSizeX,
            ScreenSizeY,
            SkillListFilterRarity,
            SkillListSortMethod,
            SkillListViewMode,
            SoundQuality,
            StorageBagPage,
            Territory,
            TextureLod,
            TexFilterMode,
            VolBackground,
            VolDialog,
            VolEffect,
            VolMusic,
            VolUi,
            Vote,
            WindowPosX,
            WindowPosY,
            WindowSizeX,
            WindowSizeY,
            SealedSeed,
            SealedCount,
            CameraFov,
            CameraRotationSpeed,
            ScreenBorderless,
            VolMaster,
            ClockMode,
            MobileUiScale,
            GamepadCursorSpeed,
            LastLoginMethod,
            Count = 44
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
            WaitForVSync = 0x41,
            DoNotCloseWindowsOnEscape = 0x45,
            ShowMinimapOnWorldMap,
            WhispersFromFriendsEtcOnly = 0x55,
            ShowChatTimestamps,
            ShowCollapsedBags,
            ItemRarityBorder,
            AlwaysShowAllyNames,
            AlwaysShowFoeNames,
            LockCompassRotation = 0x5c,
            EnableGamepad = 0x5d,
            
            Count = 0x6c
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
            WindowID_Count = 0x69
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
            ControlAction_ChatReply = 0xBE,
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
            ControlAction_UseSkill8,

            ControlAction_ToggleGamepadCursorMode = 0x13d, // right dpad

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
        GWCA_API bool Default_UICallback(InteractionMessage* interaction, void* wparam, void* lparam);

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

        GWCA_API bool GetCommandLinePref(const wchar_t* label, wchar_t** out);
        GWCA_API bool GetCommandLinePref(const wchar_t* label, uint32_t* out);
        GWCA_API bool SetCommandLinePref(const wchar_t* label, wchar_t* value);
        GWCA_API bool SetCommandLinePref(const wchar_t* label, uint32_t value);

        //GWCA_API void SetPreference(Preference pref, uint32_t value);
        GWCA_API bool SetFrameVisible(UI::Frame* frame, bool flag);
        GWCA_API bool SetFrameDisabled(UI::Frame* frame, bool flag);

        GWCA_API bool AddFrameUIInteractionCallback(GW::UI::Frame*, UI::UIInteractionCallback callback, void* wparam);

        GWCA_API bool TriggerFrameRedraw(UI::Frame* frame);

        GWCA_API bool SetFramePosition(UI::Frame* frame, UI::FramePosition& position);

        // When the player is actively using a game controller
        GWCA_API bool IsInControllerMode();

        // When the player is using a game controller and is in cursor mode
        GWCA_API bool IsInControllerCursorMode();

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
