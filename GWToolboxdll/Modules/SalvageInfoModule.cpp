#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <ctime>
#include <regex>
#include <base64.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/TextParser.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/ItemIDs.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Modules/GameSettings.h>
#include <Modules/SalvageInfoModule.h>

#include <Logger.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>

using nlohmann::json;

namespace {
    std::unordered_map<GW::ItemID, std::string> english_material_names{
        {0, "Crafting materials"}, {1, "Rare crafting materials"},
        {921, "Bone"}, {948, "Iron Ingot"}, {940, "Tanned Hide Square"},
        {953, "Scale"}, {954, "Chitin Fragment"}, {925, "Bolt of Cloth"},
        {946, "Wood Plank"}, {955, "Granite Slab"}, {929, "Pile of Glittering Dust"},
        {934, "Plant Fiber"}, {933, "Feather"}, {941, "Fur Square"},
        {926, "Bolt of Linen"}, {927, "Bolt of Damask"}, {928, "Bolt of Silk"},
        {930, "Glob of Ectoplasm"}, {949, "Steel Ingot"}, {950, "Deldrimor Steel Ingot"},
        {923, "Monstrous Claw"}, {931, "Monstrous Eye"}, {932, "Monstrous Fang"},
        {937, "Ruby"}, {938, "Sapphire"}, {935, "Diamond"},
        {936, "Onyx Gemstone"}, {922, "Lump of Charcoal"}, {945, "Obsidian Shard"},
        {939, "Tempered Glass Vial"}, {942, "Leather Square"}, {943, "Elonian Leather Square"},
        {944, "Vial of Ink"}, {951, "Roll of Parchment"}, {952, "Roll of Vellum"},
        {956, "Spiritwood Plank"}, {6532, "Amber Chunk"}, {6533, "Jadeite Shard"}
    };

    std::unordered_map<GW::ItemID, std::string> korean_material_names{
        {0, "재료 제작"}, {1, "희귀 제작 재료"},
        {921, "뼈조각"}, {948, "철광석"}, {940, "무두질한 가죽 조각"},
        {953, "비늘"}, {954, "각질 파편"}, {925, "모직물"},
        {946, "목재"}, {955, "화강암 조각"}, {929, "빛나는 먼지 덩어리"},
        {934, "실뭉치"}, {933, "깃털"}, {941, "모피 조각"},
        {926, "리넨"}, {927, "다마스크"}, {928, "비단"},
        {930, "심령의 구슬"}, {949, "강철 광석"}, {950, "델트리모 광석"},
        {923, "괴수의 발톱"}, {931, "괴수의 눈"}, {932, "괴수의 송곳니"},
        {937, "루비"}, {938, "사파이어"}, {935, "다이아몬드"},
        {936, "오닉스 원석"}, {922, "목탄 덩어리"}, {945, "흑요석 파편"},
        {939, "강화 유리 약병"}, {942, "재단된 가죽"}, {943, "엘로나 가죽 조각"},
        {944, "잉크병"}, {951, "양피 두루마리"}, {952, "송아지 피지 두루마리"},
        {956, "심령의 판자"}, {6532, "호박석 덩어리"}, {6533, "비취 원석 조각"}
    };

    std::unordered_map<GW::ItemID, std::string> french_material_names{
        {0, "Matériaux d' artisanat"}, {1, "Matériaux rares"},
        {921, "Ossement"}, {948, "Lingot de fer"}, {940, "Peau tannée"},
        {953, "Ecaille"}, {954, "Fragment de chitine"}, {925, "Rouleau de tissu"},
        {946, "Planche de bois"}, {955, "Bloc de granite"}, {929, "Tas de Poussière scintillante"},
        {934, "Fibre végétale"}, {933, "Plume"}, {941, "Morceau de fourrure"},
        {926, "Rouleau de lin"}, {927, "Rouleau de Damas"}, {928, "Rouleau de soie"},
        {930, "Boule d'ectoplasme"}, {949, "Lingot d'acier"}, {950, "Lingot d'acier de Deldrimor"},
        {923, "Griffe monstrueuse"}, {931, "Oeil monstrueux"}, {932, "Croc monstrueux"},
        {937, "Rubis"}, {938, "Saphir"}, {935, "Diamant"},
        {936, "Gemme d'onyx"}, {922, "Morceau de charbon"}, {945, "Fragment d'obsidienne"},
        {939, "Fiole de verre trempé"}, {942, "Carré de cuir"}, {943, "Carré de cuir élonien"}, 
        {944, "Fiole d'encre"}, {951, "Rouleau de parchemin"}, {952, "Rouleau de vélin"},
        {956, "Planche de Boispirite"}, {6532, "Morceau d'ambre"}, {6533, "Eclat de Jadéite"}
    };

