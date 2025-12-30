#include "stdafx.h"
#include <chrono>
#include <iomanip>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include <Modules/ItemDrops.h>
#include <Modules/Resources.h>

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);
#define MAP_ENTRY(var) \
    {                  \
        var, #var      \
    }

namespace {
    struct ItemOwner {
        GW::ItemID item;
        GW::AgentID owner;
    };

    struct ParsedItemInfo {
        std::wstring damage_type;
        uint16_t min_damage = 0;
        uint16_t max_damage = 0;
        std::wstring requirement_attribute;
        uint8_t requirement_value = 0;
    };

    using ItemModelID = decltype(GW::Item::model_id);
    std::vector<GW::Packet::StoC::AgentAdd> suppressed_packets{};
    std::vector<ItemOwner> item_owners{};

    bool hide_player_white = false;
    bool hide_player_blue = false;
    bool hide_player_purple = false;
    bool hide_player_gold = false;
    bool hide_player_green = false;
    bool hide_party_white = false;
    bool hide_party_blue = false;
    bool hide_party_purple = false;
    bool hide_party_gold = false;
    bool hide_party_green = false;
    bool track_drops = false;
    std::map<ItemModelID, std::string> dont_hide_for_player{};
    std::map<ItemModelID, std::string> dont_hide_for_party{};

    void OnAgentAdd(GW::HookStatus*, const GW::Packet::StoC::AgentAdd*);
    void OnAgentRemove(GW::HookStatus*, GW::Packet::StoC::AgentRemove*);
    void OnMapLoad(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);
    void OnItemReuseId(GW::HookStatus*, GW::Packet::StoC::ItemGeneral_ReuseID*);
    void OnItemUpdateOwner(GW::HookStatus*, GW::Packet::StoC::ItemUpdateOwner*);

    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnAgentAdd_Entry;
    GW::HookEntry OnAgentRemove_Entry;
    GW::HookEntry OnMapLoad_Entry;
    GW::HookEntry OnItemReuseId_Entry;
    GW::HookEntry OnItemUpdateOwner_Entry;


    enum class Rarity : uint8_t { White, Blue, Purple, Gold, Green, Unknown };

    using namespace GW::Constants::ItemID;

    Rarity GetRarity(const GW::Item& item)
    {
        if (item.complete_name_enc == nullptr) {
            return Rarity::Unknown;
        }

        switch (item.complete_name_enc[0]) {
            case 2621:
                return Rarity::White;
            case 2623:
                return Rarity::Blue;
            case 2626:
                return Rarity::Purple;
            case 2624:
                return Rarity::Gold;
            case 2627:
                return Rarity::Green;
            default:
                return Rarity::Unknown;
        }
    }

    const wchar_t* GetRarityString(const Rarity rarity)
    {
        switch (rarity) {
            case Rarity::White:
                return L"White";
            case Rarity::Blue:
                return L"Blue";
            case Rarity::Purple:
                return L"Purple";
            case Rarity::Gold:
                return L"Gold";
            case Rarity::Green:
                return L"Green";
            case Rarity::Unknown:
            default:
                return L"Unknown";
        }
    }

