#pragma once

#include "stl.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>

#include "ArmorsDatabase.h"

struct ComboListState;

enum class DyeColor
{
    None = 0,
    Blue = 2,
    Green = 3,
    Purple = 4,
    Red = 5,
    Yellow = 6,
    Brown = 7,
    Orange = 8,
    Silver = 9,
    Black = 10,
    Gray = 11,
    White = 12,
    Pink = 13
};


struct ComboListState {
    std::vector<Armor*> pieces;
    int current_piece_index = -1;
    Armor* current_piece = nullptr;
};

struct PlayerArmorPiece {
    uint32_t model_file_id;
    uint32_t unknow1;
    DyeColor color1;
    DyeColor color2;
    DyeColor color3;
    DyeColor color4;
};

struct PlayerArmor {
    PlayerArmorPiece head;
    PlayerArmorPiece chest;
    PlayerArmorPiece hands;
    PlayerArmorPiece legs;
    PlayerArmorPiece feets;
};

typedef void(__fastcall* SetItem_pt)(GW::Equipment* equip, void* edx, uint32_t model_file_id, uint32_t color, uint32_t arg3, uint32_t agent_id);
extern SetItem_pt SetItem_Func;
extern ComboListState head;
extern ComboListState chest;
extern ComboListState hands;
extern ComboListState legs;
extern ComboListState feets;


extern PlayerArmor player_armor;
extern GW::Constants::Profession current_profession;

const char* GetProfessionName(GW::Constants::Profession prof);
GW::Constants::Profession GetAgentProfession(GW::AgentLiving* agent);
void UpdateArmorsFilter(GW::Constants::Profession prof, Campaign campaign);
void InitItemPiece(PlayerArmorPiece* piece, const GW::Equipment::ItemData* item_data);
void SetArmorItem(const PlayerArmorPiece* piece);
bool DrawArmorPiece(const char* label, PlayerArmorPiece* player_piece, ComboListState* state);

bool armor_filter_array_getter(void* data, int idx, const char** out_text);
bool armor_pieces_array_getter(void* data, int idx, const char** out_text);