    std::unordered_map<GW::ItemID, std::string> german_material_names{
        {0, "Handwerksmaterialien"}, {1, "Seltene materialien"},
        {921, "Knochen"}, {948, "Eisenbarren"}, {940, "Gerbfellquadrat"},
        {953, "Schuppe"}, {954, "Chitin-Fragment"}, {925, "Tuchballen"},
        {946, "Holzbrett"}, {955, "Granitplatte"}, {929, "Glitzerstaubhaufen"},
        {934, "Pflanzenfaser"}, {933, "Feder"}, {941, "Pelzquadrat"},
        {926, "Leinentuchballen"}, {927, "Damastballen"}, {928, "Seidentuchballen"},
        {930, "Ektoplasmakugel"}, {949, "Stahlbarren"}, {950, "Deldrimor-Stahlbarren"},
        {923, "Monsterklaue"}, {931, "Monsterauge"}, {932, "Monsterfangzahn"},
        {937, "Rubin"}, {938, "Saphir"}, {935, "Diamant"},
        {936, "Onyx"}, {922, "Holzkohleklumpen"}, {945, "Obsidian-Scherbe"},
        {939, "Vergütetes Glasfläschchen"}, {942, "Lederquadrat"}, {943, "Elona-Lederquadrat"},
        {944, "Tintenfläschchen"}, {951, "Pergamentrolle"}, {952, "Vellinrolle"},
        {956, "Geisterholzbrett"}, {6532, "Bernstein-Klumpen"}, {6533, "Jadeit-Scherbe"}
    };

    std::unordered_map<GW::ItemID, std::string> italian_material_names{
        {0, "Materiali artigianali"}, {1, "Materiali rari"},
        {921, "Osso"}, {948, "Lingotto di Ferro"}, {940, "Scampolo di Pellame Pregiato"},
        {953, "Scaglia"}, {954, "Frammento di Chitina"}, {925, "Rotolo di Tessuto Grezzo"},
        {946, "Tavola di Legno"}, {955, "Lastra di Granito"}, {929, "Mucchio di Polvere Brillante"},
        {934, "Fibra di Pianta"}, {933, "Piuma"}, {941, "Scampolo di Pelliccia"},
        {926, "Rotolo di Lino"}, {927, "Rotolo di Damasco"}, {928, "Rotolo di Seta"},
        {930, "Globo di Ectoplasma"}, {949, "Lingotto d'Acciaio"}, {950, "Lingotto d'Acciaio di Deldrimor"},
        {923, "Artiglio Mostruoso"}, {931, "Occhio Mostruoso"}, {932, "Zanna Mostruosa"},
        {937, "Rubino"}, {938, "Zaffiro"}, {935, "Diamante"},
        {936, "Pietra d'Onice"}, {922, "Pezzo di Carbone"}, {945, "Frammento di Ossidiana"},
        {939, "Fiala di Vetro Temprato"}, {942, "Scampolo di Pelle"}, {943, "Scampolo di Pelle di Elona"},
        {944, "Fiala di Inchiostro"}, {951, "Rotolo di Cartapecora"}, {952, "Rotolo di Pergamena"},
        {956, "Tavola di Legno degli Spiriti"}, {6532, "Pezzo di Ambra"}, {6533, "Frammento di Giadeite"}
    };

    std::unordered_map<GW::ItemID, std::string> spanish_material_names{
        {0, "Materiales artesanales"}, {1, "Materiales poco frecuentes"},
        {921, "Hueso"}, {948, "Lingote de hierro"}, {940, "Trozo de piel curtido"},
        {953, "Escama"}, {954, "Fragmento de quitina"}, {925, "Rollo de tela"},
        {946, "Tabla de madera"}, {955, "Losa de granito"}, {929, "Pila de polvo brillante"},
        {934, "Maraña de fibras"}, {933, "Pluma"}, {941, "Forro"},
        {926, "Rollo de lino"}, {927, "Rollo de tela de damasco"}, {928, "Rollo de seda"},
        {930, "Pegote de ectoplasma"}, {949, "Lingote de acero"}, {950, "Lingote de acero de Deldrimor"},
        {923, "Garra de monstruo"}, {931, "Ojo de monstruo"}, {932, "Colmillo de monstruo"},
        {937, "Rubí"}, {938, "Zafiro"}, {935, "Diamante"},
        {936, "Piedra ónice"}, {922, "Trozo de carbón"}, {945, "Trozo de obsidiana"},
        {939, "Frasco de vidrio templado"}, {942, "Trozo de cuero"}, {943, "Trozo de cuero de Elona"},
        {944, "Frasco de tinta"}, {951, "Rollo de pergamino"}, {952, "Rollo de vitela"},
        {956, "Tabla de madera espiritual"}, {6532, "Trozo de ámbar"}, {6533, "Trozo de jadeíta"}
    };