    const wchar_t* GetItemTypeString(const GW::Constants::ItemType type)
    {
        switch (type) {
            case GW::Constants::ItemType::Salvage:
                return L"Salvage";
            case GW::Constants::ItemType::Axe:
                return L"Axe";
            case GW::Constants::ItemType::Bag:
                return L"Bag";
            case GW::Constants::ItemType::Boots:
                return L"Boots";
            case GW::Constants::ItemType::Bow:
                return L"Bow";
            case GW::Constants::ItemType::Bundle:
                return L"Bundle";
            case GW::Constants::ItemType::Chestpiece:
                return L"Chestpiece";
            case GW::Constants::ItemType::Rune_Mod:
                return L"Rune_Mod";
            case GW::Constants::ItemType::Usable:
                return L"Usable";
            case GW::Constants::ItemType::Dye:
                return L"Dye";
            case GW::Constants::ItemType::Materials_Zcoins:
                return L"Materials_Zcoins";
            case GW::Constants::ItemType::Offhand:
                return L"Offhand";
            case GW::Constants::ItemType::Gloves:
                return L"Gloves";
            case GW::Constants::ItemType::Hammer:
                return L"Hammer";
            case GW::Constants::ItemType::Headpiece:
                return L"Headpiece";
            case GW::Constants::ItemType::CC_Shards:
                return L"CC_Shards";
            case GW::Constants::ItemType::Key:
                return L"Key";
            case GW::Constants::ItemType::Leggings:
                return L"Leggings";
            case GW::Constants::ItemType::Gold_Coin:
                return L"Gold_Coin";
            case GW::Constants::ItemType::Quest_Item:
                return L"Quest_Item";
            case GW::Constants::ItemType::Wand:
                return L"Wand";
            case GW::Constants::ItemType::Shield:
                return L"Shield";
            case GW::Constants::ItemType::Staff:
                return L"Staff";
            case GW::Constants::ItemType::Sword:
                return L"Sword";
            case GW::Constants::ItemType::Kit:
                return L"Kit";
            case GW::Constants::ItemType::Trophy:
                return L"Trophy";
            case GW::Constants::ItemType::Scroll:
                return L"Scroll";
            case GW::Constants::ItemType::Daggers:
                return L"Daggers";
            case GW::Constants::ItemType::Present:
                return L"Present";
            case GW::Constants::ItemType::Minipet:
                return L"Minipet";
            case GW::Constants::ItemType::Scythe:
                return L"Scythe";
            case GW::Constants::ItemType::Spear:
                return L"Spear";
            case GW::Constants::ItemType::Storybook:
                return L"Storybook";
            case GW::Constants::ItemType::Costume:
                return L"Costume";
            case GW::Constants::ItemType::Costume_Headpiece:
                return L"Costume_Headpiece";
            case GW::Constants::ItemType::Unknown:
            default:
                return L"Unknown";
        }
    }

    const GW::Item* GetItemFromPacket(const GW::Packet::StoC::AgentAdd& packet)
    {
        // filter non-item-agents
        if (packet.type != 4 || packet.unk3 != 0) {
            return nullptr;
        }
        return GW::Items::GetItemById(packet.agent_type);
    }

    const std::map<ItemModelID, std::string> default_dont_hide_for_player = {
        // Weapons
        MAP_ENTRY(DSR),
        MAP_ENTRY(EternalBlade),
        MAP_ENTRY(VoltaicSpear),
        MAP_ENTRY(CrystallineSword),
        MAP_ENTRY(ObsidianEdge),

        // Rare and Valuable Items
        MAP_ENTRY(MiniDhuum),
        MAP_ENTRY(ArmbraceOfTruth),
        MAP_ENTRY(MargoniteGem),
        MAP_ENTRY(StygianGem),
        MAP_ENTRY(TitanGem),
        MAP_ENTRY(TormentGem),

        // Crafting Items
        MAP_ENTRY(Diamond),
        MAP_ENTRY(Ruby),
        MAP_ENTRY(Sapphire),
        MAP_ENTRY(GlobofEctoplasm),
        MAP_ENTRY(ObsidianShard),

        // Consumables
        MAP_ENTRY(Cupcakes),
        MAP_ENTRY(Apples),
        MAP_ENTRY(Corns),
        MAP_ENTRY(Pies),
        MAP_ENTRY(Eggs),
        MAP_ENTRY(Warsupplies),
        MAP_ENTRY(SkalefinSoup),
        MAP_ENTRY(PahnaiSalad),
        MAP_ENTRY(Kabobs),
        MAP_ENTRY(PumpkinCookie),

        // Rock Candy
        MAP_ENTRY(GRC),
        MAP_ENTRY(BRC),
        MAP_ENTRY(RRC),

        // Conset
        MAP_ENTRY(ConsEssence),
        MAP_ENTRY(ConsArmor),
        MAP_ENTRY(ConsGrail),

        // Lunars
        MAP_ENTRY(LunarDragon),
        MAP_ENTRY(LunarHorse),
        MAP_ENTRY(LunarMonkey),
        MAP_ENTRY(LunarOx),
        MAP_ENTRY(LunarRabbit),
        MAP_ENTRY(LunarRat),
        MAP_ENTRY(LunarRooster),
        MAP_ENTRY(LunarSheep),
        MAP_ENTRY(LunarSnake),
        MAP_ENTRY(LunarTiger),
        MAP_ENTRY(LunarDog),
        MAP_ENTRY(LunarPig),
        MAP_ENTRY(LunarMonkey),

        // Alcohol
        MAP_ENTRY(Absinthe),
        MAP_ENTRY(AgedDwarvenAle),
        MAP_ENTRY(AgedHuntersAle),
        MAP_ENTRY(BottleOfJuniberryGin),
        MAP_ENTRY(BottleOfVabbianWine),
        MAP_ENTRY(Cider),
        MAP_ENTRY(DwarvenAle),
        MAP_ENTRY(Eggnog),
        MAP_ENTRY(FlaskOfFirewater),
        MAP_ENTRY(Grog),
        MAP_ENTRY(HuntersAle),
        MAP_ENTRY(Keg),
        MAP_ENTRY(KrytanBrandy),
        MAP_ENTRY(Ricewine),
        MAP_ENTRY(ShamrockAle),
        MAP_ENTRY(SpikedEggnog),
        MAP_ENTRY(WitchsBrew),

        // Summons
        MAP_ENTRY(GhastlyStone),
        MAP_ENTRY(GakiSummon),
        MAP_ENTRY(TurtleSummon),

        // Summons x3
        MAP_ENTRY(TenguSummon),
        MAP_ENTRY(ImperialGuardSummon),
        MAP_ENTRY(WarhornSummon),

        // Other
        MAP_ENTRY(IdentificationKit),
        MAP_ENTRY(IdentificationKit_Superior),
        MAP_ENTRY(SalvageKit),
        MAP_ENTRY(SalvageKit_Expert),
        MAP_ENTRY(SalvageKit_Superior),
        MAP_ENTRY(Lockpick),
        MAP_ENTRY(ResScrolls),
        MAP_ENTRY(GoldCoin),
    };

