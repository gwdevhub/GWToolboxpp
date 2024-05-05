#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <ctime>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/Resources.h>
#include <Modules/GameSettings.h>
#include <Modules/PriceCheckerModule.h>

#include <Timer.h>
#include <Utils/GuiUtils.h>

using nlohmann::json;

namespace {
    bool fetch_module_prices = true;
    float high_price_threshold = 1000;
    std::wstring modified_description;
    bool fetching_prices;
    const char* trader_quotes_url = "https://kamadan.gwtoolbox.com/trader_quotes";
    json price_json = nullptr;
    clock_t last_request_time = 0;
    std::unordered_map<uint32_t, const char*> mod_to_name =
    {
        {0x240801F9, "Knight's Insignia"},
        {0x24080208, "Lieutenant's Insignia"},
        {0x24080209, "Stonefist Insignia"},
        {0x240801FA, "Dreadnought Insignia"},
        {0x240801FB, "Sentinel's Insignia"},
        {0x240800FC, "Rune of Minor Absorption"},
        {0x21E81501, "Rune of Minor Tactics"},
        {0x21E81101, "Rune of Minor Strength"},
        {0x21E81201, "Rune of Minor Axe Mastery"},
        {0x21E81301, "Rune of Minor Hammer Mastery"},
        {0x21E81401, "Rune of Minor Swordsmanship"},
        {0x240800FD, "Rune of Major Absorption"},
        {0x21E81502, "Rune of Major Tactics"},
        {0x21E81102, "Rune of Major Strength"},
        {0x21E81202, "Rune of Major Axe Mastery"},
        {0x21E81302, "Rune of Major Hammer Mastery"},
        {0x21E81402, "Rune of Major Swordsmanship"},
        {0x240800FE, "Rune of Superior Absorption"},
        {0x21E81503, "Rune of Superior Tactics"},
        {0x21E81103, "Rune of Superior Strength"},
        {0x21E81203, "Rune of Superior Axe Mastery"},
        {0x21E81303, "Rune of Superior Hammer Mastery"},
        {0x21E81403, "Rune of Superior Swordsmanship"},
        {0x240801FC, "Frostbound Insignia"},
        {0x240801FE, "Pyrebound Insignia"},
        {0x240801FF, "Stormbound Insignia"},
        {0x24080201, "Scout's Insignia"},
        {0x240801FD, "Earthbound Insignia"},
        {0x24080200, "Beastmaster's Insignia"},
        {0x21E81801, "Rune of Minor Wilderness Survival"},
        {0x21E81701, "Rune of Minor Expertise"},
        {0x21E81601, "Rune of Minor Beast Mastery"},
        {0x21E81901, "Rune of Minor Marksmanship"},
        {0x21E81802, "Rune of Major Wilderness Survival"},
        {0x21E81702, "Rune of Major Expertise"},
        {0x21E81602, "Rune of Major Beast Mastery"},
        {0x21E81902, "Rune of Major Marksmanship"},
        {0x21E81803, "Rune of Superior Wilderness Survival"},
        {0x21E81703, "Rune of Superior Expertise"},
        {0x21E81603, "Rune of Superior Beast Mastery"},
        {0x21E81903, "Rune of Superior Marksmanship"},
        {0x240801F6, "Wanderer's Insignia"},
        {0x240801F7, "Disciple's Insignia"},
        {0x240801F8, "Anchorite's Insignia"},
        {0x21E80D01, "Rune of Minor Healing Prayers"},
        {0x21E80E01, "Rune of Minor Smiting Prayers"},
        {0x21E80F01, "Rune of Minor Protection Prayers"},
        {0x21E81001, "Rune of Minor Divine Favor"},
        {0x21E80D02, "Rune of Major Healing Prayers"},
        {0x21E80E02, "Rune of Major Smiting Prayers"},
        {0x21E80F02, "Rune of Major Protection Prayers"},
        {0x21E81002, "Rune of Major Divine Favor"},
        {0x21E80D03, "Rune of Superior Healing Prayers"},
        {0x21E80E03, "Rune of Superior Smiting Prayers"},
        {0x21E80F03, "Rune of Superior Protection Prayers"},
        {0x21E81003, "Rune of Superior Divine Favor"},
        {0x2408020A, "Bloodstained Insignia"},
        {0x240801EC, "Tormentor's Insignia"},
        {0x240801EE, "Bonelace Insignia"},
        {0x240801EF, "Minion Master's Insignia"},
        {0x240801F0, "Blighter's Insignia"},
        {0x240801ED, "Undertaker's Insignia"},
        {0x21E80401, "Rune of Minor Blood Magic"},
        {0x21E80501, "Rune of Minor Death Magic"},
        {0x21E80701, "Rune of Minor Curses"},
        {0x21E80601, "Rune of Minor Soul Reaping"},
        {0x21E80402, "Rune of Major Blood Magic"},
        {0x21E80502, "Rune of Major Death Magic"},
        {0x21E80702, "Rune of Major Curses"},
        {0x21E80602, "Rune of Major Soul Reaping"},
        {0x21E80403, "Rune of Superior Blood Magic"},
        {0x21E80503, "Rune of Superior Death Magic"},
        {0x21E80703, "Rune of Superior Curses"},
        {0x21E80603, "Rune of Superior Soul Reaping"},
        {0x240801E4, "Virtuoso's Insignia"},
        {0x240801E2, "Artificer's Insignia"},
        {0x240801E3, "Prodigy's Insignia"},
        {0x21E80001, "Rune of Minor Fast Casting"},
        {0x21E80201, "Rune of Minor Domination Magic"},
        {0x21E80101, "Rune of Minor Illusion Magic"},
        {0x21E80301, "Rune of Minor Inspiration Magic"},
        {0x21E80002, "Rune of Major Fast Casting"},
        {0x21E80202, "Rune of Major Domination Magic"},
        {0x21E80102, "Rune of Major Illusion Magic"},
        {0x21E80302, "Rune of Major Inspiration Magic"},
        {0x21E80003, "Rune of Superior Fast Casting"},
        {0x21E80203, "Rune of Superior Domination Magic"},
        {0x21E80103, "Rune of Superior Illusion Magic"},
        {0x21E80303, "Rune of Superior Inspiration Magic"},
        {0x240801F2, "Hydromancer Insignia"},
        {0x240801F3, "Geomancer Insignia"},
        {0x240801F4, "Pyromancer Insignia"},
        {0x240801F5, "Aeromancer Insignia"},
        {0x240801F1, "Prismatic Insignia"},
        {0x21E80C01, "Rune of Minor Energy Storage"},
        {0x21E80A01, "Rune of Minor Fire Magic"},
        {0x21E80801, "Rune of Minor Air Magic"},
        {0x21E80901, "Rune of Minor Earth Magic"},
        {0x21E80B01, "Rune of Minor Water Magic"},
        {0x21E80C02, "Rune of Major Energy Storage"},
        {0x21E80A02, "Rune of Major Fire Magic"},
        {0x21E80802, "Rune of Major Air Magic"},
        {0x21E80902, "Rune of Major Earth Magic"},
        {0x21E80B02, "Rune of Major Water Magic"},
        {0x21E80C03, "Rune of Superior Energy Storage"},
        {0x21E80A03, "Rune of Superior Fire Magic"},
        {0x21E80803, "Rune of Superior Air Magic"},
        {0x21E80903, "Rune of Superior Earth Magic"},
        {0x21E80B03, "Rune of Superior Water Magic"},
        {0x240801DE, "Vanguard's Insignia"},
        {0x240801DF, "Infiltrator's Insignia"},
        {0x240801E0, "Saboteur's Insignia"},
        {0x240801E1, "Nightstalker's Insignia"},
        {0x21E82301, "Rune of Minor Critical Strikes"},
        {0x21E81D01, "Rune of Minor Dagger Mastery"},
        {0x21E81E01, "Rune of Minor Deadly Arts"},
        {0x21E81F01, "Rune of Minor Shadow Arts"},
        {0x21E82302, "Rune of Major Critical Strikes"},
        {0x21E81D02, "Rune of Major Dagger Mastery"},
        {0x21E81E02, "Rune of Major Deadly Arts"},
        {0x21E81F02, "Rune of Major Shadow Arts"},
        {0x21E82303, "Rune of Superior Critical Strikes"},
        {0x21E81D03, "Rune of Superior Dagger Mastery"},
        {0x21E81E03, "Rune of Superior Deadly Arts"},
        {0x21E81F03, "Rune of Superior Shadow Arts"},
        {0x24080204, "Shaman's Insignia"},
        {0x24080205, "Ghost Forge Insignia"},
        {0x24080206, "Mystic's Insignia"},
        {0x21E82201, "Rune of Minor Channeling Magic"},
        {0x21E82101, "Rune of Minor Restoration Magic"},
        {0x21E82001, "Rune of Minor Communing"},
        {0x21E82401, "Rune of Minor Spawning Power"},
        {0x21E82202, "Rune of Major Channeling Magic"},
        {0x21E82102, "Rune of Major Restoration Magic"},
        {0x21E82002, "Rune of Major Communing"},
        {0x21E82402, "Rune of Major Spawning Power"},
        {0x21E82203, "Rune of Superior Channeling Magic"},
        {0x21E82103, "Rune of Superior Restoration Magic"},
        {0x21E82003, "Rune of Superior Communing"},
        {0x21E82403, "Rune of Superior Spawning Power"},
        {0x24080202, "Windwalker Insignia"},
        {0x24080203, "Forsaken Insignia"},
        {0x21E82C01, "Rune of Minor Mysticism"},
        {0x21E82B01, "Rune of Minor Earth Prayers"},
        {0x21E82901, "Rune of Minor Scythe Mastery"},
        {0x21E82A01, "Rune of Minor Wind Prayers"},
        {0x21E82C02, "Rune of Major Mysticism"},
        {0x21E82B02, "Rune of Major Earth Prayers"},
        {0x21E82902, "Rune of Major Scythe Mastery"},
        {0x21E82A02, "Rune of Major Wind Prayers"},
        {0x21E82C03, "Rune of Superior Mysticism"},
        {0x21E82B03, "Rune of Superior Earth Prayers"},
        {0x21E82903, "Rune of Superior Scythe Mastery"},
        {0x21E82A03, "Rune of Superior Wind Prayers"},
        {0x24080207, "Centurion's Insignia"},
        {0x21E82801, "Rune of Minor Leadership"},
        {0x21E82701, "Rune of Minor Motivation"},
        {0x21E82601, "Rune of Minor Command"},
        {0x21E82501, "Rune of Minor Spear Mastery"},
        {0x21E82802, "Rune of Major Leadership"},
        {0x21E82702, "Rune of Major Motivation"},
        {0x21E82602, "Rune of Major Command"},
        {0x21E82502, "Rune of Major Spear Mastery"},
        {0x21E82803, "Rune of Superior Leadership"},
        {0x21E82703, "Rune of Superior Motivation"},
        {0x21E82603, "Rune of Superior Command"},
        {0x21E82503, "Rune of Superior Spear Mastery"},
        {0x240801E6, "Survivor Insignia"},
        {0x240801E5, "Radiant Insignia"},
        {0x240801E7, "Stalwart Insignia"},
        {0x240801E8, "Brawler's Insignia"},
        {0x240801E9, "Blessed Insignia"},
        {0x240801EA, "Herald's Insignia"},
        {0x240801EB, "Sentry's Insignia"},
        {0x24080211, "Rune of Attunement"},
        {0x24080213, "Rune of Recovery"},
        {0x24080214, "Rune of Restoration"},
        {0x24080215, "Rune of Clarity"},
        {0x24080216, "Rune of Purity"},
        {0x240800FF, "Rune of Minor Vigor"},
        {0x240800C2, "Rune of Minor Vigor"}, //Idk why but it appears like this sometimes
        {0x24080101, "Rune of Superior Vigor"},
        {0x24080100, "Rune of Major Vigor"},
        {0x24080212, "Rune of Vitae"}
    };
    std::unordered_map<uint32_t, const char*> mod_to_id =
    {
        {0x240801F9, "19152-25B80000240801F9A53003F2A7F80300"},                 // Knight's Insignia
        {0x24080208, "19153-25B80000240802082530041027E802B6A5300410A0FBEC00"}, // Lieutenant's Insignia
        {0x24080209, "19154-25B80000240802092530041227E802B7"},                 // Stonefist Insignia
        {0x240801FA, "19155-25B80000240801FAA53003F4A128000A"},                 // Dreadnought Insignia
        {0x240801FB, "19156-25B80000240801FB807003F68010110DA53003F6A1280014"}, // Sentinel's Insignia
        {0x240800FC, "903-25B80000240800FC253001F927E802EA"},                   // Rune of Minor Absorption
        {0x21E81501, "903-25B80000240800B32530016721E81501"},                   // Rune of Minor Tactics
        {0x21E81101, "903-25B80000240800B32530016721E81101"},                   // Rune of Minor Strength
        {0x21E81201, "903-25B80000240800B32530016721E81201"},                   // Rune of Minor Axe Mastery
        {0x21E81301, "903-25B80000240800B32530016721E81301"},                   // Rune of Minor Hammer Mastery
        {0x21E81401, "903-25B80000240800B32530016721E81401"},                   // Rune of Minor Swordsmanship
        {0x240800FD, "5558-25B80000240800FD253001FB27E902EA"},                  // Rune of Major Absorption
        {0x21E81502, "5558-25B80000240800B92530017321E815022530017320D80023"},  // Rune of Major Tactics
        {0x21E81102, "5558-25B80000240800B92530017321E811022530017320D80023"},  // Rune of Major Strength
        {0x21E81202, "5558-25B80000240800B92530017321E812022530017320D80023"},  // Rune of Major Axe Mastery
        {0x21E81302, "5558-25B80000240800B92530017321E813022530017320D80023"},  // Rune of Major Hammer Mastery
        {0x21E81402, "5558-25B80000240800B92530017321E814022530017320D80023"},  // Rune of Major Swordsmanship
        {0x240800FE, "5559-25B80000240800FE253001FD27EA02EA"},                  // Rune of Superior Absorption
        {0x21E81503, "5559-25B80000240800BF2530017F21E815032530017F20D8004B"},  // Rune of Superior Tactics
        {0x21E81103, "5559-25B80000240800BF2530017F21E811032530017F20D8004B"},  // Rune of Superior Strength
        {0x21E81203, "5559-25B80000240800BF2530017F21E812032530017F20D8004B"},  // Rune of Superior Axe Mastery
        {0x21E81303, "5559-25B80000240800BF2530017F21E813032530017F20D8004B"},  // Rune of Superior Hammer Mastery
        {0x21E81403, "5559-25B80000240800BF2530017F21E814032530017F20D8004B"},  // Rune of Superior Swordsmanship
        {0x240801FC, "19157-25B80000240801FCA53003F8A118030F"},                 // Frostbound Insignia
        {0x240801FE, "19159-25B80000240801FEA53003FCA118050F"},                 // Pyrebound Insignia
        {0x240801FF, "19160-25B80000240801FFA53003FEA118040F"},                 // Stormbound Insignia
        {0x24080201, "19162-25B80000240802018070040280D00000A5300402A0F80A00"}, // Scout's Insignia
        {0x240801FD, "19158-25B80000240801FDA53003FAA1180B0F"},                 // Earthbound Insignia
        {0x24080200, "19161-25B80000240802008070040081200000A5300400A0F80A00"}, // Beastmaster's Insignia
        {0x21E81801, "904-25B80000240800B42530016921E81801"},                   // Rune of Minor Wilderness Survival
        {0x21E81701, "904-25B80000240800B42530016921E81701"},                   // Rune of Minor Expertise
        {0x21E81601, "904-25B80000240800B42530016921E81601"},                   // Rune of Minor Beast Mastery
        {0x21E81901, "904-25B80000240800B42530016921E81901"},                   // Rune of Minor Marksmanship
        {0x21E81802, "5560-25B80000240800BA2530017521E818022530017520D80023"},  // Rune of Major Wilderness Survival
        {0x21E81702, "5560-25B80000240800BA2530017521E817022530017520D80023"},  // Rune of Major Expertise
        {0x21E81602, "5560-25B80000240800BA2530017521E816022530017520D80023"},  // Rune of Major Beast Mastery
        {0x21E81902, "5560-25B80000240800BA2530017521E819022530017520D80023"},  // Rune of Major Marksmanship
        {0x21E81803, "5561-25B80000240800C02530018121E818032530018120D8004B"},  // Rune of Superior Wilderness Survival
        {0x21E81703, "5561-25B80000240800C02530018121E817032530018120D8004B"},  // Rune of Superior Expertise
        {0x21E81603, "5561-25B80000240800C02530018121E816032530018120D8004B"},  // Rune of Superior Beast Mastery
        {0x21E81903, "5561-25B80000240800C02530018121E819032530018120D8004B"},  // Rune of Superior Marksmanship
        {0x240801F6, "19149-25B80000240801F6A53003ECA128000A"},                 // Wanderer's Insignia
        {0x240801F7, "19150-25B80000240801F7807003EE80B00800A53003EEA0F80F00"}, // Disciple's Insignia
        {0x240801F8, "19151-25B80000240801F8A53003F0A7280500"},                 // Anchorite's Insignia
        {0x21E80D01, "902-25B80000240800B22530016521E80D01"},                   // Rune of Minor Healing Prayers
        {0x21E80E01, "902-25B80000240800B22530016521E80E01"},                   // Rune of Minor Smiting Prayers
        {0x21E80F01, "902-25B80000240800B22530016521E80F01"},                   // Rune of Minor Protection Prayers
        {0x21E81001, "902-25B80000240800B22530016521E81001"},                   // Rune of Minor Divine Favor
        {0x21E80D02, "5556-25B80000240800B82530017121E80D022530017120D80023"},  // Rune of Major Healing Prayers
        {0x21E80E02, "5556-25B80000240800B82530017121E80E022530017120D80023"},  // Rune of Major Smiting Prayers
        {0x21E80F02, "5556-25B80000240800B82530017121E80F022530017120D80023"},  // Rune of Major Protection Prayers
        {0x21E81002, "5556-25B80000240800B82530017121E810022530017120D80023"},  // Rune of Major Divine Favor
        {0x21E80D03, "5557-25B80000240800BE2530017D21E80D032530017D20D8004B"},  // Rune of Superior Healing Prayers
        {0x21E80E03, "5557-25B80000240800BE2530017D21E80E032530017D20D8004B"},  // Rune of Superior Smiting Prayers
        {0x21E80F03, "5557-25B80000240800BE2530017D21E80F032530017D20D8004B"},  // Rune of Superior Protection Prayers
        {0x21E81003, "5557-25B80000240800BE2530017D21E810032530017D20D8004B"},  // Rune of Superior Divine Favor
        {0x2408020A, "19138-25B800002408020A2530041427E802A9"},                 // Bloodstained Insignia
        {0x240801EC, "19139-25B80000240801EC253003D828680208A53003D8A0F80A00"}, // Tormentor's Insignia
        {0x240801EE, "19141-25B80000240801EEA53003DCA118010F"},                 // Bonelace Insignia
        {0x240801EF, "19142-25B80000240801EFA53003DEA6E80500"},                 // Minion Master's Insignia
        {0x240801F0, "19143-25B80000240801F0807003E080B00400A53003E0A0F81400"}, // Blighter's Insignia
        {0x240801ED, "19140-25B80000240801EDA53003DAA7380500"},                 // Undertaker's Insignia
        {0x21E80401, "900-25B80000240800B02530016121E80401"},                   // Rune of Minor Blood Magic
        {0x21E80501, "900-25B80000240800B02530016121E80501"},                   // Rune of Minor Death Magic
        {0x21E80701, "900-25B80000240800B02530016121E80701"},                   // Rune of Minor Curses
        {0x21E80601, "900-25B80000240800B02530016121E80601"},                   // Rune of Minor Soul Reaping
        {0x21E80402, "5552-25B80000240800B62530016D21E804022530016D20D80023"},  // Rune of Major Blood Magic
        {0x21E80502, "5552-25B80000240800B62530016D21E805022530016D20D80023"},  // Rune of Major Death Magic
        {0x21E80702, "5552-25B80000240800B62530016D21E807022530016D20D80023"},  // Rune of Major Curses
        {0x21E80602, "5552-25B80000240800B62530016D21E806022530016D20D80023"},  // Rune of Major Soul Reaping
        {0x21E80403, "5553-25B80000240800BC2530017921E804032530017920D8004B"},  // Rune of Superior Blood Magic
        {0x21E80503, "5553-25B80000240800BC2530017921E805032530017920D8004B"},  // Rune of Superior Death Magic
        {0x21E80703, "5553-25B80000240800BC2530017921E807032530017920D8004B"},  // Rune of Superior Curses
        {0x21E80603, "5553-25B80000240800BC2530017921E806032530017920D8004B"},  // Rune of Superior Soul Reaping
        {0x240801E4, "19130-25B80000240801E4807003C880A00000A53003C8A0F80F00"}, // Virtuoso's Insignia
        {0x240801E2, "19128-25B80000240801E2A53003C4A7480300"},                 // Artificer's Insignia
        {0x240801E3, "19129-25B80000240801E3A53003C6A7280500"},                 // Prodigy's Insignia
        {0x21E80001, "899-25B80000240800AF2530015F21E80001"},                   // Rune of Minor Fast Casting
        {0x21E80201, "899-25B80000240800AF2530015F21E80201"},                   // Rune of Minor Domination Magic
        {0x21E80101, "899-25B80000240800AF2530015F21E80101"},                   // Rune of Minor Illusion Magic
        {0x21E80301, "899-25B80000240800AF2530015F21E80301"},                   // Rune of Minor Inspiration Magic
        {0x21E80002, "3612-25B80000240800B52530016B21E800022530016B20D80023"},  // Rune of Major Fast Casting
        {0x21E80202, "3612-25B80000240800B52530016B21E802022530016B20D80023"},  // Rune of Major Domination Magic
        {0x21E80102, "3612-25B80000240800B52530016B21E801022530016B20D80023"},  // Rune of Major Illusion Magic
        {0x21E80302, "3612-25B80000240800B52530016B21E803022530016B20D80023"},  // Rune of Major Inspiration Magic
        {0x21E80003, "5549-25B80000240800BB2530017721E800032530017720D8004B"},  // Rune of Superior Fast Casting
        {0x21E80203, "5549-25B80000240800BB2530017721E802032530017720D8004B"},  // Rune of Superior Domination Magic
        {0x21E80103, "5549-25B80000240800BB2530017721E801032530017720D8004B"},  // Rune of Superior Illusion Magic
        {0x21E80303, "5549-25B80000240800BB2530017721E803032530017720D8004B"},  // Rune of Superior Inspiration Magic
        {0x240801F2, "19145-25B80000240801F2A53003E4A128000AA53003E4A118030A"}, // Hydromancer Insignia
        {0x240801F3, "19146-25B80000240801F3A53003E6A128000AA53003E6A1180B0A"}, // Geomancer Insignia
        {0x240801F4, "19147-25B80000240801F4A53003E8A128000AA53003E8A118050A"}, // Pyromancer Insignia
        {0x240801F5, "19148-25B80000240801F5A53003EAA128000AA53003EAA118040A"}, // Aeromancer Insignia
        {0x240801F1, "19144-25B80000240801F1A53003E2A7080509"},                 // Prismatic Insignia
        {0x21E80C01, "901-25B80000240800B12530016321E80C01"},                   // Rune of Minor Energy Storage
        {0x21E80A01, "901-25B80000240800B12530016321E80A01"},                   // Rune of Minor Fire Magic
        {0x21E80801, "901-25B80000240800B12530016321E80801"},                   // Rune of Minor Air Magic
        {0x21E80901, "901-25B80000240800B12530016321E80901"},                   // Rune of Minor Earth Magic
        {0x21E80B01, "901-25B80000240800B12530016321E80B01"},                   // Rune of Minor Water Magic
        {0x21E80C02, "5554-25B80000240800B72530016F21E80C022530016F20D80023"},  // Rune of Major Energy Storage
        {0x21E80A02, "5554-25B80000240800B72530016F21E80A022530016F20D80023"},  // Rune of Major Fire Magic
        {0x21E80802, "5554-25B80000240800B72530016F21E808022530016F20D80023"},  // Rune of Major Air Magic
        {0x21E80902, "5554-25B80000240800B72530016F21E809022530016F20D80023"},  // Rune of Major Earth Magic
        {0x21E80B02, "5554-25B80000240800B72530016F21E80B022530016F20D80023"},  // Rune of Major Water Magic
        {0x21E80C03, "5555-25B80000240800BD2530017B21E80C032530017B20D8004B"},  // Rune of Superior Energy Storage
        {0x21E80A03, "5555-25B80000240800BD2530017B21E80A032530017B20D8004B"},  // Rune of Superior Fire Magic
        {0x21E80803, "5555-25B80000240800BD2530017B21E808032530017B20D8004B"},  // Rune of Superior Air Magic
        {0x21E80903, "5555-25B80000240800BD2530017B21E809032530017B20D8004B"},  // Rune of Superior Earth Magic
        {0x21E80B03, "5555-25B80000240800BD2530017B21E80B032530017B20D8004B"},  // Rune of Superior Water Magic
        {0x240801DE, "19124-25B80000240801DEA53003BCA158000AA53003BCA118000A"}, // Vanguard's Insignia
        {0x240801DF, "19125-25B80000240801DFA53003BEA158000AA53003BEA118010A"}, // Infiltrator's Insignia
        {0x240801E0, "19126-25B80000240801E0A53003C0A158000AA53003C0A118020A"}, // Saboteur's Insignia
        {0x240801E1, "19127-25B80000240801E1807003C280900000A53003C2A0F80F00"}, // Nightstalker's Insignia
        {0x21E82301, "6324-25B800002408013B2530027721E82301"},                  // Rune of Minor Critical Strikes
        {0x21E81D01, "6324-25B800002408013B2530027721E81D01"},                  // Rune of Minor Dagger Mastery
        {0x21E81E01, "6324-25B800002408013B2530027721E81E01"},                  // Rune of Minor Deadly Arts
        {0x21E81F01, "6324-25B800002408013B2530027721E81F01"},                  // Rune of Minor Shadow Arts
        {0x21E82302, "6325-25B800002408013C2530027921E823022530027920D80023"},  // Rune of Major Critical Strikes
        {0x21E81D02, "6325-25B800002408013C2530027921E81D022530027920D80023"},  // Rune of Major Dagger Mastery
        {0x21E81E02, "6325-25B800002408013C2530027921E81E022530027920D80023"},  // Rune of Major Deadly Arts
        {0x21E81F02, "6325-25B800002408013C2530027921E81F022530027920D80023"},  // Rune of Major Shadow Arts
        {0x21E82303, "6326-25B800002408013D2530027B21E823032530027B20D8004B"},  // Rune of Superior Critical Strikes
        {0x21E81D03, "6326-25B800002408013D2530027B21E81D032530027B20D8004B"},  // Rune of Superior Dagger Mastery
        {0x21E81E03, "6326-25B800002408013D2530027B21E81E032530027B20D8004B"},  // Rune of Superior Deadly Arts
        {0x21E81F03, "6326-25B800002408013D2530027B21E81F032530027B20D8004B"},  // Rune of Superior Shadow Arts
        {0x24080204, "19165-25B8000024080204A5300408A6F80500"},                 // Shaman's Insignia
        {0x24080205, "19166-25B80000240802058070040A80B01900A530040AA0F80F00"}, // Ghost Forge Insignia
        {0x24080206, "19167-25B80000240802068070040C80A00000A530040CA0F80F00"}, // Mystic's Insignia
        {0x21E82201, "6327-25B800002408013E2530027D21E82201"},                  // Rune of Minor Channeling Magic
        {0x21E82101, "6327-25B800002408013E2530027D21E82101"},                  // Rune of Minor Restoration Magic
        {0x21E82001, "6327-25B800002408013E2530027D21E82001"},                  // Rune of Minor Communing
        {0x21E82401, "6327-25B800002408013E2530027D21E82401"},                  // Rune of Minor Spawning Power
        {0x21E82202, "6328-25B800002408013F2530027F21E822022530027F20D80023"},  // Rune of Major Channeling Magic
        {0x21E82102, "6328-25B800002408013F2530027F21E821022530027F20D80023"},  // Rune of Major Restoration Magic
        {0x21E82002, "6328-25B800002408013F2530027F21E820022530027F20D80023"},  // Rune of Major Communing
        {0x21E82402, "6328-25B800002408013F2530027F21E824022530027F20D80023"},  // Rune of Major Spawning Power
        {0x21E82203, "6329-25B80000240801402530028121E822032530028120D8004B"},  // Rune of Superior Channeling Magic
        {0x21E82103, "6329-25B80000240801402530028121E821032530028120D8004B"},  // Rune of Superior Restoration Magic
        {0x21E82003, "6329-25B80000240801402530028121E820032530028120D8004B"},  // Rune of Superior Communing
        {0x21E82403, "6329-25B80000240801402530028121E824032530028120D8004B"},  // Rune of Superior Spawning Power
        {0x24080202, "19163-25B8000024080202A5300404A7180506"},                 // Windwalker Insignia
        {0x24080203, "19164-25B80000240802038070040681400600A5300406A0F80A00"}, // Forsaken Insignia
        {0x21E82C01, "15545-25B80000240801822530030521E82C01"},                 // Rune of Minor Mysticism
        {0x21E82B01, "15545-25B80000240801822530030521E82B01"},                 // Rune of Minor Earth Prayers
        {0x21E82901, "15545-25B80000240801822530030521E82901"},                 // Rune of Minor Scythe Mastery
        {0x21E82A01, "15545-25B80000240801822530030521E82A01"},                 // Rune of Minor Wind Prayers
        {0x21E82C02, "15546-25B80000240801832530030721E82C022530030720D80023"}, // Rune of Major Mysticism
        {0x21E82B02, "15546-25B80000240801832530030721E82B022530030720D80023"}, // Rune of Major Earth Prayers
        {0x21E82902, "15546-25B80000240801832530030721E829022530030720D80023"}, // Rune of Major Scythe Mastery
        {0x21E82A02, "15546-25B80000240801832530030721E82A022530030720D80023"}, // Rune of Major Wind Prayers
        {0x21E82C03, "15547-25B80000240801842530030921E82C032530030920D8004B"}, // Rune of Superior Mysticism
        {0x21E82B03, "15547-25B80000240801842530030921E82B032530030920D8004B"}, // Rune of Superior Earth Prayers
        {0x21E82903, "15547-25B80000240801842530030921E829032530030920D8004B"}, // Rune of Superior Scythe Mastery
        {0x21E82A03, "15547-25B80000240801842530030921E82A032530030920D8004B"}, // Rune of Superior Wind Prayers
        {0x24080207, "19168-25B80000240802078070040E81300000A530040EA0F80A00"}, // Centurion's Insignia
        {0x21E82801, "15548-25B80000240801852530030B21E82801"},                 // Rune of Minor Leadership
        {0x21E82701, "15548-25B80000240801852530030B21E82701"},                 // Rune of Minor Motivation
        {0x21E82601, "15548-25B80000240801852530030B21E82601"},                 // Rune of Minor Command
        {0x21E82501, "15548-25B80000240801852530030B21E82501"},                 // Rune of Minor Spear Mastery
        {0x21E82802, "15549-25B80000240801862530030D21E828022530030D20D80023"}, // Rune of Major Leadership
        {0x21E82702, "15549-25B80000240801862530030D21E827022530030D20D80023"}, // Rune of Major Motivation
        {0x21E82602, "15549-25B80000240801862530030D21E826022530030D20D80023"}, // Rune of Major Command
        {0x21E82502, "15549-25B80000240801862530030D21E825022530030D20D80023"}, // Rune of Major Spear Mastery
        {0x21E82803, "15550-25B80000240801872530030F21E828032530030F20D8004B"}, // Rune of Superior Leadership
        {0x21E82703, "15550-25B80000240801872530030F21E827032530030F20D8004B"}, // Rune of Superior Motivation
        {0x21E82603, "15550-25B80000240801872530030F21E826032530030F20D8004B"}, // Rune of Superior Command
        {0x21E82503, "15550-25B80000240801872530030F21E825032530030F20D8004B"}, // Rune of Superior Spear Mastery
        {0x240801E6, "19132-25B80000240801E6253003CC26D80500"},                 // Survivor Insignia
        {0x240801E5, "19131-25B80000240801E5253003CA26C80001"},                 // Radiant Insignia
        {0x240801E7, "19133-25B80000240801E7A53003CEA158000A"},                 // Stalwart Insignia
        {0x240801E8, "19134-25B80000240801E8807003D080900000A53003D0A0F80A00"}, // Brawler's Insignia
        {0x240801E9, "19135-25B80000240801E9807003D280B00600A53003D2A0F80A00"}, // Blessed Insignia
        {0x240801EA, "19136-25B80000240801EA807003D480C00000A53003D4A0F80A00"}, // Herald's Insignia
        {0x240801EB, "19137-25B80000240801EB807003D681100000A53003D6A0F80A00"}, // Sentry's Insignia
        {0x24080211, "898-25B80000240802112530042322D80002"},                   // Rune of Attunement
        {0x24080213, "5550-25B80000240802132530042727780407"},                  // Rune of Recovery
        {0x24080214, "5550-25B80000240802142530042927780300"},                  // Rune of Restoration
        {0x24080215, "5550-25B80000240802152530042B27780801"},                  // Rune of Clarity
        {0x24080216, "5550-25B80000240802162530042D27780605"},                  // Rune of Purity
        {0x240800FF, "898-25B80000240800FF253001FF27E802C2"},                   // Rune of Minor Vigor
        {0x240800C2, "898-25B80000240800FF253001FF27E802C2"},                   // Rune of Minor Vigor
        {0x24080101, "5551-25B80000240801012530020327EA02C2"},                  // Rune of Superior Vigor
        {0x24080100, "5550-25B80000240801002530020127E902C2"},                  // Rune of Major Vigor
        {0x24080212, "898-25B80000240802122530042523480A00"}                    // Rune of Vitae
    };


