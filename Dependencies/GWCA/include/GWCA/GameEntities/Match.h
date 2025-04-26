#pragma once

#include <cstdint>
#include <GWCA/GameEntities/Guild.h>

namespace GW {

    namespace Constants {
		enum class MapID : uint32_t;
    }

	enum class ObserverMatchType : uint32_t {
		SpecialEvent,
        HallOfHeroe,
        MyGuildsBattle,
        MyGuildsHeroesAscentGame,
        TopGuildBattle,
		TopGuildHeroesAscentGame,
        UploadedGame,
		Top1v1Battle,
        Top1v1TournamentBattle
	};

    struct ObserverMatchTeam {
        /* +h0000 */ uint32_t id;       // 0 - gray, 1 - blue, 2 - red
        /* +h0004 */ uint32_t h0004;    // indicates something with the tabard
        /* +h0008 */ uint32_t h0008;    // same.
        /* +h000C */ CapeDesign cape;   // aka. tabard in gw client
        /* +h0028 */ wchar_t* name;     // encoded name
    };
    static_assert(sizeof(ObserverMatchTeam) == 0x2C, "struct ObserverMatchTeam has incorrect size");

    struct ObserverMatch { // total: 0x4C/76
        /* +h0000 */ uint32_t match_id;
        /* +h0004 */ uint32_t match_id_dup;
        /* +h0008 */ GW::Constants::MapID map_id;
        /* +h000C */ uint32_t age;  // Elapsed time since match started [minutes]
        /* +h0010 */ ObserverMatchType type;
        /* +h0014 */ uint32_t match_title; // eg. 19 - "Monthly Championship Guild Tournament Finals", 18 - "Semifinals", 17 - "Quarterfinals", 16 - "Playoffs", 0 - No title
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ uint32_t count; // number of teams
        /* +h0020 */ ObserverMatchTeam team[2];
        /* +h0078 */ wchar_t* team1_name_dup;
        /* +h007C */ uint32_t h007C[10];
    };
    static_assert(sizeof(ObserverMatch) == 0xA4, "struct ObserverMatch has incorrect size");
}