    const std::map<ItemModelID, std::string> default_dont_hide_for_party = {
        MAP_ENTRY(EternalBlade),
        MAP_ENTRY(VoltaicSpear),
        MAP_ENTRY(MiniDhuum),
        MAP_ENTRY(CrystallineSword),
        MAP_ENTRY(DSR),
        MAP_ENTRY(ObsidianEdge),
    };

    GW::AgentID GetItemOwner(const GW::ItemID item_id)
    {
        const auto it = std::ranges::find_if(item_owners, [item_id](auto owner) {
            return owner.item == item_id;
        });
        return it == item_owners.end() ? 0 : it->owner;
    }

    bool WantToHide(const GW::Item& item, const bool can_pick_up)
    {
        using GW::Constants::ItemType;
        switch (static_cast<ItemType>(item.type)) {
            case ItemType::Bundle:
            case ItemType::Quest_Item:
            case ItemType::Minipet:
                return false;
            case ItemType::Key:
                if (!item.value)
                    return false; // Dungeon keys
                break;
            case ItemType::Trophy:
                if (!item.value)
                    return false; // Mission keys and Nightfall "treasures"
            default:
                break;
        }

        const auto rarity = GetRarity(item);

        if (can_pick_up) {
            if (dont_hide_for_player.contains(item.model_id)) {
                return false;
            }

            switch (rarity) {
                case Rarity::White:
                    return hide_player_white;
                case Rarity::Blue:
                    return hide_player_blue;
                case Rarity::Purple:
                    return hide_player_purple;
                case Rarity::Gold:
                    return hide_player_gold;
                case Rarity::Green:
                    return hide_player_green;
                case Rarity::Unknown:
                    return false;
            }
        }

        if (dont_hide_for_party.contains(item.model_id)) {
            return false;
        }

        switch (rarity) {
            case Rarity::White:
                return hide_party_white;
            case Rarity::Blue:
                return hide_party_blue;
            case Rarity::Purple:
                return hide_party_purple;
            case Rarity::Gold:
                return hide_party_gold;
            case Rarity::Green:
                return hide_party_green;
            case Rarity::Unknown:
                return false;
        }

        return false;
    }