    void LoadFromCache()
    {
        const auto computer_path = Resources::GetComputerFolderPath();
        const auto cache_path = computer_path / "price_quote_cache.json";
        std::ifstream cache_file(cache_path);
        if (!cache_file.is_open()) {
            return;
        }

        try
        {
            std::stringstream buffer;
            buffer << cache_file.rdbuf();
            cache_file.close();

            price_json = nlohmann::json::parse(buffer.str());
        }
        catch (...)
        {
        }

        return;
    }

    void CacheResponse(const std::string& response)
    {
        const auto computer_path = Resources::GetComputerFolderPath();
        const auto cache_path = computer_path / "price_quote_cache.json";
        std::ofstream cache_file(cache_path);
        if (!cache_file.is_open()) {
            return;
        }

        cache_file << response;
        cache_file.close();
    }

    void EnsureLatestPriceJson()
    {
        if (TIMER_DIFF(last_request_time) < 3600)
            return;
        if (fetching_prices)
            return;

        fetching_prices = true;
        last_request_time = TIMER_INIT();
        Resources::Download(trader_quotes_url, [](bool success, const std::string& response, void*) {
            if (!success)
            {
                LoadFromCache();
                fetching_prices = false;
                return;
            }

            try
            {
                CacheResponse(response);
                price_json = nlohmann::json::parse(response);
            }
            catch (...)
            {
            }

            fetching_prices = false;
            });
    }