    std::unordered_map<GW::ItemID, std::string> traditionalchinese_material_names{
        {0, "製作材料"}, {1, "稀有製作材料"},
        {921, "骨頭"}, {948, "鐵礦石"}, {940, "褐色獸皮"},
        {953, "鱗片"}, {954, "外殼"}, {925, "布料"},
        {946, "木材"}, {955, "花崗岩石板"}, {929, "閃爍之土"},
        {934, "植物纖維"}, {933, "羽毛"}, {941, "毛皮"},
        {926, "亞麻布"}, {927, "緞布"}, {928, "絲綢"},
        {930, "心靈之玉"}, {949, "鋼鐵礦石"}, {950, "戴爾狄摩鋼鐵礦石"},
        {923, "巨大的爪"}, {931, "巨大的眼"}, {932, "巨大尖牙"},
        {937, "紅寶石"}, {938, "藍寶石"}, {935, "金剛石"},
        {936, "瑪瑙寶石"}, {922, "結塊的木炭"}, {945, "黑曜石碎片"},
        {939, "調合後的玻璃瓶"}, {942, "皮革"}, {943, "伊洛那皮革"},
        {944, "小瓶墨水"}, {951, "羊皮紙卷"}, {952, "牛皮紙卷"},
        {956, "心靈之板"}, {6532, "琥珀"}, {6533, "硬玉"}
    };

    std::unordered_map<GW::ItemID, std::string> japanese_material_names{
        {0, "クラフト素材"}, {1, "レアなクラフト素材"},
        {921, "骨片"}, {948, "鉄鉱石"}, {940, "動物の皮"},
        {953, "鱗片"}, {954, "甲殻のかけら"}, {925, "布の生地"},
        {946, "木の枝"}, {955, "グラニットの石板"}, {929, "光り輝く粉"},
        {934, "植物の繊維"}, {933, "羽毛"}, {941, "毛皮"},
        {926, "リネンの生地"}, {927, "ダマスク織の生地"}, {928, "シルクの生地"},
        {930, "エクトプラズム"}, {949, "鋼鉄"}, {950, "デルドリモア鉱石"},
        {923, "巨大な爪"}, {931, "巨大な目"}, {932, "巨大な牙"},
        {937, "ルビー"}, {938, "サファイア"}, {935, "ダイアモンド"},
        {936, "オニキス"}, {922, "黒炭"}, {945, "黒曜石の破片"},
        {939, "強化ガラスビン"}, {942, "レザー"}, {943, "イロナ産の革"},
        {944, "インクビン"}, {951, "パーチメント紙"}, {952, "ベラム紙"},
        {956, "霊木"}, {6532, "コハクの塊"}, {6533, "ヒスイの輝石"}
    };

    std::unordered_map<GW::ItemID, std::string> polish_material_names{
        {0, "Surowcami"}, {1, "Rzadkimi surowcami"},
        {921, "Kość"}, {948, "Sztabka Żelaza"}, {940, "Łatka Garbowanej Skóry"},
        {953, "Łuska"}, {954, "Kawałek Chityny"}, {925, "Skrawek Płótna"},
        {946, "Szczapa Drewna"}, {955, "Płytka Granitowa"}, {929, "Garść Lśniącego Pyłu"},
        {934, "Włókno Roślinne"}, {933, "Pióro"}, {941, "Łatka Futra"},
        {926, "Skrawek Lnu"}, {927, "Skrawek Adamaszku"}, {928, "Skrawek Jedwabiu"},
        {930, "Kula Ektoplazmy"}, {949, "Sztabka Stali"}, {950, "Sztabka Stali Deldrimorskiej"},
        {923, "Pazur Bestii"}, {931, "Oko Bestii"}, {932, "Kieł Bestii"},
        {937, "Rubin"}, {938, "Szafir"}, {935, "Diament"},
        {936, "Onyks"}, {922, "Bryłka Węgla"}, {945, "Odłamek Obsydianu"},
        {939, "Fiolka z Hartowanego Szkła"}, {942, "Łatka Skóry"}, {943, "Elońska Skórzana Łatka"},
        {944, "Fiolka Atramentu"}, {951, "Zwój Pergaminu"}, {952, "Zwój Welinu"},
        {956, "Gałąź Duchodrewna"}, {6532, "Bryłka Bursztynu"}, {6533, "Odłamek Jadeitu"}
    };

    std::unordered_map<GW::ItemID, std::string> russian_material_names{ // For some reason, the text parser for russian returns english names. Leaving like this for now
        {0, "Crafting materials"}, {1, "Rare crafting materials"},
        {921, "Bone"}, {948, "Iron Ingot"}, {940, "Tanned Hide Square"},
        {953, "Scale"}, {954, "Chitin Fragment"}, {925, "Bolt of Cloth"},
        {946, "Wood Plank"}, {955, "Granite Slab"}, {929, "Pile of Glittering Dust"},
        {934, "Plant Fiber"}, {933, "Feather"}, {941, "Fur Square"},
        {926, "Bolt of Linen"}, {927, "Bolt of Damask"}, {928, "Bolt of Silk"},
        {930, "Glob of Ectoplasm"}, {949, "Steel Ingot"}, {950, "Deldrimor Steel Ingot"},
        {923, "Monstrous Claw"}, {931, "Monstrous Eye"}, {932, "Monstrous Fang"},
        {937, "Ruby"}, {938, "Sapphire"}, {935, "Diamond"},
        {936, "Onyx Gemstone"}, {922, "Lump of Charcoal"}, {945, "Obsidian Shard"},
        {939, "Tempered Glass Vial"}, {942, "Leather Square"}, {943, "Elonian Leather Square"},
        {944, "Vial of Ink"}, {951, "Roll of Parchment"}, {952, "Roll of Vellum"},
        {956, "Spiritwood Plank"}, {6532, "Amber Chunk"}, {6533, "Jadeite Shard"}
    };