    std::wstring SanitizeForCSV(const std::wstring& str)
    {
        std::wstring result;
        bool needs_quotes = false;

        for (wchar_t c : str) {
            if (c == L'"') {
                result += L"\"\"";
                needs_quotes = true;
            }
            else if (c == L',' || c == L'\n' || c == L'\r') {
                if (c == L'\n' || c == L'\r') {
                    result += L' ';
                }
                else {
                    result += c;
                }
                needs_quotes = true;
            }
            else {
                result += c;
            }
        }

        if (needs_quotes) {
            return L"\"" + result + L"\"";
        }
        return result;
    }

    ParsedItemInfo ParseInfoString(const std::wstring& info)
    {
        ParsedItemInfo parsed;

        std::wregex damage_regex(L"(\\w+) Dmg: (\\d+)-(\\d+)");
        std::wsmatch damage_match;
        if (std::regex_search(info, damage_match, damage_regex)) {
            parsed.damage_type = damage_match[1].str();
            parsed.min_damage = static_cast<uint16_t>(std::stoi(damage_match[2].str()));
            parsed.max_damage = static_cast<uint16_t>(std::stoi(damage_match[3].str()));
        }

        std::wregex req_regex(L"Requires (\\d+) ([^)<]+)");
        std::wsmatch req_match;
        if (std::regex_search(info, req_match, req_regex)) {
            parsed.requirement_value = static_cast<uint8_t>(std::stoi(req_match[1].str()));
            parsed.requirement_attribute = req_match[2].str();
        }

        return parsed;
    }

    void WritePendingDropToCSV(ItemDrops::PendingDrop* drop)
    {
        const auto filename = Resources::GetPath(L"drops.csv");
        auto path = Resources::GetPath(filename);
        const bool file_exists = std::filesystem::exists(path);
        ItemDrops::Instance().GetDropHistory().push_back(*drop);

        try {
            std::wofstream my_file(filename, std::ios::app);
            if (!my_file.is_open()) {
                delete drop;
                return;
            }

            if (!file_exists) {
                my_file << L"timestamp,item_id,item_name,item_type,rarity,quantity,map,value,damage_type,min_damage,max_damage,requirement_attribute,requirement_value,item_agent_id,owner_id,model_id,player_count,hero_count,henchman_count,game_mode\n";
            }

            const auto now = std::chrono::system_clock::now();
            const auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
            localtime_s(&tm_buf, &time);

            const auto safe_name = SanitizeForCSV(drop->item_name);

            my_file << std::put_time(&tm_buf, L"%Y-%m-%d %H:%M:%S") << L"," << drop->item_id << L"," << safe_name << L"," << drop->type << L"," << drop->rarity << L"," << drop->quantity << L"," << drop->map_name << L"," << drop->value << L","
                    << drop->damage_type << L","
                    << drop->min_damage << L"," << drop->max_damage << L"," << drop->requirement_attribute << L"," << static_cast<int>(drop->requirement_value) << L"," << drop->agent_id << L"," << drop->owner_id << L"," << drop->model_id << L","
                    << drop->player_count << L"," << drop->hero_count << L"," << drop->henchman_count << L"," << drop->game_mode << L"\n";
            my_file.flush();
            my_file.close();
        } catch (...) {}
        delete drop;
    }

    void DecodeMapName(ItemDrops::PendingDrop* drop)
    {
        if (drop->map_name_enc) {
            GW::UI::AsyncDecodeStr(
                drop->map_name_enc,
                [](void* param, const wchar_t* decoded) {
                    auto* pending = static_cast<ItemDrops::PendingDrop*>(param);
                    pending->map_name = decoded ? decoded : L"";
                    WritePendingDropToCSV(pending);
                },
                drop
            );
        }
        else {
            drop->map_name = L"";
            WritePendingDropToCSV(drop);
        }
    }

    void DecodeInfoString(ItemDrops::PendingDrop* drop)
    {
        if (drop->info_string_enc) {
            GW::UI::AsyncDecodeStr(
                drop->info_string_enc,
                [](void* param, const wchar_t* decoded) {
                    auto* pending = static_cast<ItemDrops::PendingDrop*>(param);
                    pending->info_string = decoded ? decoded : L"";

                    auto parsed = ParseInfoString(pending->info_string);
                    pending->damage_type = parsed.damage_type;
                    pending->min_damage = parsed.min_damage;
                    pending->max_damage = parsed.max_damage;
                    pending->requirement_attribute = parsed.requirement_attribute;
                    pending->requirement_value = parsed.requirement_value;

                    DecodeMapName(pending);
                },
                drop
            );
        }
        else {
            drop->info_string = L"";
            DecodeMapName(drop);
        }
    }