    bool IsCommonMaterial(const GW::Item* item) {
        if (item && item->GetModifier(0x2508))
            return item->GetModifier(0x2508)->arg1() <= std::to_underlying(GW::Constants::MaterialSlot::Feather);
        return false;
    }

    std::wstring PrintPrice(float price, const char* name = nullptr) {
        auto unit = L'g';
        auto color = L"@ItemCommon";
        if (price > high_price_threshold) {
            color = L"@ItemRare";
        }

        if (price > 1000.f) {
            price /= 1000.f;
            unit = L'k';
        }
        return std::format(L"\x2\x108\x107\n<c={}>{}: {:.4g}{}</c>\x1", color, name && *name ? GuiUtils::StringToWString(name) : L"Item price", price, unit);
    }

    void UpdateDescription(const uint32_t item_id, wchar_t** description_out)
    {
        if (!(description_out && *description_out))
            return;
        const auto item = GW::Items::GetItemById(item_id);
        if (!item)
            return;
        
        std::vector<uint32_t> mod_matches;
        modified_description = *description_out;
        for (size_t i = 0; i < item->mod_struct_size; i++) {
            const auto found = mod_to_id.find(item->mod_struct[i].mod);
            if (found == mod_to_id.end())
                continue;
            auto price = PriceCheckerModule::Instance().GetPriceById(found->second);
            if (price < .1f)
                continue;
            const auto name = mod_to_name.find(found->first);
            if (name == mod_to_name.end())
                continue;
            modified_description.append(PrintPrice(price, name->second));
        }
        if (item->type == GW::Constants::ItemType::Materials_Zcoins) {
            const auto model_id_str = std::to_string(item->model_id);
            auto price = PriceCheckerModule::Instance().GetPriceById(model_id_str.c_str());
            if (price > .0f) {
                if (IsCommonMaterial(item)) {
                    price = price / 10;
                }
                modified_description += PrintPrice(price);
            }
        }

        *description_out = modified_description.data();
    }