    std::unordered_map<GW::ItemID, std::string> borkborkbork_material_names{
        {0, "Creffting meteriels"}, {1, "Rere-a creffting meteriels"},
        {921, "Bune-a"}, {948, "Irun Ingut"}, {940, "Tunned Heede-a Sqooaere-a"},
        {953, "Scaele-a"}, {954, "Cheeteen Fraegment"}, {925, "Bult ooff Clut"},
        {946, "Vuud Plunk"}, {955, "Gruneete-a Slaeb"}, {929, "Peele-a ooff Gleettereeng Doost"},
        {934, "Plunt Feeber"}, {933, "Feaezeer"}, {941, "Foor Sqooaere-a"},
        {926, "Bult ooff Leenee"}, {927, "Bult ooff Daemaesk"}, {928, "Bult ooff Seelk"},
        {930, "Glub ooff Ictuplaesm"}, {949, "Steel Ingut"}, {950, "Deldreemur Steel Ingut"},
        {923, "Munstruoos Claev"}, {931, "Munstruoos Iye-a"}, {932, "Munstruoos Fung"},
        {937, "Rooby"}, {938, "Saepphure-a"}, {935, "Deeaemund"},
        {936, "Oonyx Gemstune-a"}, {922, "Loomp ooff Chaercuael"}, {945, "Oobseedeeun Shaerd"},
        {939, "Tempered Glaess Feeael"}, {942, "Leaezeer Sqooaere-a"}, {943, "Iluneeun Leaezeer Sqooaere-a"},
        {944, "Feeael ooff Ink"}, {951, "Rull ooff Paerchment"}, {952, "Rull ooff Felloom"},
        {956, "Spureetvuud Plunk"}, {6532, "Aember Choonk"}, {6533, "Jaedeeete-a Shaerd"}
    };

    std::unordered_map<GW::Constants::Language, std::unordered_map<GW::ItemID, std::string>> material_names; // initialized with all the other maps on module initialization

