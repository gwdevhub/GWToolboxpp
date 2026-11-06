#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Macros.h>
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

	struct TextLabelFrame;
	struct MultiLineTextLabelFrame;
	struct ButtonFrame;

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

		typedef void(__cdecl* DecodeStr_Callback)(void* param, const wchar_t* s);

		struct ChatTemplate {
			uint32_t        agent_id;
			uint32_t        type; // 0 = build, 1 = equipment
			Array<wchar_t>  code;
			wchar_t* name;
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
			uint32_t child_offset_id;
			FrameRelation* parent_dupe;
			uint32_t frame_hash_id;
			TList<FrameRelation> children;
			TList<FrameRelation> siblings;
			Frame* GetFrame();
			Frame* GetParent() const;
		};

		static_assert(sizeof(FrameRelation) == 0x28);

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

			void GetTopLeftOnScreenNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			void GetBottomRightOnScreenNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			void GetContentTopLeftNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			void GetContentBottomRightNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			void GetSizeOnScreenNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			void GetViewportScaleNative(GW::Vec2f* result, const Frame* frame = nullptr) const;
			Vec2f GetTopLeftOnScreen(const Frame* frame = nullptr) const { Vec2f result; GetTopLeftOnScreenNative(&result, frame); return result; }
			Vec2f GetBottomRightOnScreen(const Frame* frame = nullptr) const { Vec2f result; GetBottomRightOnScreenNative(&result, frame); return result; }
			Vec2f GetContentTopLeft(const Frame* frame = nullptr) const { Vec2f result; GetContentTopLeftNative(&result, frame); return result; }
			Vec2f GetContentBottomRight(const Frame* frame = nullptr) const { Vec2f result; GetContentBottomRightNative(&result, frame); return result; }
			Vec2f GetSizeOnScreen(const Frame* frame = nullptr) const { Vec2f result; GetSizeOnScreenNative(&result, frame); return result; }
			Vec2f GetViewportScale(const Frame* frame = nullptr) const { Vec2f result; GetViewportScaleNative(&result, frame); return result; }
		};

		struct FrameInteractionCallback {
			UIInteractionCallback callback;
			void* uictl_context;
			uint32_t callback_state;
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
			uint32_t m_ctlSpec;
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
			bool IsBeingDestroyed() const {
				return (frame_state & 0x8) != 0;
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

		static_assert(GWCA_OFFSETOF(Frame, relation) == 0x128);

		struct AgentNameTagInfo {
			/* +h0000 */ uint32_t agent_id;
			/* +h0004 */ uint32_t h0004;
			/* +h0008 */ uint32_t h0008;
			/* +h000C */ wchar_t* name_enc;
			/* +h0010 */ uint8_t highlight;
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
			/* +h0024 */ uint32_t extra_info_color; // ARGB
			/* +h0028 */ uint32_t extra_info_attributes; // bold/size etc
		};

		// Note: some windows are affected by UI scale (e.g. party members), others are not (e.g. compass)
		struct WindowPosition {
			uint32_t state; // & 0x1 == visible
			Vec2f p1;
			Vec2f p2;
			bool visible() const { return (state & 0x1) != 0; }
			// Returns vector of from X coord, to X coord.
			void xAxisNative(Vec2f* result, float multiplier = 1.f, bool clamp_position = true) const;
			// Returns vector of from Y coord, to Y coord.
			void yAxisNative(Vec2f* result, float multiplier = 1.f, bool clamp_position = true) const;
			Vec2f xAxis(float multiplier = 1.f, bool clamp_position = true) const { Vec2f result; xAxisNative(&result, multiplier, clamp_position); return result; }
			Vec2f yAxis(float multiplier = 1.f, bool clamp_position = true) const { Vec2f result; yAxisNative(&result, multiplier, clamp_position); return result; }
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
			Count = 0x2b
		};
		enum class FlagPreference : uint32_t {
			FlagPref_0x0,
			FlagPref_0x1,
			FlagPref_0x2,
			FlagPref_0x3,
			// boolean preferences
			ChannelAlliance,
			FlagPref_0x5,
			ChannelEmotes,
			ChannelGuild,
			ChannelLocal,
			ChannelGroup,
			ChannelTrade,
			FlagPref_0xb,
			FlagPref_0xc,
			FlagPref_0xd,
			FlagPref_0xe,
			FlagPref_0xf,
			FlagPref_0x10,
			ShowTextInSkillFloaters,
			ShowKRGBRatingsInGame,
			FlagPref_0x13,
			AutoHideUIOnLoginScreen,
			DoubleClickToInteract,
			InvertMouseControlOfCamera,
			DisableMouseWalking,
			AutoCameraInObserveMode,
			AutoHideUIInObserveMode,
			FlagPref_0x1a,
			FlagPref_0x1b,
			FlagPref_0x1c,
			FlagPref_0x1d,
			FlagPref_0x1e,
			FlagPref_0x1f,
			FlagPref_0x20,
			FlagPref_0x21,
			FlagPref_0x22,
			FlagPref_0x23,
			FlagPref_0x24,
			FlagPref_0x25,
			FlagPref_0x26,
			FlagPref_0x27,
			FlagPref_0x28,
			FlagPref_0x29,
			FlagPref_0x2a,
			FlagPref_0x2b,
			FlagPref_0x2c,
			RememberAccountName,
			IsWindowed,
			FlagPref_0x2f,
			FlagPref_0x30,
			ShowSpendAttributesButton, // The game uses this hacky method of showing the "spend attributes" button next to the exp bar.
			ConciseSkillDescriptions,
			DoNotShowSkillTipsOnEffectMonitor,
			DoNotShowSkillTipsOnSkillBars,
			FlagPref_0x35,
			FlagPref_0x36,
			MuteWhenGuildWarsIsInBackground,
			FlagPref_0x38,
			AutoTargetFoes,
			AutoTargetNPCs,
			AlwaysShowNearbyNamesPvP,
			FadeDistantNameTags,
			FlagPref_0x3d,
			FlagPref_0x3e,
			FlagPref_0x3f,
			FlagPref_0x40,
			WaitForVSync,
			FlagPref_0x42,
			FlagPref_0x43,
			FlagPref_0x44,
			DoNotCloseWindowsOnEscape,
			ShowMinimapOnWorldMap,
			FlagPref_0x47,
			FlagPref_0x48,
			FlagPref_0x49,
			FlagPref_0x4a,
			FlagPref_0x4b,
			FlagPref_0x4c,
			FlagPref_0x4d,
			FlagPref_0x4e,
			FlagPref_0x4f,
			FlagPref_0x50,
			FlagPref_0x51,
			HighResolutionPlayerTextures,
			FlagPref_0x53,
			EnhancedDrawDistance,
			WhispersFromFriendsEtcOnly,
			ShowChatTimestamps,
			ShowCollapsedBags,
			ItemRarityBorder,
			AlwaysShowAllyNames,
			AlwaysShowFoeNames,
			FlagPref_0x5b,
			LockCompassRotation,
			EnableGamepad,
			DpiScaling,
			FlagPref_0x5f,
			FlagPref_0x60,
			HdSkillIcons,
			HdBloom,
			FlagPref_0x63,
			Ssao,
			FlagPref_0x65,
			FlagPref_0x66,
			FlagPref_0x67,
			FlagPref_0x68,
			FlagPref_0x69,
			FlagPref_0x6a,
			FlagPref_0x6b,
			FlagPref_0x6c,
			FlagPref_0x6d,
			FlagPref_0x6e,
			LegacyStartMissionButton,
			FlagPref_0x70,
			EnableMobileHUD,
			Count
		};
		static_assert(FlagPreference::Count == (FlagPreference)0x72);
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

		enum class UiProfileSetting : uint32_t {
			CameraFollowsPlayer = 0,
			GamepadAutoTargetSwitch = 1,
			MobileHUD = 2,
			AdSupported = 3,
			ForceAprilFools = 4,
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
		static_assert(sizeof(CreateUIComponentPacket) == 0x18);

		GW::Constants::Language GetTextLanguage();

		bool ButtonClick(Frame* btn_frame);

		GW::MultiLineTextLabelFrame* GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match = true);

		GW::ButtonFrame* GetButtonFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match = true);

		GW::TextLabelFrame* GetTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match = true);

		uint32_t CreateUIComponent(uint32_t parent_frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, void* wparam, const wchar_t* component_label);

		bool DestroyUIComponent(Frame* frame);

		// Bitfield accepted by Frame::SetBounds/SetPositionInternal
		enum FrameLayoutMode : uint32_t {
			FrameLayoutMode_None = 0,
			FrameLayoutMode_AnchorBottom = 0x1,           // vertical: anchor to the rect's bottom edge
			FrameLayoutMode_Center = 0x2,                // required alongside centered placement on either axis (asserts otherwise)
			FrameLayoutMode_AnchorLeft = 0x4,             // horizontal: anchor to the rect's left edge
			FrameLayoutMode_AnchorRight = 0x8,            // horizontal: anchor to the rect's right edge
			FrameLayoutMode_AnchorTop = 0x10,             // vertical: anchor to the rect's top edge
			FrameLayoutMode_StretchWidth = 0x20,          // horizontal: fill the rect's full width (also behaves as a left anchor)
			FrameLayoutMode_StretchHeight = 0x40,         // vertical: fill the rect's full height (also behaves as a bottom anchor)
			FrameLayoutMode_AnchorHorizontalMargin = 0x80,  // gate: trim the resolved rect inward on the AnchorLeft/AnchorRight edge(s)
			FrameLayoutMode_AnchorVerticalMargin = 0x100,   // gate: trim the resolved rect inward on the AnchorTop/AnchorBottom edge(s)
		};
		inline FrameLayoutMode operator|(FrameLayoutMode a, FrameLayoutMode b) {
			return static_cast<FrameLayoutMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		}
		inline FrameLayoutMode operator&(FrameLayoutMode a, FrameLayoutMode b) {
			return static_cast<FrameLayoutMode>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		}
		inline FrameLayoutMode operator~(FrameLayoutMode a) {
			return static_cast<FrameLayoutMode>(~static_cast<uint32_t>(a));
		}

		// Frame layout primitives (GW's Frame::SetBounds/SetPosition) for a container's kMeasureContent/kSetLayout handler.
		void SetFrameBounds(Frame* frame, FrameLayoutMode mode, float* rect, float* size_out);
		void SetFramePosition(Frame* frame, FrameLayoutMode mode, float* rect);

		// Appends an additional handler onto frame->frame_callbacks
		bool AddFrameCallback(Frame* frame, UIInteractionCallback callback, void* uictl_context = nullptr);

		bool SelectDropdownOption(Frame* frame, uint32_t value);

		void* GetFrameContext(GW::UI::Frame* frame);

		void* GetLastFrameContext(GW::UI::Frame* frame);

		Frame* GetRootFrame();

		Frame* GetChildFrame(Frame* parent, uint32_t child_offset);
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

		bool SetFrameTitle(GW::UI::Frame*, const wchar_t* title);

		Frame* GetParentFrame(Frame* frame);
		Frame* GetFrameById(uint32_t frame_id);
		Frame* GetFrameByLabel(const wchar_t* frame_label);
		bool Default_UICallback(InteractionMessage* interaction, void* wparam, void* lparam);

		bool SendFrameUIMessage(UI::Frame* frame, UI::UIMessage message_id, void* wParam, void* lParam = nullptr);

		// SendMessage for Guild Wars UI messages, most UI interactions will use this. Returns true if not blocked
		bool SendUIMessage(UI::UIMessage msgid, void* wParam = nullptr, void* lParam = nullptr);

		bool Keydown(ControlAction key, Frame* target = nullptr);
		bool Keyup(ControlAction key, Frame* target = nullptr);
		bool Keypress(ControlAction key, Frame* target = nullptr);

		UI::WindowPosition* GetWindowPosition(UI::WindowID window_id);
		bool SetWindowVisible(UI::WindowID window_id, bool is_visible);
		bool SetWindowPosition(UI::WindowID window_id, UI::WindowPosition* info);

		bool DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint* pts);

		// Call from GameThread to be safe
		ArrayByte* GetSettings();

		bool GetIsUIDrawn();
		bool GetIsShiftScreenShot();
		bool GetIsWorldMapShowing();

		void AsyncDecodeStrToBuffer(const wchar_t* enc_str, wchar_t* buffer, size_t size);
		void AsyncDecodeStrWithCallback(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param = 0, GW::Constants::Language language_id = (GW::Constants::Language)0xff);
        inline void AsyncDecodeStr(const wchar_t* text, wchar_t* buffer, size_t size) { AsyncDecodeStrToBuffer(text, buffer, size); }
        inline void AsyncDecodeStr(const wchar_t* text, DecodeStr_Callback callback, void* context = nullptr, GW::Constants::Language language = (GW::Constants::Language)0xff) { AsyncDecodeStrWithCallback(text, callback, context, language); }

		bool IsValidEncStr(const wchar_t* enc_str);

		bool UInt32ToEncStr(uint32_t value, wchar_t* buffer, size_t count);
		uint32_t EncStrToUInt32(const wchar_t* enc_str);

		void SetOpenLinks(bool toggle);

		uint32_t GetEnumPreference(EnumPreference pref);
		uint32_t GetPreferenceOptions(EnumPreference pref, uint32_t** options_out = 0);
		uint32_t GetNumberPreference(NumberPreference pref);
		bool GetFlagPreference(FlagPreference pref);
		wchar_t* GetStringPreference(StringPreference pref);
		bool SetEnumPreference(EnumPreference pref, uint32_t value);
		bool SetNumberPreference(NumberPreference pref, uint32_t value);
		bool SetFlagPreference(FlagPreference pref, bool value);
		bool SetStringPreference(StringPreference pref, wchar_t* value);
        inline uint32_t GetPreference(EnumPreference pref) { return GetEnumPreference(pref); }
        inline uint32_t GetPreference(NumberPreference pref) { return GetNumberPreference(pref); }
        inline bool GetPreference(FlagPreference pref) { return GetFlagPreference(pref); }
        inline wchar_t* GetPreference(StringPreference pref) { return GetStringPreference(pref); }
        inline bool SetPreference(EnumPreference pref, uint32_t value) { return SetEnumPreference(pref, value); }
        inline bool SetPreference(NumberPreference pref, uint32_t value) { return SetNumberPreference(pref, value); }
        inline bool SetPreference(FlagPreference pref, bool value) { return SetFlagPreference(pref, value); }
        inline bool SetPreference(StringPreference pref, wchar_t* value) { return SetStringPreference(pref, value); }

		bool GetCommandLinePrefString(const wchar_t* label, wchar_t** out);
		bool GetCommandLinePrefNumber(const wchar_t* label, uint32_t* out);
		bool SetCommandLinePrefString(const wchar_t* label, wchar_t* value);
		bool SetCommandLinePrefNumber(const wchar_t* label, uint32_t value);
        inline bool GetCommandLinePref(const wchar_t* label, wchar_t** out) { return GetCommandLinePrefString(label, out); }
        inline bool GetCommandLinePref(const wchar_t* label, uint32_t* out) { return GetCommandLinePrefNumber(label, out); }
        inline bool SetCommandLinePref(const wchar_t* label, wchar_t* value) { return SetCommandLinePrefString(label, value); }
        inline bool SetCommandLinePref(const wchar_t* label, uint32_t value) { return SetCommandLinePrefNumber(label, value); }

		//GWCA_API void SetPreference(Preference pref, uint32_t value);
		bool SetFrameVisible(UI::Frame* frame, bool flag);
		bool SetFrameDisabled(UI::Frame* frame, bool flag);

		// Stubbed: always returns false. The underlying game function is not scanned for.
		bool AddFrameUIInteractionCallback(GW::UI::Frame*, UI::UIInteractionCallback callback, void* wparam);

		bool TriggerFrameRedraw(UI::Frame* frame);

		// When the player is actively using a game controller
		bool IsInControllerMode();

		// When the GW interface is in mobile layout
		bool IsInMobileMode();

		// When the player is using a game controller and is in cursor mode
		bool IsInControllerCursorMode();

		using NativeKeyCallback = void (__cdecl *)(HookStatus* status, uint32_t key, void* context);
		bool RegisterKeydownCallback(HookEntry* entry, NativeKeyCallback callback, void* context = nullptr);
		void RemoveKeydownCallbackNative(
			HookEntry* entry);
		bool RegisterKeyupCallback(HookEntry* entry, NativeKeyCallback callback, void* context = nullptr);
		void RemoveKeyupCallbackNative(
			HookEntry* entry);

		using NativeUIMessageCallback = void (__cdecl *)(HookStatus* status, UIMessage message_id, void* wParam, void* lParam, void* context);

		// Add a listener for a broadcasted UI message. If blocked here, will not cascade to individual listening frames.
		bool RegisterUIMessageCallback(
			HookEntry* entry,
			UIMessage message_id,
			NativeUIMessageCallback callback,
			void* context = nullptr,
			int32_t altitude = -0x8000);

		void RemoveUIMessageCallbackNative(
			HookEntry* entry, UIMessage message_id = UIMessage::kNone);

		using NativeFrameUIMessageCallback = void (__cdecl *)(HookStatus* status, const Frame* frame, UIMessage message_id,
		                                               void* wParam, void* lParam, void* context);

		// Add a listener for every frame that receives a UI message. Triggered onces for every frame that is listening for this message id.
		bool RegisterFrameUIMessageCallback(
			HookEntry* entry,
			UIMessage message_id,
			NativeFrameUIMessageCallback callback,
			void* context = nullptr,
			int32_t altitude = -0x8000);

		bool RemoveFrameUIMessageCallbackNative(
			HookEntry* entry);



		TooltipInfo* GetCurrentTooltip();

		using NativeCreateUIComponentCallback = void (__cdecl *)(CreateUIComponentPacket* packet, void* context);
		bool RegisterCreateUIComponentCallback(HookEntry* entry, NativeCreateUIComponentCallback callback,
		                                               void* context = nullptr, int32_t altitude = -0x8000);

		bool RemoveCreateUIComponentCallbackNative(HookEntry* entry);