    void DecodeItemName(ItemDrops::PendingDrop* drop, const wchar_t* name_enc)
    {
        if (name_enc) {
            GW::UI::AsyncDecodeStr(
                name_enc,
                [](void* param, const wchar_t* decoded) {
                    auto* pending = static_cast<ItemDrops::PendingDrop*>(param);
                    pending->item_name = decoded ? decoded : L"";
                    DecodeInfoString(pending);
                },
                drop
            );
        }
        else {
            drop->item_name = L"";
            DecodeInfoString(drop);
        }
    }

    void WriteDropToCSV(const GW::Item* item, const GW::AgentID owner_id)
    {
        if (!item || !item->item_id || !item->name_enc) {
            return;
        }

        auto* drop = new ItemDrops::PendingDrop();
        drop->timestamp = std::chrono::system_clock::now();
        drop->item_id = item->item_id;
        drop->agent_id = item->agent_id;
        drop->owner_id = owner_id;
        drop->model_id = item->model_id;
        drop->quantity = item->quantity;
        drop->type = GetItemTypeString(item->type);
        drop->rarity = GetRarityString(GetRarity(*item));
        drop->player_count = GW::PartyMgr::GetPartyPlayerCount();
        drop->hero_count = GW::PartyMgr::GetPartyHeroCount();
        drop->henchman_count = GW::PartyMgr::GetPartyHenchmanCount();
        drop->game_mode = GW::PartyMgr::GetIsPartyInHardMode() ? L"Hard Mode" : L"Normal Mode";
        drop->value = item->value;
        drop->info_string_enc = item->info_string;

        const auto* map = GW::Map::GetCurrentMapInfo();
        if (map) {
            GW::UI::UInt32ToEncStr(map->name_id, drop->map_name_enc_buf, 8);
            drop->map_name_enc = drop->map_name_enc_buf;
        }
        else {
            drop->map_name_enc = nullptr;
        }

        DecodeItemName(drop, item->name_enc);
    }

    void OnAgentAdd(GW::HookStatus* status, const GW::Packet::StoC::AgentAdd* packet)
    {
        const auto* item = GetItemFromPacket(*packet);
        if (!item) {
            return;
        }

        const auto my_agent_id = GW::Agents::GetControlledCharacterId();

        const auto owner_id = GetItemOwner(item->item_id);
        const auto can_pick_up = owner_id == 0                    // not reserved
                                 || owner_id == my_agent_id; // reserved for user

        if (track_drops) {
            WriteDropToCSV(item, owner_id);
        }

        if (WantToHide(*item, can_pick_up)) {
            status->blocked = true;
            suppressed_packets.push_back(*packet);
        }
    }

    void OnAgentRemove(GW::HookStatus* status, GW::Packet::StoC::AgentRemove* packet)
    {
        // Block despawning the agent if the client never spawned it.
        const auto it = std::ranges::find_if(suppressed_packets, [agent_id = packet->agent_id](const auto& suppressed_packet) {
            return suppressed_packet.agent_id == agent_id;
        });

        if (it == suppressed_packets.end()) {
            return;
        }

        suppressed_packets.erase(it);
        status->blocked = true;
    }

    void OnMapLoad(GW::HookStatus*, GW::Packet::StoC::MapLoaded*)
    {
        suppressed_packets.clear();
        item_owners.clear();
    }

    void OnItemReuseId(GW::HookStatus*, GW::Packet::StoC::ItemGeneral_ReuseID* packet)
    {
        const auto it = std::ranges::find_if(item_owners, [item_id = packet->item_id](auto owner) {
            return owner.item == item_id;
        });

        if (it != item_owners.end()) {
            item_owners.erase(it);
        }
    }

    void OnItemUpdateOwner(GW::HookStatus*, GW::Packet::StoC::ItemUpdateOwner* packet)
    {
        const auto it = std::ranges::find_if(item_owners, [item_id = packet->item_id](auto owner) {
            return owner.item == item_id;
        });

        if (it == item_owners.end()) {
            item_owners.push_back({packet->item_id, packet->owner_agent_id});
        }
        else {
            it->owner = packet->owner_agent_id;
        }
    }