    std::unordered_map<std::string, GW::ItemID> material_to_id_lookup {
        {"bones", GW::Constants::ItemID::Bone},
        {"bone", GW::Constants::ItemID::Bone},
        {"bolts of cloth", GW::Constants::ItemID::BoltofCloth},
        {"bolt of cloth", GW::Constants::ItemID::BoltofCloth},
        {"piles of glittering dust", GW::Constants::ItemID::PileofGlitteringDust},
        {"pile of glittering dust", GW::Constants::ItemID::PileofGlitteringDust},
        {"feathers", GW::Constants::ItemID::Feather},
        {"feather", GW::Constants::ItemID::Feather},
        {"plant fibers", GW::Constants::ItemID::PlantFiber},
        {"plant fiber", GW::Constants::ItemID::PlantFiber},
        {"tanned hide squares", GW::Constants::ItemID::TannedHideSquare},
        {"tanned hide square", GW::Constants::ItemID::TannedHideSquare},
        {"wood planks", GW::Constants::ItemID::WoodPlank},
        {"wood plank", GW::Constants::ItemID::WoodPlank},
        {"iron ingots", GW::Constants::ItemID::IronIngot},
        {"iron ingot", GW::Constants::ItemID::IronIngot},
        {"scales", GW::Constants::ItemID::Scale},
        {"scale", GW::Constants::ItemID::Scale},
        {"chitin fragments", GW::Constants::ItemID::ChitinFragment},
        {"chitin fragment", GW::Constants::ItemID::ChitinFragment},
        {"granite slabs", GW::Constants::ItemID::GraniteSlab},
        {"granite slab", GW::Constants::ItemID::GraniteSlab},
        {"amber chunks", GW::Constants::ItemID::AmberChunk},
        {"amber chunk", GW::Constants::ItemID::AmberChunk},
        {"bolts of damask", GW::Constants::ItemID::BoltofDamask},
        {"bolt of damask", GW::Constants::ItemID::BoltofDamask},
        {"bolts of linen", GW::Constants::ItemID::BoltofLinen},
        {"bolt of linen", GW::Constants::ItemID::BoltofLinen},
        {"bolts of silk", GW::Constants::ItemID::BoltofSilk},
        {"bolt of silk", GW::Constants::ItemID::BoltofSilk},
        {"deldrimor steel ingots", GW::Constants::ItemID::DeldrimorSteelIngot},
        {"deldrimor steel ingot", GW::Constants::ItemID::DeldrimorSteelIngot},
        {"diamonds", GW::Constants::ItemID::Diamond},
        {"diamond", GW::Constants::ItemID::Diamond},
        {"elonian leather squares", GW::Constants::ItemID::ElonianLeatherSquare},
        {"elonian leather square", GW::Constants::ItemID::ElonianLeatherSquare},
        {"fur squares", GW::Constants::ItemID::FurSquare},
        {"fur square", GW::Constants::ItemID::FurSquare},
        {"globs of ectoplasm", GW::Constants::ItemID::GlobofEctoplasm},
        {"glob of ectoplasm", GW::Constants::ItemID::GlobofEctoplasm},
        {"jadeite shards", GW::Constants::ItemID::JadeiteShard},
        {"jadeite shard", GW::Constants::ItemID::JadeiteShard},
        {"leather squares", GW::Constants::ItemID::LeatherSquare},
        {"leather square", GW::Constants::ItemID::LeatherSquare},
        {"lumps of charcoal", GW::Constants::ItemID::LumpofCharcoal},
        {"lump of charcoal", GW::Constants::ItemID::LumpofCharcoal},
        {"monstrous claws", GW::Constants::ItemID::MonstrousClaw},
        {"monstrous claw", GW::Constants::ItemID::MonstrousClaw},
        {"monstrous eyes", GW::Constants::ItemID::MonstrousEye},
        {"monstrous eye", GW::Constants::ItemID::MonstrousEye},
        {"monstrous fangs", GW::Constants::ItemID::MonstrousFang},
        {"monstrous fang", GW::Constants::ItemID::MonstrousFang},
        {"obsidian shards", GW::Constants::ItemID::ObsidianShard},
        {"obsidian shard", GW::Constants::ItemID::ObsidianShard},
        {"onyx gemstones", GW::Constants::ItemID::OnyxGemstone},
        {"onyx gemstone", GW::Constants::ItemID::OnyxGemstone},
        {"rolls of parchment", GW::Constants::ItemID::RollofParchment},
        {"roll of parchment", GW::Constants::ItemID::RollofParchment},
        {"rolls of vellum", GW::Constants::ItemID::RollofVellum},
        {"roll of vellum", GW::Constants::ItemID::RollofVellum},
        {"rubys", GW::Constants::ItemID::Ruby},
        {"ruby", GW::Constants::ItemID::Ruby},
        {"sapphires", GW::Constants::ItemID::Sapphire},
        {"sapphire", GW::Constants::ItemID::Sapphire},
        {"spiritwood planks", GW::Constants::ItemID::SpiritwoodPlank},
        {"spiritwood plank", GW::Constants::ItemID::SpiritwoodPlank},
        {"steel ingots", GW::Constants::ItemID::SteelIngot},
        {"steel ingot", GW::Constants::ItemID::SteelIngot},
        {"tempered glass vials", GW::Constants::ItemID::TemperedGlassVial},
        {"tempered glass vial", GW::Constants::ItemID::TemperedGlassVial},
        {"vials of ink", GW::Constants::ItemID::VialofInk},
        {"vial of ink", GW::Constants::ItemID::VialofInk},
    };

    struct SalvageInfo {
        std::vector<GW::ItemID> crafting_materials;
        std::vector<GW::ItemID> rare_crafting_materials;
    };

    void to_json(json& j, const SalvageInfo& p){
        j = json{
            {"CraftingMaterials", p.crafting_materials},
            {"RareCraftingMaterials", p.rare_crafting_materials},
        };
    }

    void from_json(json& j, SalvageInfo& p){
        j.at("CraftingMaterials").get_to(p.crafting_materials);
        j.at("RareCraftingMaterials").get_to(p.rare_crafting_materials);
    }

    // The follwing regex captures the text between [[ ]] in the disambiguation page: eg. Image:Eerie Focus (Nightfall).jpg|[[Eerie Focus (Nightfall)]]
    const std::regex disambig_entries_regex(R"(\[\[([^\[\]]+)\]\])");
    // The following regexes capture the entire text after commonsalvage or raresalvage: eg. | commonsalvage = [[Pile of Glittering Dust|Piles of Glittering Dust]]
    const std::regex common_materials_regex("commonsalvage = (([a-zA-Z [\\]<>|'])*)");
    const std::regex rare_materials_regex("raresalvage = (([a-zA-Z [\\]<>|'])*)");
    bool fetch_salvage_info = true;
    const std::string disambig = "{{disambig}}";
    const std::wstring color = L"@ItemCommon";
    std::wstring modified_description;
    std::wstring name_cache;
    // Json containing uint32_t keys being the item IDs and string values being the salvage info
    json salvage_info_cache = nullptr;
    bool fetching_info = false;
    std::queue<std::wstring> fetch_queue;

