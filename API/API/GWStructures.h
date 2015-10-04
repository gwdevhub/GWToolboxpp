#pragma once

#include "APIMain.h"
#include <Windows.h>

namespace GWAPI {

	namespace GW {

		struct Agent {
			DWORD* vtable;
			BYTE unknown1[24];
			BYTE unknown2[4]; //This actually points to the agent before but with a small offset
			Agent* NextAgent; //Pointer to the next agent (by id)
			BYTE unknown3[8];
			long Id; //AgentId
			float Z; //Z coord in float
			BYTE unknown4[8];
			float BoxHoverWidth; //Width of the model's box
			float BoxHoverHeight; //Height of the model's box
			BYTE unknown5[8];
			float Rotation; //Rotation in radians from East (-pi to pi)
			BYTE unknown6[8];
			long NameProperties; //Bitmap basically telling what the agent is
			BYTE unknown7[24];
			float X; //X coord in float
			float Y; //Y coord in float
			DWORD Ground;
			BYTE unknown8[4];
			float NameTagX; //Exactly the same as X above
			float NameTagY; //Exactly the same aswell
			float NameTagZ; //Z coord in float (actually negated)
			BYTE unknown9[12];
			long Type; //0xDB = players, npc's, monsters etc. 0x200 = signpost/chest/object (unclickable). 0x400 = item to pick up
			float MoveX; //If moving, how much on the X axis per second
			float MoveY; //If moving, how much on the Y axis per second
			//BYTE unknown10[68];
			BYTE unknown10[28];
			long Owner;
			BYTE unknown24[8];
			long ExtraType;
			BYTE unknown11[24];
			float WeaponAttackSpeed; //The base attack speed in float of last attacks weapon. 1.33 = axe, sWORD, daggers etc.
			float AttackSpeedModifier; //Attack speed modifier of the last attack. 0.67 = 33% increase (1-.33)
			WORD PlayerNumber; //Selfexplanatory. All non-players have identifiers for their type. Two of the same mob = same number
			BYTE unknown12[6];
			DWORD** Equip;
			BYTE unknown13[10];
			BYTE Primary; //Primary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
			BYTE Secondary; //Secondary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
			BYTE Level; //Duh!
			BYTE TeamId; //0=None, 1=Blue, 2=Red, 3=Yellow
			BYTE unknown14[14];
			float Energy; //Only works for yourself
			long MaxEnergy; //Only works for yourself
			BYTE unknown15[4];
			float HPPips; //Regen/degen as float
			BYTE unknown16[4];
			//Offset +0x130
			float HP; //Health in % where 1=100% and 0=0%
			long MaxHP; //Only works for yourself
			long Effects; //Bitmap for effects to display when targetted. DOES include hexes
			BYTE unknown17[4];
			BYTE Hex; //Bitmap for the hex effect when targetted (apparently obsolete!)
			BYTE unknown18[18];
			long ModelState; //Different values for different states of the model.
			long TypeMap; //Odd variable! 0x08 = dead, 0xC00 = boss, 0x40000 = spirit, 0x400000 = player
			BYTE unknown19[16];
			long InSpiritRange; //Tells if agent is within spirit range of you. Doesn't work anymore?
			BYTE unknown20[16];
			long LoginNumber; //Unique number in instance that only works for players
			float ModelMode; //Float for the current mode the agent is in. Varies a lot
			BYTE unknown21[4];
			long ModelAnimation; //Id of the current animation
			BYTE unknown22[32];
			WORD Allegiance; //0x100 = ally/non-attackable, 0x300 = enemy, 0x400 = spirit/pet, 0x500 = minion, 0x600 = npc/minipet
			WORD WeaponType; //1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff
			//Offset +0x1B4
			WORD Skill; //0 = not using a skill. Anything else is the Id of that skill
			BYTE unknown23[4];
			WORD WeaponItemId;
			WORD OffhandItemId;

			// Health Bar Effect Bitmasks.
			inline bool GetIsBleeding() { return (Effects & 1) != 0; }
			inline bool GetIsConditioned() { return (Effects & 2) != 0; }
			inline bool GetIsDead() { return (Effects & 16) != 0; }
			inline bool GetIsDeepWounded() { return (Effects & 32) != 0; }
			inline bool GetIsPoisoned() { return (Effects & 64) != 0; }
			inline bool GetIsEnchanted() { return (Effects & 128) != 0; }
			inline bool GetIsDegenHexed() { return (Effects & 1024) != 0; }
			inline bool GetIsHexed() { return (Effects & 2048) != 0; }
			inline bool GetIsWeaponSpelled() { return (Effects & 32768) != 0; }