    void SpawnSuppressedItems()
    {
        for (const auto& packet : suppressed_packets) {
            GW::GameThread::Enqueue([cpy = packet]() mutable {
                // since a user can log out and exit the game with suppressed items still in memory,
                // only spawn if there is still a valid map context.
                // note: there is still an ItemContext at this point, so don't rely on that.
                if (GW::GetMapContext() != nullptr) {
                    GW::StoC::EmulatePacket(reinterpret_cast<GW::Packet::StoC::PacketBase*>(&cpy));
                }
            });
        }

        suppressed_packets.clear();
    }
} // namespace

void ItemDrops::Initialize()
{
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdd_Entry, OnAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_Entry, OnAgentRemove);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&OnMapLoad_Entry, OnMapLoad);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&OnItemReuseId_Entry, OnItemReuseId);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemUpdateOwner>(&OnItemUpdateOwner_Entry, OnItemUpdateOwner);

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"spawnblockeditems", [](GW::HookStatus*, const wchar_t*, int, const LPWSTR*) {
        SpawnSuppressedItems();
    });
}

void ItemDrops::SignalTerminate()
{
    ToolboxModule::SignalTerminate();

    SpawnSuppressedItems();
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&OnMapLoad_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&OnItemReuseId_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemUpdateOwner>(&OnItemUpdateOwner_Entry);

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

bool ItemDrops::CanTerminate()
{
    return suppressed_packets.empty();
}

void ItemDrops::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(hide_player_white);
    LOAD_BOOL(hide_player_blue);
    LOAD_BOOL(hide_player_purple);
    LOAD_BOOL(hide_player_gold);
    LOAD_BOOL(hide_player_green);
    LOAD_BOOL(hide_party_white);
    LOAD_BOOL(hide_party_blue);
    LOAD_BOOL(hide_party_purple);
    LOAD_BOOL(hide_party_gold);
    LOAD_BOOL(hide_party_green);
    LOAD_BOOL(track_drops);

    dont_hide_for_player = GuiUtils::IniToMap<decltype(dont_hide_for_player)>(ini, Name(), "dont_hide_for_player", default_dont_hide_for_player);
    dont_hide_for_party = GuiUtils::IniToMap<decltype(dont_hide_for_party)>(ini, Name(), "dont_hide_for_party", default_dont_hide_for_party);
}

void ItemDrops::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(hide_player_white);
    SAVE_BOOL(hide_player_blue);
    SAVE_BOOL(hide_player_purple);
    SAVE_BOOL(hide_player_gold);
    SAVE_BOOL(hide_player_green);
    SAVE_BOOL(hide_party_white);
    SAVE_BOOL(hide_party_blue);
    SAVE_BOOL(hide_party_purple);
    SAVE_BOOL(hide_party_gold);
    SAVE_BOOL(hide_party_green);
    SAVE_BOOL(track_drops);

    GuiUtils::MapToIni(ini, Name(), "dont_hide_for_player", dont_hide_for_player);
    GuiUtils::MapToIni(ini, Name(), "dont_hide_for_party", dont_hide_for_party);
}