    using GetItemDescription_pt = void(__cdecl*)(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out, wchar_t** out2);
    GetItemDescription_pt GetItemDescription_Func = nullptr, GetItemDescription_Ret = nullptr;
    // Block full item descriptions
    void OnGetItemDescription(const uint32_t item_id, const uint32_t flags, const uint32_t quantity, const uint32_t unk, wchar_t** name_out, wchar_t** description_out)
    {
        GW::Hook::EnterHook();
        wchar_t** tmp_name_out = nullptr;
        if (!name_out)
            name_out = tmp_name_out; // Ensure name_out is valid; we're going to piggy back on it.
        GetItemDescription_Ret(item_id, flags, quantity, unk, name_out, description_out);
        UpdateDescription(item_id, description_out);
        GW::Hook::LeaveHook();
    }

}


void PriceCheckerModule::Initialize()
{
    ToolboxModule::Initialize();

    // Copied from GameSettings.cpp
    GetItemDescription_Func = (GetItemDescription_pt)GameSettings::OnGetItemDescription;
    if (GetItemDescription_Func) {
        ASSERT(GW::HookBase::CreateHook((void**)&GetItemDescription_Func, OnGetItemDescription, (void**)&GetItemDescription_Ret) == 0);
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }

    LoadFromCache();
}