    std::vector<unsigned char> WStringToByteArray(const std::wstring& wstr)
    {
        std::vector<unsigned char> byte_array;
        byte_array.reserve(wstr.size() * sizeof(wchar_t));
        for (wchar_t wc : wstr) {
            byte_array.push_back((wc >> 0) & 0xFF); // Low byte
            byte_array.push_back((wc >> 8) & 0xFF); // High byte (if little-endian)
        }
        return byte_array;
    }

    std::string GetBase64EncodedName(std::wstring value)
    {
        const auto bytes = WStringToByteArray(value);
        std::string key_str;
        key_str.resize(bytes.size());
        b64_enc((void*)bytes.data(), bytes.size(), key_str.data());
        return key_str;
    }

    std::string ToLowerString(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = static_cast<char>(std::tolower(c));
        }

        return result;
    }

    std::string Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\n\r\f\v");
        size_t end = str.find_last_not_of(" \t\n\r\f\v");
        return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
    }

    std::string Replace(std::string& source, const std::string& from, const std::string& to) {
        size_t startPos = 0;
        while ((startPos = source.find(from, startPos)) != std::string::npos) {
            source.replace(startPos, from.length(), to);
            startPos += to.length(); // Move past the last replaced part to avoid replacing it again
        }

        return source;
    }

    //Splits a string by a substring, removing the separators
    std::vector<std::string> Split(const std::string& s, const std::string& separator)
    {
        const auto sep_len = separator.size();
        std::vector<std::string> output;
        std::string::size_type prev_pos = 0, pos = 0;
        while ((pos = s.find(separator, pos)) != std::string::npos) {
            std::string substring(s.substr(prev_pos, pos - prev_pos));
            output.push_back(substring);
            prev_pos = ++pos + sep_len - 1;
        }

        output.push_back(s.substr(prev_pos, pos - prev_pos));
        return output;
    }

    void AsyncGetItemSimpleName(const std::wstring& name, std::wstring& res, const GW::Constants::Language language = GW::Constants::Language::English)
    {
        if (name.empty()) return;
        GW::UI::AsyncDecodeStr(name.c_str(), &res, language);
    }

    void LoadLocalCache()
    {
        const auto computer_path = Resources::GetComputerFolderPath();
        const auto cache_path = computer_path / "salvage_info_cache.json";
        std::ifstream cache_file(cache_path);
        if (!cache_file.is_open()) {
            return;
        }

        try
        {
            std::stringstream buffer;
            buffer << cache_file.rdbuf();
            cache_file.close();

            salvage_info_cache = nlohmann::json::parse(buffer.str());
        }
        catch (...)
        {
        }

        return;
    }

    void SaveLocalCache()
    {
        const auto computer_path = Resources::GetComputerFolderPath();
        const auto cache_path = computer_path / "salvage_info_cache.json";
        std::ofstream cache_file(cache_path);
        if (!cache_file.is_open()) {
            return;
        }

        cache_file << salvage_info_cache.dump();
        cache_file.close();
    }

    std::optional<SalvageInfo> LoadFromCache(std::wstring key) {
        const auto key_str = GetBase64EncodedName(key);
        Log::Info(std::format("[{}] Looking for key", key_str).c_str());
        const auto iter = salvage_info_cache.find(key_str);
        if (iter == salvage_info_cache.end() || !iter->is_object()) {
            Log::Info(std::format("[{}] No key found", key_str).c_str());
            return std::optional<SalvageInfo>();
        }

        try {
            SalvageInfo salvageInfo;
            from_json(iter.value(), salvageInfo);
            Log::Info(std::format("[{}] Found value. Returning", key_str).c_str());
            return salvageInfo;
        }
        catch (std::exception ex) {
            Log::Error(std::format("[{}] Exception when fetching salvage info for key. Details:{}", key_str, ex.what()).c_str());
            return std::optional<SalvageInfo>();
        }
    }

    void SaveToCache(std::wstring key, SalvageInfo value) {
        const auto key_str = GetBase64EncodedName(key);
        Log::Info(std::format("[{}] Saving key to cache", key_str).c_str());
        salvage_info_cache[key_str] = value;
    }

    std::string ConvertMaterialToString(const GW::ItemID item_id, const GW::Constants::Language language)
    {
        const auto lang_map_iter = material_names.find(language);
        if (lang_map_iter == material_names.end()) {
            return std::to_string((uint32_t)item_id);
        }

        const auto name_iter = lang_map_iter->second.find(item_id);
        if (name_iter == lang_map_iter->second.end()) {
            return std::to_string((uint32_t)item_id);
        }

        return name_iter->second;
    }

    std::string BuildItemListString(const std::vector<GW::ItemID>& items, const GW::Constants::Language language) {
        std::string itemListString = "";
        if (items.size() == 0) {
            return itemListString;
        }

        for (size_t i = 0; i < items.size() - 1; i++) {
            itemListString += std::format("{}, ", ConvertMaterialToString(items[i], language));
        }

        itemListString += ConvertMaterialToString(items[items.size() - 1], language);
        return itemListString;
    }

    std::vector<GW::ItemID> GetMaterials(std::string raw_materials_info) {
        // Raw string comes in like this: [[Pile of Glittering Dust|Piles of Glittering Dust]] <br> [[Wood Plank]]s
        // Strip '[' and ']'
        raw_materials_info.erase(
            std::remove_if(
                raw_materials_info.begin(), raw_materials_info.end(),
                [](char c) {
                    return c == '[' || c == ']';
                }
            ),
            raw_materials_info.end()
        );

        // Split materials by " <br> "
        const auto mats = Split(Replace(raw_materials_info, "\n", "<br>"), "<br>");
        
        // If the material info contains a '|', we want to split by that and choose the second option (plural)
        std::vector<std::string> final_mats;
        for (size_t i = 0; i < mats.size(); i++) {
            const auto tokens = Split(mats[i], "|");
            const auto final_mat = ToLowerString(Trim(tokens[tokens.size() - 1]));
            final_mats.push_back(final_mat);
        }

        std::vector<GW::ItemID> final_info;
        for (size_t i = 0; i < final_mats.size(); i++) {
            const auto &final_mat = final_mats[i];
            if (material_to_id_lookup.contains(final_mat)) {
                final_info.push_back(material_to_id_lookup[final_mat]);
            }
        }

        return final_info;
    }

    void ParseItemWikiResponse(const std::string response, SalvageInfo& salvage_info) {
        std::smatch matches;

        // Add materials to the list only if they're not already present (mostly the case with items with different pages per region)
        if (std::regex_search(response, matches, common_materials_regex) && matches.size() > 1) {
            const auto materials_list = GetMaterials(matches[1].str());
            for (size_t i = 0; i < materials_list.size(); i++) {
                if (std::find(salvage_info.crafting_materials.begin(), salvage_info.crafting_materials.end(), materials_list[i]) == salvage_info.crafting_materials.end()) {
                    salvage_info.crafting_materials.push_back(materials_list[i]);
                }
            }
        }

        if (std::regex_search(response, matches, rare_materials_regex) && matches.size() > 1) {
            const auto materials_list = GetMaterials(matches[1].str());
            for (size_t i = 0; i < materials_list.size(); i++) {
                if (std::find(salvage_info.rare_crafting_materials.begin(), salvage_info.rare_crafting_materials.end(), materials_list[i]) == salvage_info.rare_crafting_materials.end()) {
                    salvage_info.rare_crafting_materials.push_back(materials_list[i]);
                }
            }
        }
    }

    std::wstring FetchDescription(const GW::Item* item) {
        if (!item) {
            return L"";
        }

        const auto maybe_info = LoadFromCache(std::wstring(item->name_enc));
        // Could not find the item. We need to fetch the item name and then fetch the salvage info from wiki
        if (!maybe_info.has_value()) {
            fetch_queue.push(std::wstring(item->name_enc));
            return L"";
        }

        const auto language = GW::GetGameContext()->text_parser->language_id;
        const auto lang_map_iter = material_names.find(language);
        if (lang_map_iter == material_names.end()) {
            L"";
        }

        const auto salvage_info = maybe_info.value();
        std::wstring salvage_description;
        if (!salvage_info.crafting_materials.empty()) {
            const auto crafting_materials_str = GuiUtils::StringToWString(lang_map_iter->second.at(0));
            salvage_description += std::format(L"\x2\x108\x107\n<c={}>{}: {}</c>\x1", color, crafting_materials_str, GuiUtils::StringToWString(BuildItemListString(salvage_info.crafting_materials, language)));
        }

        if (!salvage_info.rare_crafting_materials.empty()) {
            const auto rare_crafting_materials_str = GuiUtils::StringToWString(lang_map_iter->second.at(1));
            salvage_description += std::format(L"\x2\x108\x107\n<c={}>{}: {}</c>\x1", color, rare_crafting_materials_str, GuiUtils::StringToWString(BuildItemListString(salvage_info.rare_crafting_materials, language)));
        }

        return salvage_description;
    }

    void UpdateDescription(const uint32_t item_id, wchar_t** description_out)
    {
        if (!(description_out && *description_out)) return;
        const auto item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));

        if (!item) return;

        if (item->IsUsable() || item->IsGreen() || item->IsArmor()) {
            return;
        }

        if ((item->type == GW::Constants::ItemType::Trophy && item->GetRarity() == GW::Constants::Rarity::White && item->info_string && item->is_material_salvageable) ||
            item->type == GW::Constants::ItemType::Salvage ||
            item->type == GW::Constants::ItemType::CC_Shards ||
            item->IsWeapon()) {
            modified_description = *description_out;
            modified_description += FetchDescription(item);
            *description_out = modified_description.data();
        }

        return;
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