#if !defined(GWCA_STDLIB)
		inline void RemoveKeydownCallback(HookEntry* entry) { RemoveKeydownCallbackNative(entry); }
		inline void RemoveKeyupCallback(HookEntry* entry) { RemoveKeyupCallbackNative(entry); }
		inline void RemoveUIMessageCallback(HookEntry* entry, UIMessage message = UIMessage::kNone) { RemoveUIMessageCallbackNative(entry, message); }
		inline bool RemoveFrameUIMessageCallback(HookEntry* entry) { return RemoveFrameUIMessageCallbackNative(entry); }
		inline bool RemoveCreateUIComponentCallback(HookEntry* entry) { return RemoveCreateUIComponentCallbackNative(entry); }
#endif

	}
}

namespace GW::UI {
    extern "C" GWCA_API Frame* __cdecl GW_UI_FrameRelation_GetFrame(FrameRelation* self) noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_FrameRelation_GetParent(const FrameRelation* self) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetTopLeftOnScreenNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetBottomRightOnScreenNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetContentTopLeftNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetContentBottomRightNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetSizeOnScreenNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_FramePosition_GetViewportScaleNative(const FramePosition* self, GW::Vec2f* result, const Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_WindowPosition_xAxisNative(const WindowPosition* self, Vec2f* result, float multiplier, bool clamp_position) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_WindowPosition_yAxisNative(const WindowPosition* self, Vec2f* result, float multiplier, bool clamp_position) noexcept;
    extern "C" GWCA_API GW::Constants::Language __cdecl GW_UI_GetTextLanguage() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_ButtonClick(Frame* btn_frame) noexcept;
    extern "C" GWCA_API GW::MultiLineTextLabelFrame* __cdecl GW_UI_GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match) noexcept;
    extern "C" GWCA_API GW::ButtonFrame* __cdecl GW_UI_GetButtonFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match) noexcept;
    extern "C" GWCA_API GW::TextLabelFrame* __cdecl GW_UI_GetTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_UI_CreateUIComponent(uint32_t parent_frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, void* wparam, const wchar_t* component_label) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_DestroyUIComponent(Frame* frame) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_SetFrameBounds(Frame* frame, FrameLayoutMode mode, float* rect, float* size_out) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_SetFramePosition(Frame* frame, FrameLayoutMode mode, float* rect) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_AddFrameCallback(Frame* frame, UIInteractionCallback callback, void* uictl_context) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SelectDropdownOption(Frame* frame, uint32_t value) noexcept;
    extern "C" GWCA_API void* __cdecl GW_UI_GetFrameContext(GW::UI::Frame* frame) noexcept;
    extern "C" GWCA_API void* __cdecl GW_UI_GetLastFrameContext(GW::UI::Frame* frame) noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_GetRootFrame() noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_GetChildFrame(Frame* parent, uint32_t child_offset) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetFrameTitle(GW::UI::Frame* gwca_arg0, const wchar_t* title) noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_GetParentFrame(Frame* frame) noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_GetFrameById(uint32_t frame_id) noexcept;
    extern "C" GWCA_API Frame* __cdecl GW_UI_GetFrameByLabel(const wchar_t* frame_label) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_Default_UICallback(InteractionMessage* interaction, void* wparam, void* lparam) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SendFrameUIMessage(UI::Frame* frame, UI::UIMessage message_id, void* wParam, void* lParam) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SendUIMessage(UI::UIMessage msgid, void* wParam, void* lParam) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_Keydown(ControlAction key, Frame* target) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_Keyup(ControlAction key, Frame* target) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_Keypress(ControlAction key, Frame* target) noexcept;
    extern "C" GWCA_API UI::WindowPosition* __cdecl GW_UI_GetWindowPosition(UI::WindowID window_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetWindowVisible(UI::WindowID window_id, bool is_visible) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetWindowPosition(UI::WindowID window_id, UI::WindowPosition* info) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint* pts) noexcept;
    extern "C" GWCA_API ArrayByte* __cdecl GW_UI_GetSettings() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetIsUIDrawn() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetIsShiftScreenShot() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetIsWorldMapShowing() noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_AsyncDecodeStrToBuffer(const wchar_t* enc_str, wchar_t* buffer, size_t size) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_AsyncDecodeStrWithCallback(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param, GW::Constants::Language language_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_IsValidEncStr(const wchar_t* enc_str) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_UInt32ToEncStr(uint32_t value, wchar_t* buffer, size_t count) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_UI_EncStrToUInt32(const wchar_t* enc_str) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_SetOpenLinks(bool toggle) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_UI_GetEnumPreference(EnumPreference pref) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_UI_GetPreferenceOptions(EnumPreference pref, uint32_t** options_out) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_UI_GetNumberPreference(NumberPreference pref) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetFlagPreference(FlagPreference pref) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_UI_GetStringPreference(StringPreference pref) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetEnumPreference(EnumPreference pref, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetNumberPreference(NumberPreference pref, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetFlagPreference(FlagPreference pref, bool value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetStringPreference(StringPreference pref, wchar_t* value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetCommandLinePrefString(const wchar_t* label, wchar_t** out) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_GetCommandLinePrefNumber(const wchar_t* label, uint32_t* out) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetCommandLinePrefString(const wchar_t* label, wchar_t* value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetCommandLinePrefNumber(const wchar_t* label, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetFrameVisible(UI::Frame* frame, bool flag) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_SetFrameDisabled(UI::Frame* frame, bool flag) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_AddFrameUIInteractionCallback(GW::UI::Frame* gwca_arg0, UI::UIInteractionCallback callback, void* wparam) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_TriggerFrameRedraw(UI::Frame* frame) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_IsInControllerMode() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_IsInMobileMode() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_IsInControllerCursorMode() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RegisterKeydownCallback(HookEntry* entry, NativeKeyCallback callback, void* context) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_RemoveKeydownCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RegisterKeyupCallback(HookEntry* entry, NativeKeyCallback callback, void* context) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_RemoveKeyupCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RegisterUIMessageCallback(HookEntry* entry, UIMessage message_id, NativeUIMessageCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API void __cdecl GW_UI_RemoveUIMessageCallbackNative(HookEntry* entry, UIMessage message_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RegisterFrameUIMessageCallback(HookEntry* entry, UIMessage message_id, NativeFrameUIMessageCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RemoveFrameUIMessageCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API TooltipInfo* __cdecl GW_UI_GetCurrentTooltip() noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RegisterCreateUIComponentCallback(HookEntry* entry, NativeCreateUIComponentCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_UI_RemoveCreateUIComponentCallbackNative(HookEntry* entry) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::UI {
    inline Frame* FrameRelation::GetFrame()
    {
        return GWCA_CALL_C(GW_UI_FrameRelation_GetFrame, this);
    }

    inline Frame* FrameRelation::GetParent() const
    {
        return GWCA_CALL_C(GW_UI_FrameRelation_GetParent, this);
    }

    inline void FramePosition::GetTopLeftOnScreenNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetTopLeftOnScreenNative, this, result, frame);
    }

    inline void FramePosition::GetBottomRightOnScreenNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetBottomRightOnScreenNative, this, result, frame);
    }

    inline void FramePosition::GetContentTopLeftNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetContentTopLeftNative, this, result, frame);
    }

    inline void FramePosition::GetContentBottomRightNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetContentBottomRightNative, this, result, frame);
    }

    inline void FramePosition::GetSizeOnScreenNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetSizeOnScreenNative, this, result, frame);
    }

    inline void FramePosition::GetViewportScaleNative(GW::Vec2f* result, const Frame* frame) const
    {
        return GWCA_CALL_C(GW_UI_FramePosition_GetViewportScaleNative, this, result, frame);
    }

    inline void WindowPosition::xAxisNative(Vec2f* result, float multiplier, bool clamp_position) const
    {
        return GWCA_CALL_C(GW_UI_WindowPosition_xAxisNative, this, result, multiplier, clamp_position);
    }

    inline void WindowPosition::yAxisNative(Vec2f* result, float multiplier, bool clamp_position) const
    {
        return GWCA_CALL_C(GW_UI_WindowPosition_yAxisNative, this, result, multiplier, clamp_position);
    }

    inline GW::Constants::Language GetTextLanguage()
    {
        return GWCA_CALL_C(GW_UI_GetTextLanguage);
    }

    inline bool ButtonClick(Frame* btn_frame)
    {
        return GWCA_CALL_C(GW_UI_ButtonClick, btn_frame);
    }

    inline GW::MultiLineTextLabelFrame* GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        return GWCA_CALL_C(GW_UI_GetMultilineTextFrameByEncodedString, encoded_string, partial_match);
    }

    inline GW::ButtonFrame* GetButtonFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        return GWCA_CALL_C(GW_UI_GetButtonFrameByEncodedString, encoded_string, partial_match);
    }

    inline GW::TextLabelFrame* GetTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        return GWCA_CALL_C(GW_UI_GetTextFrameByEncodedString, encoded_string, partial_match);
    }

    inline uint32_t CreateUIComponent(uint32_t parent_frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, void* wparam, const wchar_t* component_label)
    {
        return GWCA_CALL_C(GW_UI_CreateUIComponent, parent_frame_id, component_flags, tab_index, event_callback, wparam, component_label);
    }

    inline bool DestroyUIComponent(Frame* frame)
    {
        return GWCA_CALL_C(GW_UI_DestroyUIComponent, frame);
    }

    inline void SetFrameBounds(Frame* frame, FrameLayoutMode mode, float* rect, float* size_out)
    {
        return GWCA_CALL_C(GW_UI_SetFrameBounds, frame, mode, rect, size_out);
    }

    inline void SetFramePosition(Frame* frame, FrameLayoutMode mode, float* rect)
    {
        return GWCA_CALL_C(GW_UI_SetFramePosition, frame, mode, rect);
    }

    inline bool AddFrameCallback(Frame* frame, UIInteractionCallback callback, void* uictl_context)
    {
        return GWCA_CALL_C(GW_UI_AddFrameCallback, frame, callback, uictl_context);
    }

    inline bool SelectDropdownOption(Frame* frame, uint32_t value)
    {
        return GWCA_CALL_C(GW_UI_SelectDropdownOption, frame, value);
    }

    inline void* GetFrameContext(GW::UI::Frame* frame)
    {
        return GWCA_CALL_C(GW_UI_GetFrameContext, frame);
    }

    inline void* GetLastFrameContext(GW::UI::Frame* frame)
    {
        return GWCA_CALL_C(GW_UI_GetLastFrameContext, frame);
    }

    inline Frame* GetRootFrame()
    {
        return GWCA_CALL_C(GW_UI_GetRootFrame);
    }

    inline Frame* GetChildFrame(Frame* parent, uint32_t child_offset)
    {
        return GWCA_CALL_C(GW_UI_GetChildFrame, parent, child_offset);
    }

    inline bool SetFrameTitle(GW::UI::Frame* gwca_arg0, const wchar_t* title)
    {
        return GWCA_CALL_C(GW_UI_SetFrameTitle, gwca_arg0, title);
    }

    inline Frame* GetParentFrame(Frame* frame)
    {
        return GWCA_CALL_C(GW_UI_GetParentFrame, frame);
    }

    inline Frame* GetFrameById(uint32_t frame_id)
    {
        return GWCA_CALL_C(GW_UI_GetFrameById, frame_id);
    }

    inline Frame* GetFrameByLabel(const wchar_t* frame_label)
    {
        return GWCA_CALL_C(GW_UI_GetFrameByLabel, frame_label);
    }

    inline bool Default_UICallback(InteractionMessage* interaction, void* wparam, void* lparam)
    {
        return GWCA_CALL_C(GW_UI_Default_UICallback, interaction, wparam, lparam);
    }

    inline bool SendFrameUIMessage(UI::Frame* frame, UI::UIMessage message_id, void* wParam, void* lParam)
    {
        return GWCA_CALL_C(GW_UI_SendFrameUIMessage, frame, message_id, wParam, lParam);
    }

    inline bool SendUIMessage(UI::UIMessage msgid, void* wParam, void* lParam)
    {
        return GWCA_CALL_C(GW_UI_SendUIMessage, msgid, wParam, lParam);
    }

    inline bool Keydown(ControlAction key, Frame* target)
    {
        return GWCA_CALL_C(GW_UI_Keydown, key, target);
    }

    inline bool Keyup(ControlAction key, Frame* target)
    {
        return GWCA_CALL_C(GW_UI_Keyup, key, target);
    }

    inline bool Keypress(ControlAction key, Frame* target)
    {
        return GWCA_CALL_C(GW_UI_Keypress, key, target);
    }

    inline UI::WindowPosition* GetWindowPosition(UI::WindowID window_id)
    {
        return GWCA_CALL_C(GW_UI_GetWindowPosition, window_id);
    }

    inline bool SetWindowVisible(UI::WindowID window_id, bool is_visible)
    {
        return GWCA_CALL_C(GW_UI_SetWindowVisible, window_id, is_visible);
    }

    inline bool SetWindowPosition(UI::WindowID window_id, UI::WindowPosition* info)
    {
        return GWCA_CALL_C(GW_UI_SetWindowPosition, window_id, info);
    }

    inline bool DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint* pts)
    {
        return GWCA_CALL_C(GW_UI_DrawOnCompass, session_id, pt_count, pts);
    }

    inline ArrayByte* GetSettings()
    {
        return GWCA_CALL_C(GW_UI_GetSettings);
    }

    inline bool GetIsUIDrawn()
    {
        return GWCA_CALL_C(GW_UI_GetIsUIDrawn);
    }

    inline bool GetIsShiftScreenShot()
    {
        return GWCA_CALL_C(GW_UI_GetIsShiftScreenShot);
    }

    inline bool GetIsWorldMapShowing()
    {
        return GWCA_CALL_C(GW_UI_GetIsWorldMapShowing);
    }

    inline void AsyncDecodeStrToBuffer(const wchar_t* enc_str, wchar_t* buffer, size_t size)
    {
        return GWCA_CALL_C(GW_UI_AsyncDecodeStrToBuffer, enc_str, buffer, size);
    }

    inline void AsyncDecodeStrWithCallback(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param, GW::Constants::Language language_id)
    {
        return GWCA_CALL_C(GW_UI_AsyncDecodeStrWithCallback, enc_str, callback, callback_param, language_id);
    }

    inline bool IsValidEncStr(const wchar_t* enc_str)
    {
        return GWCA_CALL_C(GW_UI_IsValidEncStr, enc_str);
    }

    inline bool UInt32ToEncStr(uint32_t value, wchar_t* buffer, size_t count)
    {
        return GWCA_CALL_C(GW_UI_UInt32ToEncStr, value, buffer, count);
    }

    inline uint32_t EncStrToUInt32(const wchar_t* enc_str)
    {
        return GWCA_CALL_C(GW_UI_EncStrToUInt32, enc_str);
    }

    inline void SetOpenLinks(bool toggle)
    {
        return GWCA_CALL_C(GW_UI_SetOpenLinks, toggle);
    }

    inline uint32_t GetEnumPreference(EnumPreference pref)
    {
        return GWCA_CALL_C(GW_UI_GetEnumPreference, pref);
    }

    inline uint32_t GetPreferenceOptions(EnumPreference pref, uint32_t** options_out)
    {
        return GWCA_CALL_C(GW_UI_GetPreferenceOptions, pref, options_out);
    }

    inline uint32_t GetNumberPreference(NumberPreference pref)
    {
        return GWCA_CALL_C(GW_UI_GetNumberPreference, pref);
    }

    inline bool GetFlagPreference(FlagPreference pref)
    {
        return GWCA_CALL_C(GW_UI_GetFlagPreference, pref);
    }

    inline wchar_t* GetStringPreference(StringPreference pref)
    {
        return GWCA_CALL_C(GW_UI_GetStringPreference, pref);
    }

    inline bool SetEnumPreference(EnumPreference pref, uint32_t value)
    {
        return GWCA_CALL_C(GW_UI_SetEnumPreference, pref, value);
    }

    inline bool SetNumberPreference(NumberPreference pref, uint32_t value)
    {
        return GWCA_CALL_C(GW_UI_SetNumberPreference, pref, value);
    }

    inline bool SetFlagPreference(FlagPreference pref, bool value)
    {
        return GWCA_CALL_C(GW_UI_SetFlagPreference, pref, value);
    }

    inline bool SetStringPreference(StringPreference pref, wchar_t* value)
    {
        return GWCA_CALL_C(GW_UI_SetStringPreference, pref, value);
    }

    inline bool GetCommandLinePrefString(const wchar_t* label, wchar_t** out)
    {
        return GWCA_CALL_C(GW_UI_GetCommandLinePrefString, label, out);
    }

    inline bool GetCommandLinePrefNumber(const wchar_t* label, uint32_t* out)
    {
        return GWCA_CALL_C(GW_UI_GetCommandLinePrefNumber, label, out);
    }

    inline bool SetCommandLinePrefString(const wchar_t* label, wchar_t* value)
    {
        return GWCA_CALL_C(GW_UI_SetCommandLinePrefString, label, value);
    }

    inline bool SetCommandLinePrefNumber(const wchar_t* label, uint32_t value)
    {
        return GWCA_CALL_C(GW_UI_SetCommandLinePrefNumber, label, value);
    }

    inline bool SetFrameVisible(UI::Frame* frame, bool flag)
    {
        return GWCA_CALL_C(GW_UI_SetFrameVisible, frame, flag);
    }

    inline bool SetFrameDisabled(UI::Frame* frame, bool flag)
    {
        return GWCA_CALL_C(GW_UI_SetFrameDisabled, frame, flag);
    }

    inline bool AddFrameUIInteractionCallback(GW::UI::Frame* gwca_arg0, UI::UIInteractionCallback callback, void* wparam)
    {
        return GWCA_CALL_C(GW_UI_AddFrameUIInteractionCallback, gwca_arg0, callback, wparam);
    }

    inline bool TriggerFrameRedraw(UI::Frame* frame)
    {
        return GWCA_CALL_C(GW_UI_TriggerFrameRedraw, frame);
    }

    inline bool IsInControllerMode()
    {
        return GWCA_CALL_C(GW_UI_IsInControllerMode);
    }

    inline bool IsInMobileMode()
    {
        return GWCA_CALL_C(GW_UI_IsInMobileMode);
    }

    inline bool IsInControllerCursorMode()
    {
        return GWCA_CALL_C(GW_UI_IsInControllerCursorMode);
    }

    inline bool RegisterKeydownCallback(HookEntry* entry, NativeKeyCallback callback, void* context)
    {
        return GWCA_CALL_C(GW_UI_RegisterKeydownCallback, entry, callback, context);
    }

    inline void RemoveKeydownCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_UI_RemoveKeydownCallbackNative, entry);
    }

    inline bool RegisterKeyupCallback(HookEntry* entry, NativeKeyCallback callback, void* context)
    {
        return GWCA_CALL_C(GW_UI_RegisterKeyupCallback, entry, callback, context);
    }

    inline void RemoveKeyupCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_UI_RemoveKeyupCallbackNative, entry);
    }

    inline bool RegisterUIMessageCallback(HookEntry* entry, UIMessage message_id, NativeUIMessageCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_UI_RegisterUIMessageCallback, entry, message_id, callback, context, altitude);
    }

    inline void RemoveUIMessageCallbackNative(HookEntry* entry, UIMessage message_id)
    {
        return GWCA_CALL_C(GW_UI_RemoveUIMessageCallbackNative, entry, message_id);
    }

    inline bool RegisterFrameUIMessageCallback(HookEntry* entry, UIMessage message_id, NativeFrameUIMessageCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_UI_RegisterFrameUIMessageCallback, entry, message_id, callback, context, altitude);
    }

    inline bool RemoveFrameUIMessageCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_UI_RemoveFrameUIMessageCallbackNative, entry);
    }

    inline TooltipInfo* GetCurrentTooltip()
    {
        return GWCA_CALL_C(GW_UI_GetCurrentTooltip);
    }

    inline bool RegisterCreateUIComponentCallback(HookEntry* entry, NativeCreateUIComponentCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_UI_RegisterCreateUIComponentCallback, entry, callback, context, altitude);
    }

    inline bool RemoveCreateUIComponentCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_UI_RemoveCreateUIComponentCallbackNative, entry);
    }
}
#endif
