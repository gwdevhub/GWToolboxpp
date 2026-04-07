#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <GWCA/Constants/Constants.h>

#include <Modules/InventoryManager.h>
#include <Modules/PriceCheckerModule.h>
#include <Modules/Resources.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <Timer.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

using nlohmann::json;

namespace {

    bool fetching_prices;
    const char* trader_quotes_url = "https://kamadan.gwtoolbox.com/trader_quotes";

    constexpr clock_t request_interval = 1000 * 60 * 5;
    clock_t last_request_time = -request_interval;
    std::unordered_map<std::string, uint32_t> prices_by_identifier;

    bool show_merchant_price_for_melandrus_accord_instead = true;

    // -------------------------------------------------------------------------
    // Price lookup tables
    // -------------------------------------------------------------------------

    struct PriceInfo {
        const char* name;
        const char* id;
    };

    static const std::unordered_map<uint32_t, PriceInfo> price_info_by_unique_mod_struct = {
        {0x25300423, {"Rune of Attunement", "08038225300423"}},
        {0x25300425, {"Rune of Vitae", "08038225300425"}},
        {0x27e802c2, {"Rune of Minor Vigor", "08038227e802c2"}},
        {0x21e80001, {"Mesmer Rune of Minor Fast Casting", "08038321e80001"}},
        {0x21e80101, {"Mesmer Rune of Minor Illusion Magic", "08038321e80101"}},
        {0x21e80201, {"Mesmer Rune of Minor Domination Magic", "08038321e80201"}},
        {0x21e80301, {"Mesmer Rune of Minor Inspiration Magic", "08038321e80301"}},
        {0x21e80401, {"Necromancer Rune of Minor Blood Magic", "08038421e80401"}},
        {0x21e80501, {"Necromancer Rune of Minor Death Magic", "08038421e80501"}},
        {0x21e80601, {"Necromancer Rune of Minor Soul Reaping", "08038421e80601"}},
        {0x21e80701, {"Necromancer Rune of Minor Curses", "08038421e80701"}},
        {0x21e80801, {"Elementalist Rune of Minor Air Magic", "08038521e80801"}},
        {0x21e80901, {"Elementalist Rune of Minor Earth Magic", "08038521e80901"}},
        {0x21e80a01, {"Elementalist Rune of Minor Fire Magic", "08038521e80a01"}},
        {0x21e80b01, {"Elementalist Rune of Minor Water Magic", "08038521e80b01"}},
        {0x21e80c01, {"Elementalist Rune of Minor Energy Storage", "08038521e80c01"}},
        {0x21e80d01, {"Monk Rune of Minor Healing Prayers", "08038621e80d01"}},
        {0x21e80e01, {"Monk Rune of Minor Smiting Prayers", "08038621e80e01"}},
        {0x21e80f01, {"Monk Rune of Minor Protection Prayers", "08038621e80f01"}},
        {0x21e81001, {"Monk Rune of Minor Divine Favor", "08038621e81001"}},
        {0x21e81101, {"Warrior Rune of Minor Strength", "08038721e81101"}},
        {0x21e81201, {"Warrior Rune of Minor Axe Mastery", "08038721e81201"}},
        {0x21e81301, {"Warrior Rune of Minor Hammer Mastery", "08038721e81301"}},
        {0x21e81401, {"Warrior Rune of Minor Swordsmanship", "08038721e81401"}},
        {0x21e81501, {"Warrior Rune of Minor Tactics", "08038721e81501"}},
        {0x27e802ea, {"Warrior Rune of Minor Absorption", "08038727e802ea"}},
        {0x21e81601, {"Ranger Rune of Minor Beast Mastery", "08038821e81601"}},
        {0x21e81701, {"Ranger Rune of Minor Expertise", "08038821e81701"}},
        {0x21e81801, {"Ranger Rune of Minor Wilderness Survival", "08038821e81801"}},
        {0x21e81901, {"Ranger Rune of Minor Marksmanship", "08038821e81901"}},
        {0x21e80002, {"Mesmer Rune of Major Fast Casting", "080e1c21e80002"}},
        {0x21e80102, {"Mesmer Rune of Major Illusion Magic", "080e1c21e80102"}},
        {0x21e80202, {"Mesmer Rune of Major Domination Magic", "080e1c21e80202"}},
        {0x21e80302, {"Mesmer Rune of Major Inspiration Magic", "080e1c21e80302"}},
        {0x21e80003, {"Mesmer Rune of Superior Fast Casting", "0815ad21e80003"}},
        {0x21e80103, {"Mesmer Rune of Superior Illusion Magic", "0815ad21e80103"}},
        {0x21e80203, {"Mesmer Rune of Superior Domination Magic", "0815ad21e80203"}},
        {0x21e80303, {"Mesmer Rune of Superior Inspiration Magic", "0815ad21e80303"}},
        {0x25300427, {"Rune of Recovery", "0815ae25300427"}},
        {0x25300429, {"Rune of Restoration", "0815ae25300429"}},
        {0x2530042b, {"Rune of Clarity", "0815ae2530042b"}},
        {0x2530042d, {"Rune of Purity", "0815ae2530042d"}},
        {0x27e902c2, {"Rune of Major Vigor", "0815ae27e902c2"}},
        {0x27ea02c2, {"Rune of Superior Vigor", "0815af27ea02c2"}},
        {0x21e80402, {"Necromancer Rune of Major Blood Magic", "0815b021e80402"}},
        {0x21e80502, {"Necromancer Rune of Major Death Magic", "0815b021e80502"}},
        {0x21e80602, {"Necromancer Rune of Major Soul Reaping", "0815b021e80602"}},
        {0x21e80702, {"Necromancer Rune of Major Curses", "0815b021e80702"}},
        {0x21e80403, {"Necromancer Rune of Superior Blood Magic", "0815b121e80403"}},
        {0x21e80503, {"Necromancer Rune of Superior Death Magic", "0815b121e80503"}},
        {0x21e80603, {"Necromancer Rune of Superior Soul Reaping", "0815b121e80603"}},
        {0x21e80703, {"Necromancer Rune of Superior Curses", "0815b121e80703"}},
        {0x21e80802, {"Elementalist Rune of Major Air Magic", "0815b221e80802"}},
        {0x21e80902, {"Elementalist Rune of Major Earth Magic", "0815b221e80902"}},
        {0x21e80a02, {"Elementalist Rune of Major Fire Magic", "0815b221e80a02"}},
        {0x21e80b02, {"Elementalist Rune of Major Water Magic", "0815b221e80b02"}},
        {0x21e80c02, {"Elementalist Rune of Major Energy Storage", "0815b221e80c02"}},
        {0x21e80803, {"Elementalist Rune of Superior Air Magic", "0815b321e80803"}},
        {0x21e80903, {"Elementalist Rune of Superior Earth Magic", "0815b321e80903"}},
        {0x21e80a03, {"Elementalist Rune of Superior Fire Magic", "0815b321e80a03"}},
        {0x21e80b03, {"Elementalist Rune of Superior Water Magic", "0815b321e80b03"}},
        {0x21e80c03, {"Elementalist Rune of Superior Energy Storage", "0815b321e80c03"}},
        {0x21e80d02, {"Monk Rune of Major Healing Prayers", "0815b421e80d02"}},
        {0x21e80e02, {"Monk Rune of Major Smiting Prayers", "0815b421e80e02"}},
        {0x21e80f02, {"Monk Rune of Major Protection Prayers", "0815b421e80f02"}},
        {0x21e81002, {"Monk Rune of Major Divine Favor", "0815b421e81002"}},
        {0x21e80d03, {"Monk Rune of Superior Healing Prayers", "0815b521e80d03"}},
        {0x21e80e03, {"Monk Rune of Superior Smiting Prayers", "0815b521e80e03"}},
        {0x21e80f03, {"Monk Rune of Superior Protection Prayers", "0815b521e80f03"}},
        {0x21e81003, {"Monk Rune of Superior Divine Favor", "0815b521e81003"}},
        {0x21e81102, {"Warrior Rune of Major Strength", "0815b621e81102"}},
        {0x21e81202, {"Warrior Rune of Major Axe Mastery", "0815b621e81202"}},
        {0x21e81302, {"Warrior Rune of Major Hammer Mastery", "0815b621e81302"}},
        {0x21e81402, {"Warrior Rune of Major Swordsmanship", "0815b621e81402"}},
        {0x21e81502, {"Warrior Rune of Major Tactics", "0815b621e81502"}},
        {0x27e902ea, {"Warrior Rune of Major Absorption", "0815b627e902ea"}},
        {0x21e81103, {"Warrior Rune of Superior Strength", "0815b721e81103"}},
        {0x21e81203, {"Warrior Rune of Superior Axe Mastery", "0815b721e81203"}},
        {0x21e81303, {"Warrior Rune of Superior Hammer Mastery", "0815b721e81303"}},
        {0x21e81403, {"Warrior Rune of Superior Swordsmanship", "0815b721e81403"}},
        {0x21e81503, {"Warrior Rune of Superior Tactics", "0815b721e81503"}},
        {0x27ea02ea, {"Warrior Rune of Superior Absorption", "0815b727ea02ea"}},
        {0x21e81602, {"Ranger Rune of Major Beast Mastery", "0815b821e81602"}},
        {0x21e81702, {"Ranger Rune of Major Expertise", "0815b821e81702"}},
        {0x21e81802, {"Ranger Rune of Major Wilderness Survival", "0815b821e81802"}},
        {0x21e81902, {"Ranger Rune of Major Marksmanship", "0815b821e81902"}},
        {0x21e81603, {"Ranger Rune of Superior Beast Mastery", "0815b921e81603"}},
        {0x21e81703, {"Ranger Rune of Superior Expertise", "0815b921e81703"}},
        {0x21e81803, {"Ranger Rune of Superior Wilderness Survival", "0815b921e81803"}},
        {0x21e81903, {"Ranger Rune of Superior Marksmanship", "0815b921e81903"}},
        {0x21e81d01, {"Assassin Rune of Minor Dagger Mastery", "0818b421e81d01"}},
        {0x21e81e01, {"Assassin Rune of Minor Deadly Arts", "0818b421e81e01"}},
        {0x21e81f01, {"Assassin Rune of Minor Shadow Arts", "0818b421e81f01"}},
        {0x21e82301, {"Assassin Rune of Minor Critical Strikes", "0818b421e82301"}},
        {0x21e81d02, {"Assassin Rune of Major Dagger Mastery", "0818b521e81d02"}},
        {0x21e81e02, {"Assassin Rune of Major Deadly Arts", "0818b521e81e02"}},
        {0x21e81f02, {"Assassin Rune of Major Shadow Arts", "0818b521e81f02"}},
        {0x21e82302, {"Assassin Rune of Major Critical Strikes", "0818b521e82302"}},
        {0x21e81d03, {"Assassin Rune of Superior Dagger Mastery", "0818b621e81d03"}},
        {0x21e81e03, {"Assassin Rune of Superior Deadly Arts", "0818b621e81e03"}},
        {0x21e81f03, {"Assassin Rune of Superior Shadow Arts", "0818b621e81f03"}},
        {0x21e82303, {"Assassin Rune of Superior Critical Strikes", "0818b621e82303"}},
        {0x21e82001, {"Ritualist Rune of Minor Communing", "0818b721e82001"}},
        {0x21e82101, {"Ritualist Rune of Minor Restoration Magic", "0818b721e82101"}},
        {0x21e82201, {"Ritualist Rune of Minor Channeling Magic", "0818b721e82201"}},
        {0x21e82401, {"Ritualist Rune of Minor Spawning Power", "0818b721e82401"}},
        {0x21e82002, {"Ritualist Rune of Major Communing", "0818b821e82002"}},
        {0x21e82102, {"Ritualist Rune of Major Restoration Magic", "0818b821e82102"}},
        {0x21e82202, {"Ritualist Rune of Major Channeling Magic", "0818b821e82202"}},
        {0x21e82402, {"Ritualist Rune of Major Spawning Power", "0818b821e82402"}},
        {0x21e82003, {"Ritualist Rune of Superior Communing", "0818b921e82003"}},
        {0x21e82103, {"Ritualist Rune of Superior Restoration Magic", "0818b921e82103"}},
        {0x21e82203, {"Ritualist Rune of Superior Channeling Magic", "0818b921e82203"}},
        {0x21e82403, {"Ritualist Rune of Superior Spawning Power", "0818b921e82403"}},
        {0x21e82901, {"Dervish Rune of Minor Scythe Mastery", "083cb921e82901"}},
        {0x21e82a01, {"Dervish Rune of Minor Wind Prayers", "083cb921e82a01"}},
        {0x21e82b01, {"Dervish Rune of Minor Earth Prayers", "083cb921e82b01"}},
        {0x21e82c01, {"Dervish Rune of Minor Mysticism", "083cb921e82c01"}},
        {0x21e82902, {"Dervish Rune of Major Scythe Mastery", "083cba21e82902"}},
        {0x21e82a02, {"Dervish Rune of Major Wind Prayers", "083cba21e82a02"}},
        {0x21e82b02, {"Dervish Rune of Major Earth Prayers", "083cba21e82b02"}},
        {0x21e82c02, {"Dervish Rune of Major Mysticism", "083cba21e82c02"}},
        {0x21e82903, {"Dervish Rune of Superior Scythe Mastery", "083cbb21e82903"}},
        {0x21e82a03, {"Dervish Rune of Superior Wind Prayers", "083cbb21e82a03"}},
        {0x21e82b03, {"Dervish Rune of Superior Earth Prayers", "083cbb21e82b03"}},
        {0x21e82c03, {"Dervish Rune of Superior Mysticism", "083cbb21e82c03"}},
        {0x21e82501, {"Paragon Rune of Minor Spear Mastery", "083cbc21e82501"}},
        {0x21e82601, {"Paragon Rune of Minor Command", "083cbc21e82601"}},
        {0x21e82701, {"Paragon Rune of Minor Motivation", "083cbc21e82701"}},
        {0x21e82801, {"Paragon Rune of Minor Leadership", "083cbc21e82801"}},
        {0x21e82502, {"Paragon Rune of Major Spear Mastery", "083cbd21e82502"}},
        {0x21e82602, {"Paragon Rune of Major Command", "083cbd21e82602"}},
        {0x21e82702, {"Paragon Rune of Major Motivation", "083cbd21e82702"}},
        {0x21e82802, {"Paragon Rune of Major Leadership", "083cbd21e82802"}},
        {0x21e82503, {"Paragon Rune of Superior Spear Mastery", "083cbe21e82503"}},
        {0x21e82603, {"Paragon Rune of Superior Command", "083cbe21e82603"}},
        {0x21e82703, {"Paragon Rune of Superior Motivation", "083cbe21e82703"}},
        {0x21e82803, {"Paragon Rune of Superior Leadership", "083cbe21e82803"}},
        {0xa53003bc, {"Vanguard's Insignia [Assassin]", "084ab4a53003bc"}},
        {0xa53003be, {"Infiltrator's Insignia [Assassin]", "084ab5a53003be"}},
        {0xa53003c0, {"Saboteur's Insignia [Assassin]", "084ab6a53003c0"}},
        {0xa53003c2, {"Nightstalker's Insignia [Assassin]", "084ab7a53003c2"}},
        {0xa53003c4, {"Artificer's Insignia [Mesmer]", "084ab8a53003c4"}},
        {0xa53003c6, {"Prodigy's Insignia [Mesmer]", "084ab9a53003c6"}},
        {0xa53003c8, {"Virtuoso's Insignia [Mesmer]", "084abaa53003c8"}},
        {0x253003ca, {"Radiant Insignia", "084abb253003ca"}},
        {0x253003cc, {"Survivor Insignia", "084abc253003cc"}},
        {0xa53003ce, {"Stalwart Insignia", "084abda53003ce"}},
        {0xa53003d0, {"Brawler's Insignia", "084abea53003d0"}},
        {0xa53003d2, {"Blessed Insignia", "084abfa53003d2"}},
        {0xa53003d4, {"Herald's Insignia", "084ac0a53003d4"}},
        {0xa53003d6, {"Sentry's Insignia", "084ac1a53003d6"}},
        {0x27e802a9, {"Bloodstained Insignia [Necromancer]", "084ac227e802a9"}},
        {0x253003d8, {"Tormentor's Insignia [Necromancer]", "084ac3253003d8"}},
        {0xa53003da, {"Undertaker's Insignia [Necromancer]", "084ac4a53003da"}},
        {0xa53003dc, {"Bonelace Insignia [Necromancer]", "084ac5a53003dc"}},
        {0xa53003de, {"Minion Master's Insignia [Necromancer]", "084ac6a53003de"}},
        {0xa53003e0, {"Blighter's Insignia [Necromancer]", "084ac7a53003e0"}},
        {0xa53003e2, {"Prismatic Insignia [Elementalist]", "084ac8a53003e2"}},
        {0xa53003e4, {"Hydromancer Insignia [Elementalist]", "084ac9a53003e4"}},
        {0xa53003e6, {"Geomancer Insignia [Elementalist]", "084acaa53003e6"}},
        {0xa53003e8, {"Pyromancer Insignia [Elementalist]", "084acba53003e8"}},
        {0xa53003ea, {"Aeromancer Insignia [Elementalist]", "084acca53003ea"}},
        {0xa53003ec, {"Wanderer's Insignia [Monk]", "084acda53003ec"}},
        {0xa53003ee, {"Disciple's Insignia [Monk]", "084acea53003ee"}},
        {0xa53003f0, {"Anchorite's Insignia [Monk]", "084acfa53003f0"}},
        {0xa53003f2, {"Knight's Insignia [Warrior]", "084ad0a53003f2"}},
        {0x27e802b6, {"Lieutenant's Insignia [Warrior]", "084ad127e802b6"}},
        {0x27e802b7, {"Stonefist Insignia [Warrior]", "084ad227e802b7"}},
        {0xa53003f4, {"Dreadnought Insignia [Warrior]", "084ad3a53003f4"}},
        {0xa53003f6, {"Sentinel's Insignia [Warrior]", "084ad4a53003f6"}},
        {0xa53003f8, {"Frostbound Insignia [Ranger]", "084ad5a53003f8"}},
        {0xa53003fa, {"Earthbound Insignia [Ranger]", "084ad6a53003fa"}},
        {0xa53003fc, {"Pyrebound Insignia [Ranger]", "084ad7a53003fc"}},
        {0xa53003fe, {"Stormbound Insignia [Ranger]", "084ad8a53003fe"}},
        {0xa5300400, {"Beastmaster's Insignia [Ranger]", "084ad9a5300400"}},
        {0xa5300402, {"Scout's Insignia [Ranger]", "084adaa5300402"}},
        {0xa5300404, {"Windwalker Insignia [Dervish]", "084adba5300404"}},
        {0xa5300406, {"Forsaken Insignia [Dervish]", "084adca5300406"}},
        {0xa5300408, {"Shaman's Insignia [Ritualist]", "084adda5300408"}},
        {0xa530040a, {"Ghost Forge Insignia [Ritualist]", "084adea530040a"}},
        {0xa530040c, {"Mystic's Insignia [Ritualist]", "084adfa530040c"}},
        {0xa530040e, {"Centurion's Insignia [Paragon]", "084ae0a530040e"}},
        {0x24d00201, {"Vial of Dye [Blue]", "0a009224d00201"}},
        {0x24d00301, {"Vial of Dye [Green]", "0a009224d00301"}},
        {0x24d00401, {"Vial of Dye [Purple]", "0a009224d00401"}},
        {0x24d00501, {"Vial of Dye [Red]", "0a009224d00501"}},
        {0x24d00601, {"Vial of Dye [Yellow]", "0a009224d00601"}},
        {0x24d00701, {"Vial of Dye [Brown]", "0a009224d00701"}},
        {0x24d00801, {"Vial of Dye [Orange]", "0a009224d00801"}},
        {0x24d00901, {"Vial of Dye [Silver]", "0a009224d00901"}},
        {0x24d00a01, {"Vial of Dye [Black]", "0a009224d00a01"}},
        {0x24d00c01, {"Vial of Dye [White]", "0a009224d00c01"}},
        {0x24d00d01, {"Vial of Dye [Pink]", "0a009224d00d01"}},
        {0x000b0399, {"Bone", "0b0399"}},
        {0x000b039a, {"Lump of Charcoal", "0b039a"}},
        {0x000b039b, {"Monstrous Claw", "0b039b"}},
        {0x000b039d, {"Bolt of Cloth", "0b039d"}},
        {0x000b039e, {"Bolt of Linen", "0b039e"}},
        {0x000b039f, {"Bolt of Damask", "0b039f"}},
        {0x000b03a0, {"Bolt of Silk", "0b03a0"}},
        {0x000b03a1, {"Pile of Glittering Dust", "0b03a1"}},
        {0x000b03a2, {"Glob of Ectoplasm", "0b03a2"}},
        {0x000b03a3, {"Monstrous Eye", "0b03a3"}},
        {0x000b03a4, {"Monstrous Fang", "0b03a4"}},
        {0x000b03a5, {"Feather", "0b03a5"}},
        {0x000b03a6, {"Plant Fiber", "0b03a6"}},
        {0x000b03a7, {"Diamond", "0b03a7"}},
        {0x000b03a8, {"Onyx Gemstone", "0b03a8"}},
        {0x000b03a9, {"Ruby", "0b03a9"}},
        {0x000b03aa, {"Sapphire", "0b03aa"}},
        {0x000b03ab, {"Tempered Glass Vial", "0b03ab"}},
        {0x000b03ac, {"Tanned Hide Square", "0b03ac"}},
        {0x000b03ad, {"Fur Square", "0b03ad"}},
        {0x000b03ae, {"Leather Square", "0b03ae"}},
        {0x000b03af, {"Elonian Leather Square", "0b03af"}},
        {0x000b03b0, {"Vial of Ink", "0b03b0"}},
        {0x000b03b1, {"Obsidian Shard", "0b03b1"}},
        {0x000b03b2, {"Wood Plank", "0b03b2"}},
        {0x000b03b4, {"Iron Ingot", "0b03b4"}},
        {0x000b03b5, {"Steel Ingot", "0b03b5"}},
        {0x000b03b6, {"Deldrimor Steel Ingot", "0b03b6"}},
        {0x000b03b7, {"Roll of Parchment", "0b03b7"}},
        {0x000b03b8, {"Roll of Vellum", "0b03b8"}},
        {0x000b03b9, {"Scale", "0b03b9"}},
        {0x000b03ba, {"Chitin Fragment", "0b03ba"}},
        {0x000b03bb, {"Granite Slab", "0b03bb"}},
        {0x000b03bc, {"Spiritwood Plank", "0b03bc"}},
        {0x000b1984, {"Amber Chunk", "0b1984"}},
        {0x000b1985, {"Jadeite Shard", "0b1985"}},
    };