void PriceCheckerModule::Terminate()
{
    ToolboxModule::Terminate();

    if (GetItemDescription_Func) {
        GW::HookBase::DisableHooks(GetItemDescription_Func);
    }
}

void PriceCheckerModule::Update(const float)
{
    EnsureLatestPriceJson();
}

void PriceCheckerModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_FLOAT(high_price_threshold);
    SAVE_BOOL(fetch_module_prices);
}

void PriceCheckerModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    LOAD_FLOAT(high_price_threshold);
    LOAD_BOOL(fetch_module_prices);
}

void PriceCheckerModule::RegisterSettingsContent()
{
    //ToolboxModule::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, const bool is_showing) {
            if (!is_showing) {
                return;
            }

            ImGui::Checkbox("Fetch prices for item components", &fetch_module_prices);
            ImGui::ShowHelp("When enabled, the item description will contain information about the components of the item and their respective prices");
            ImGui::SliderFloat("High price threshold", &high_price_threshold, 100, 50000);
        },
        0.9f
    );
}

float PriceCheckerModule::GetPriceById(const char* id) {
    const auto& it_buy = price_json.find("buy");
    if (it_buy == price_json.end() || !it_buy->is_object()) {
        return 0;
    }

    const auto& buy = it_buy.value();
    const auto& it_id = buy.find(id);
    if (it_id == it_buy->end() || !it_id->is_object()) {
        return 0;
    }
    const auto& price_value = it_id->find("p");
    if (!(price_value != it_id->end() && price_value->is_number())) return 0;
    return price_value->get<float>();
}