			// Agent Type Bitmasks.
			inline bool GetIsLivingType() { return (Type & 0xDB) != 0; }
			inline bool GetIsSignpostType() { return (Type & 0x200) != 0; }
			inline bool GetIsItemType() { return (Type & 0x400) != 0; }

			// Agent TypeMap Bitmasks.
			inline bool GetInCombatStance() { return (TypeMap & 1) != 0; }
			inline bool GetHasQuest() { return (TypeMap & 2) != 0; }
			inline bool GetIsDeadByTypeMap() { return (TypeMap & 8) != 0; }
			inline bool GetIsFemale() { return (TypeMap & 512) != 0; }
			inline bool GetHasBossGlow() { return (TypeMap & 1024) != 0; }
			inline bool GetIsHidingCape() { return (TypeMap & 4096) != 0; }
			inline bool GetCanBeViewedInPartyWindow() { return (TypeMap & 131072) != 0; }
			inline bool GetIsSpawned() { return (TypeMap & 262144) != 0; }
			inline bool GetIsBeingObserved() { return (TypeMap & 4194304) != 0; }

			// Modelstates.
			inline bool GetIsKnockedDown() { return ModelState == 1104; }
			inline bool GetIsMoving() { return ModelState == 12 || ModelState == 76 || ModelState == 204; }
			inline bool GetIsAttacking() { return ModelState == 96 || ModelState == 1088 || ModelState == 1120; }
		};

		struct MapAgent{
			float curenergy; //?
			float maxenergy; //?
			float energyregen;
			DWORD skilltimestamp; //?
			float unk2;
			float MaxEnergy2; // again?
			float unk3;
			DWORD unk4;
			float curHealth;
			float maxHealth;
			float healthregen;
			DWORD unk5;
			DWORD Effects;

			// Health Bar Effect Bitmasks.
			inline bool GetIsBleeding() { return (Effects & 1) != 0; }
			inline bool GetIsConditioned() { return (Effects & 2) != 0; }
			inline bool GetIsDead() { return (Effects & 16) != 0; }
			inline bool GetIsDeepWounded() { return (Effects & 32) != 0; }
			inline bool GetIsPoisoned() { return (Effects & 64) != 0; }
			inline bool GetIsEnchanted() { return (Effects & 128) != 0; }
			inline bool GetIsDegenHexed() { return (Effects & 1024) != 0; }
			inline bool GetIsHexed() { return (Effects & 2048) != 0; }
			inline bool GetIsWeaponSpelled() { return (Effects & 32768) != 0; }
		};

		struct PartyMember{
			DWORD loginnumber;
			DWORD unk1;
			DWORD isLoaded;
		};


		typedef MemoryMgr::gw_array<MapAgent> MapAgentArray;
		typedef MemoryMgr::gw_array<PartyMember> PartyMemberArray;

		class AgentArray : public MemoryMgr::gw_array < Agent* > {
		public:
			Agent* GetPlayer() { return index(GetPlayerId()); }
			Agent* GetTarget() { return index(GetTargetId()); }
			inline DWORD GetPlayerId() { return *(DWORD*)MemoryMgr::PlayerAgentIDPtr; }
			inline DWORD GetTargetId() { return *(DWORD*)MemoryMgr::TargetAgentIDPtr; }
		};



		struct Bag;
		struct Item;

		typedef MemoryMgr::gw_array<Item*> ItemArray;

		struct Bag{							// total : 24 BYTEs
			BYTE unknown1[4];					// 0000	|--4 BYTEs--|
			long index;							// 0004
			DWORD BagId;						// 0008
			DWORD ContainerItem;				// 000C
			DWORD ItemsCount;					// 0010
			Bag* BagArray;						// 0014
			ItemArray Items;						// 0020
		};

		struct Item{							// total : 50 BYTEs
			DWORD ItemId;						// 0000
			DWORD AgentId;						// 0004
			BYTE unknown1[4];					// 0008	|--4 BYTEs--|
			Bag* bag;							// 000C
			DWORD* ModStruct;						// 0010						pointer to an array of mods
			DWORD ModStructSize;				// 0014						size of this array
			wchar_t* Customized;				// 0018
			BYTE unknown3[3];
			short type;
			short extraId;
			short value;
			BYTE unknown4[4];
			short interaction;
			long ModelId;
			BYTE* modString;
			BYTE unknown5[4];
			BYTE* extraItemInfo;
			BYTE unknown6[15];
			BYTE Quantity;
			BYTE equipped;
			BYTE profession;
			BYTE slot;						// 004F
		};