void SalvageInfoModule::Initialize()
{
    ToolboxModule::Initialize();

    // Copied from GameSettings.cpp
    GetItemDescription_Func = (GetItemDescription_pt)GameSettings::OnGetItemDescription;
    if (GetItemDescription_Func) {
        ASSERT(GW::HookBase::CreateHook((void**)&GetItemDescription_Func, OnGetItemDescription, (void**)&GetItemDescription_Ret) == 0);
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }

    material_names[GW::Constants::Language::English] = english_material_names;
    material_names[GW::Constants::Language::Korean] = korean_material_names;
    material_names[GW::Constants::Language::French] = french_material_names;
    material_names[GW::Constants::Language::German] = german_material_names;
    material_names[GW::Constants::Language::Italian] = italian_material_names;
    material_names[GW::Constants::Language::Spanish] = spanish_material_names;
    material_names[GW::Constants::Language::TraditionalChinese] = traditionalchinese_material_names;
    material_names[GW::Constants::Language::Japanese] = japanese_material_names;
    material_names[GW::Constants::Language::Polish] = polish_material_names;
    material_names[GW::Constants::Language::Russian] = russian_material_names;
    material_names[GW::Constants::Language::BorkBorkBork] = borkborkbork_material_names;
    LoadLocalCache();
}