    int MaterialSlotToModelID(GW::Constants::MaterialSlot mat)
    {
        const auto info = GW::Items::GetMaterialInfo(mat);
        return info ? info->model_id : 0;
    }

    GW::Constants::MaterialSlot GetItemMaterialSlot(const GW::Item* item)
    {
        if (!item) return static_cast<GW::Constants::MaterialSlot>(0xff);
        const auto mod = static_cast<const InventoryManager::Item*>(item)->GetModifier(0x2508);
        if (!mod) return static_cast<GW::Constants::MaterialSlot>(0xff);
        return static_cast<GW::Constants::MaterialSlot>(mod->arg1());
    }

    uint32_t GetPriceById(const char* id)
    {
        const auto& prices = PriceCheckerModule::FetchPrices();
        const auto found = prices.find(id);
        return found != prices.end() ? found->second : 0u;
    }

    bool ParsePriceJson(const std::string& prices_json_str)
    {
        const json& prices_json = json::parse(prices_json_str, nullptr, false);
        if (prices_json == json::value_t::discarded) return false;

        prices_by_identifier.clear();
        const auto& it_buy = prices_json.find("sell");
        if (it_buy == prices_json.end() || !it_buy->is_object()) return false;

        for (auto it = it_buy.value().begin(); it != it_buy.value().end(); ++it) {
            if (!it->is_object()) continue;
            const auto& price_value = it->find("p");
            if (price_value == it->end() || !price_value->is_number_unsigned()) continue;
            prices_by_identifier[it.key()] = price_value->get<uint32_t>();
        }
        return !prices_by_identifier.empty();
    }