void ItemDrops::DrawSettingsInternal()
{
    ImGui::Text("Drop Tracking Settings");
    ImGui::Checkbox("Drop Tracking Enabled", &track_drops);
    ImGui::ShowHelp("This creates a CSV at DIRECTORY which contains all the information about drops you've gotten.");
    ImGui::Separator();
    ImGui::Text("Item Filter Settings");
    ImGui::NewLine();
    ImGui::Text("Block the following item drops:");
    ImGui::Separator();
    ImGui::TextDisabled("First column is for items you can pick up, second for items reserved for a party member");
    ImGui::Columns(2, "player_or_ally");

    ImGui::Checkbox("White##player", &hide_player_white);
    ImGui::Checkbox("Blue##player", &hide_player_blue);
    ImGui::Checkbox("Purple##player", &hide_player_purple);
    ImGui::Checkbox("Gold##player", &hide_player_gold);
    ImGui::Checkbox("Green##player", &hide_player_green);

    ImGui::NextColumn();

    ImGui::Checkbox("White##party", &hide_party_white);
    ImGui::Checkbox("Blue##party", &hide_party_blue);
    ImGui::Checkbox("Purple##party", &hide_party_purple);
    ImGui::Checkbox("Gold##party", &hide_party_gold);
    ImGui::Checkbox("Green##party", &hide_party_green);

    ImGui::EndColumns();

    const auto itembtn = std::format("Spawn all blocked items ({})", suppressed_packets.size());

    if (ImGui::Button(itembtn.c_str())) {
        SpawnSuppressedItems();
    }

    ImGui::Separator();

    ImGui::TextDisabled("Below, you can define items that should never be blocked for you or party members.");

    auto& style = ImGui::GetStyle();
    const auto old_color = style.Colors[ImGuiCol_Header];
    style.Colors[ImGuiCol_Header] = ImColor{};
    if (ImGui::CollapsingHeader("Don't hide items for you with model ids")) {
        ImGui::PushID("BlockPlayerItems");

        if (ImGui::Button("Restore defaults##player")) {
            dont_hide_for_player = default_dont_hide_for_player;
        }
        ImGui::BeginChild("dont_block_for_player", ImVec2(0.0f, dont_hide_for_player.size() * 26.f));
        for (const auto& [item_id, item_name] : dont_hide_for_player) {
            ImGui::PushID(static_cast<int>(item_id));
            ImGui::Text("%s (%d)", item_name.c_str(), item_id);
            ImGui::SameLine();
            const bool clicked = ImGui::Button(" X ");
            ImGui::PopID();
            if (clicked) {
                dont_hide_for_player.erase(item_id);
                break;
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        bool submitted = false;
        ImGui::Text("Add new item:");
        static int new_item_id;
        static char buf[50];
        ImGui::InputText("Item Name##player", buf, 50);
        ImGui::InputInt("Item Model ID##player", &new_item_id);
        submitted |= ImGui::Button("Add");
        if (submitted && new_item_id > 0) {
            const auto new_id = static_cast<uint32_t>(new_item_id);
            if (!dont_hide_for_player.contains(new_id)) {
                dont_hide_for_player[new_id] = std::string(buf);
                Log::Flash("Added Item %s with ID (%d)", buf, new_id);
                std::ranges::fill(buf, '\0');
                new_item_id = 0;
            }
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Don't hide items for party members with model ids")) {
        ImGui::PushID("BlockPartyItems");
        if (ImGui::Button("Restore defaults##party")) {
            dont_hide_for_party = default_dont_hide_for_party;
        }
        ImGui::BeginChild("dont_block_for_party", ImVec2(0.0f, dont_hide_for_party.size() * 26.f));
        for (const auto& [item_id, item_name] : dont_hide_for_party) {
            ImGui::PushID(static_cast<int>(item_id));
            ImGui::Text("%s (%d)", item_name.c_str(), item_id);
            ImGui::SameLine();
            const bool clicked = ImGui::Button(" X ");
            ImGui::PopID();
            if (clicked) {
                dont_hide_for_party.erase(item_id);
                break;
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        bool submitted = false;
        ImGui::Text("Add new item:");
        static int new_item_id_party;
        static char buf[50];
        ImGui::InputText("Item Name##party", buf, 50);
        ImGui::InputInt("Item Model ID##party", &new_item_id_party);
        submitted |= ImGui::Button("Add");
        if (submitted && new_item_id_party > 0) {
            const auto new_id = static_cast<uint32_t>(new_item_id_party);
            if (!dont_hide_for_party.contains(new_id)) {
                dont_hide_for_party[new_id] = std::string(buf);
                Log::Flash("Added Item %s with ID (%d)", buf, new_id);
                std::ranges::fill(buf, '\0');
                new_item_id_party = 0;
            }
        }

        ImGui::PopID();
    }
    style.Colors[ImGuiCol_Header] = old_color;
}


std::vector<ItemDrops::PendingDrop>& ItemDrops::GetDropHistory()
{
    return drop_history;
}

void ItemDrops::ClearDropHistory()
{
    drop_history.clear();
}

bool ItemDrops::IsTrackingEnabled() const
{
    return track_drops;
}