void SalvageInfoModule::Terminate()
{
    ToolboxModule::Terminate();

    if (GetItemDescription_Func) {
        GW::HookBase::DisableHooks(GetItemDescription_Func);
    }
}

void SalvageInfoModule::Update(const float)
{
    if (fetching_info) {
        return;
    }

    if (fetch_queue.empty()) {
        return;
    }

    const auto name_enc = fetch_queue.front();
    fetch_queue.pop();

    //Detect duplicate fetch requests and ignore them
    if (LoadFromCache(name_enc).has_value()) {
        return;
    }

    fetching_info = true;
    name_cache.clear();
    const auto base64_name = GetBase64EncodedName(name_enc);
    Log::Info(std::format("[{}] Fetching name", base64_name).c_str());
    // Async Get Name sometimes hangs if not run from the game thread
    GW::GameThread::Enqueue([name_enc] {
        AsyncGetItemSimpleName(name_enc, name_cache);
    });

    // Free the main thread and move the work on a background thread. When the work finishes, the main thread can process another item from the queue
    Resources::EnqueueWorkerTask([name_enc, base64_name] {
        while (name_cache.empty()) {
            Sleep(16);
        }

        Log::Info(std::format("[{}] Fetching crafting materials for {}", base64_name, GuiUtils::WStringToString(name_cache)).c_str());
        const auto item_url = GuiUtils::WikiTemplateUrlFromTitle(name_cache);

        Log::Info(std::format("[{}] Fetching crafting materials from {}", base64_name, item_url).c_str());
        std::string response;
        if (!Resources::Download(item_url, response)) {
            Log::Info(std::format("Failed to fetch item data from {}. Error: {}", item_url, response).c_str());
            // salvage_info_cache[std::to_string(item_id)] = ""; // Sometimes download fails for items
            fetching_info = false;
            return;
        }

        SalvageInfo salvage_info{};
        // Check for disambig wiki entry. In this case, fetch all crafting materials for all entries
        if (response.starts_with(disambig)) {
            Log::Info(std::format("[{}] Found disambig page. Fetching crafting materials for all entries", base64_name, item_url).c_str());
            std::smatch matches;
            while (std::regex_search(response, matches, disambig_entries_regex))
            {
                if (matches.size() > 0) {
                    Log::Info(std::format("[{}] Fetching crafting materials for {}", base64_name, matches[matches.size() - 1].str()).c_str());
                    const auto sub_item_url = GuiUtils::WikiTemplateUrlFromTitle(matches[matches.size() - 1].str());
                    std::string sub_response; // We need to keep the previous response so that we can process the disambig entries one by one
                    if (!Resources::Download(sub_item_url, sub_response)) {
                        Log::Info(std::format("Failed to fetch item data from {}. Error: {}", sub_item_url, sub_response).c_str());
                        continue;
                    }

                    ParseItemWikiResponse(sub_response, salvage_info);
                }

                response = matches.suffix().str();
            }
        }
        else {
            ParseItemWikiResponse(response, salvage_info);
        }

        Log::Info(std::format("[{}] Caching crafting materials", base64_name, item_url).c_str());
        SaveToCache(name_enc, salvage_info);
        SaveLocalCache();
        fetching_info = false;
    });
}

void SalvageInfoModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(fetch_salvage_info);
}

void SalvageInfoModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    LOAD_BOOL(fetch_salvage_info);
}

void SalvageInfoModule::RegisterSettingsContent()
{
    //ToolboxModule::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, const bool is_showing) {
            if (!is_showing) {
                return;
            }

            ImGui::Checkbox("Fetch salvage information for items", &fetch_salvage_info);
            ImGui::ShowHelp("When enabled, the item description will contain information about the crafting materials that can be salvaged from the item");
        },
        0.9f
    );
}