    void SignalItemDescriptionUpdated()
    {
        GW::GameThread::Enqueue([] {
            const auto item = GW::Items::GetHoveredItem();
            if (!item) return;
            GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, item);
        });
    }

} // namespace

void PriceCheckerModule::Initialize()
{
    ToolboxModule::Initialize();
    FetchPrices();
}

void PriceCheckerModule::Terminate()
{
    ToolboxModule::Terminate();
}

void PriceCheckerModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(show_merchant_price_for_melandrus_accord_instead);
}

void PriceCheckerModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(show_merchant_price_for_melandrus_accord_instead);
}

void PriceCheckerModule::DrawSettingsInternal()
{
    ImGui::Checkbox("Show merchant price for Melandru's Accord instead of trader price", &show_merchant_price_for_melandrus_accord_instead);
    ImGui::ShowHelp("In Melandru's Accord, show fixed merchant prices for materials instead of live trader prices");
}

const std::unordered_map<std::string, uint32_t>& PriceCheckerModule::FetchPrices()
{
    if (TIMER_DIFF(last_request_time) > request_interval) {
        last_request_time = TIMER_INIT();
        Resources::Download(trader_quotes_url, [](bool success, const std::string& response, void*) {
            if (!success) {
                last_request_time -= request_interval;
                return;
            }
            ParsePriceJson(response);
            SignalItemDescriptionUpdated();
        });
    }
    return prices_by_identifier;
}