		struct Skill{							// total : A0 BYTEs
			DWORD SkillId;						// 0000
			BYTE Unknown1[4];					// 0004
			long Campaign;						// 0008	
			long Type;							// 000C
			DWORD Special;
			long ComboReq;						// 0014
			DWORD Effect1;
			DWORD Condition;
			DWORD Effect2;
			DWORD WeaponReq;
			BYTE Profession;					// 0028
			BYTE Attribute;						// 0029
			BYTE Unknown2[2];					// 002A
			long SkillId_PvP;					// 002C
			BYTE Combo;							// 0030
			BYTE Target;						// 0031
			BYTE unknown3;						// 0032
			BYTE SkillEquipType;				// 0033
			BYTE Unknown4;						// 0034
			BYTE EnergyCost;
			BYTE HealthCost;
			BYTE Unknown7;
			DWORD Adrenaline;					// 0038
			float Activation;					// 003C
			float Aftercast;					// 0040
			long Duration0;						// 0044
			long Duration15;					// 0048
			long Recharge;						// 004C
			BYTE Unknown5[12];					// 0050
			long Scale0;						// 005C
			long Scale15;						// 0060
			long BonusScale0;					// 0064
			long BonusScale15;					// 0068
			float AoERange;						// 006C
			float ConstEffect;					// 0070
			BYTE unknown6[44];					// 0074

			BYTE GetEnergyCost()
			{
				switch (EnergyCost){
				case 11: return 15;
				case 12: return 25;
				default: return EnergyCost;
				}
			}
		};

		struct Skillbar {						// total : BC BYTEs
			DWORD AgentId;						// 0000						id of the agent whose skillbar this is
			struct {
				long AdrenalineA;					// 0000					
				long AdrenalineB;					// 0004					
				DWORD Recharge;						// 0008					
				DWORD SkillId;						// 000C						see GWConst::SkillIds
				DWORD Event;						// 0010	s
			}Skills[8];			// 0004
			DWORD Disabled;
			BYTE unknown1[8];					// 00A8	|--8 BYTEs--|
			DWORD Casting;						// 00B0
			BYTE unknown2[8];					// 00B4	|--8 BYTEs--|
		};

		typedef MemoryMgr::gw_array<Skillbar> SkillbarArray;

		struct Effect {							// total : 18 BYTEs
			DWORD SkillId;						// 0000						skill id of the effect
			long EffectType;					// 0004						type classifier 0 = condition/shout, 8 = stance, 11 = maintained enchantment, 14 = enchantment/nature ritual
			DWORD EffectId;						// 0008						unique identifier of effect
			DWORD AgentId;						// 000C						non-zero means maintained enchantment - caster id
			float Duration;						// 0010						non-zero if effect has a duration
			DWORD TimeStamp;					// 0014						GW-timestamp of when effect was applied - only with duration

			long GetTimeElapsed() const { return MemoryMgr::GetSkillTimer() - TimeStamp; }
			long GetTimeRemaining() const { return (long)(Duration * 1000) - GetTimeElapsed(); }
			static Effect Nil() { return Effect{ 0, 0, 0, 0, 0, 0 }; }
		};

		struct Buff {							// total : 10 bytes
			DWORD SkillId;						// 0000						skill id of the buff
			DWORD Unknown1;						// 0004
			DWORD BuffId;						// 0008						id of buff in the buff array
			DWORD TargetAgentId;				// 000C						agent id of the target (0 if no target)
			static Buff Nil() { return Buff{ 0, 0, 0, 0 }; }
		};

		typedef MemoryMgr::gw_array<Effect> EffectArray;
		typedef MemoryMgr::gw_array<Buff> BuffArray;


		struct AgentEffects {
			DWORD AgentId;
			BuffArray Buffs;
			EffectArray Effects;
		};

		typedef MemoryMgr::gw_array<AgentEffects> AgentEffectsArray;


		struct GHKey{
			DWORD key[4];
		};

		struct Guild {
			GHKey GuildHallKey;

			inline wchar_t* GetGuildName(){ return (wchar_t*)(this + 0x30); }
			inline wchar_t* GetGuildTag(){ return (wchar_t*)(this + 0x80); }
		};

		typedef MemoryMgr::gw_array<Guild*> GuildArray;

		struct MissionMapIcon { // MapOverlay from GWCA
			long index;
			float X;
			float Y;
			long unknown1; // = 0
			long unknown2; // = 0
			long option; // Affilitation/color. Enum { 0 = gray, blue, red, yellow, teal, purple, green, gray };
			long unknown3; // = 0
			long modelId; // Model of the displayed icon in the Minimap
			long unknown4; // = 0
			void* unknown5; // May concern the name
		};

		typedef MemoryMgr::gw_array<MissionMapIcon> MissionMapIconArray;

	}
}