uint32_t PriceCheckerModule::GetTraderSellPrice(const GW::Item* item)
{
    return GetPriceByItem(item);
}

uint32_t PriceCheckerModule::GetTraderSellPrice(const GW::Constants::MaterialSlot material)
{
    if (GW::PlayerMgr::IsMelandrusAccord() && show_merchant_price_for_melandrus_accord_instead) return GetMerchantSellPrice(material);
    GW::Item item;
    memset(&item, 0, sizeof(item));
    item.type = GW::Constants::ItemType::Materials_Zcoins;
    item.model_id = MaterialSlotToModelID(material);
    return GetTraderSellPrice(&item);
}

uint32_t PriceCheckerModule::GetMerchantSellPrice(const GW::Constants::MaterialSlot material)
{
    using namespace GW::Constants;
    if (material == MaterialSlot::IronIngot) return 5;
    if (material == MaterialSlot::WoodPlank) return 4;
    if (material <= MaterialSlot::Feather) return 3;
    switch (material) {
        case MaterialSlot::Sapphire:
        case MaterialSlot::Ruby:
        case MaterialSlot::Diamond:
        case MaterialSlot::OnyxGemstone:
            return 250;
        case MaterialSlot::AmberChunk:
        case MaterialSlot::JadeiteShard:
        case MaterialSlot::GlobofEctoplasm:
        case MaterialSlot::MonstrousEye:
        case MaterialSlot::MonstrousClaw:
        case MaterialSlot::MonstrousFang:
        case MaterialSlot::ObsidianShard:
            return 100;
        case MaterialSlot::RollofParchment:
        case MaterialSlot::RollofVellum:
        case MaterialSlot::TemperedGlassVial:
        case MaterialSlot::VialofInk:
        case MaterialSlot::SpiritwoodPlank:
            return 20;
        case MaterialSlot::BoltofDamask:
        case MaterialSlot::BoltofLinen:
        case MaterialSlot::BoltofSilk:
            return 15;
        case MaterialSlot::FurSquare:
            return 10;
    }
    return 30;
}

uint32_t PriceCheckerModule::GetPriceByItem(const GW::Item* item, std::string* item_name_out, unsigned int mod_start_index)
{
    if (item->type == GW::Constants::ItemType::Materials_Zcoins) {
        uint32_t mod = (std::to_underlying(item->type) << 16) | (item->model_id & 0xffff);
        const auto found = price_info_by_unique_mod_struct.find(mod);
        if (found == price_info_by_unique_mod_struct.end()) return 0;
        if (GW::PlayerMgr::IsMelandrusAccord() && show_merchant_price_for_melandrus_accord_instead) return GetMerchantSellPrice(GetItemMaterialSlot(item));
        return GetPriceById(found->second.id);
    }

    size_t found_count = 0;
    for (size_t i = 0; i < item->mod_struct_size; i++) {
        const auto mod = item->mod_struct[i].mod;
        const auto found = price_info_by_unique_mod_struct.find(mod);
        if (found == price_info_by_unique_mod_struct.end()) continue;
        if (found_count == mod_start_index) {
            if (item_name_out) *item_name_out = found->second.name;
            return GetPriceById(found->second.id);
        }
        found_count++;
    }
    return 0;
}
