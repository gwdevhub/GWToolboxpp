#include "stdafx.h"
#include <chrono>

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
#include <Timer.h>
#include <Utils/TextUtils.h>

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
    clock_t last_drops_written = 0;


    using ItemModelID = decltype(GW::Item::model_id);
    std::vector<GW::Packet::StoC::AgentAdd> suppressed_packets{};
    std::vector<ItemOwner> item_owners{};

    std::vector<ItemDrops::PendingDrop*> drop_history;
    std::vector<ItemDrops::PendingDrop*> pending_write_to_csv;
    std::vector<std::string> pending_full_exports;

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


    using namespace GW::Constants::ItemID;


    std::map<std::wstring, GuiUtils::EncString*> cached_item_names;
    GuiUtils::EncString* GetItemName(const wchar_t* enc_string) {
        if (!enc_string) return nullptr;
        if (!cached_item_names.contains(enc_string)) {
            auto enc_str = (new GuiUtils::EncString(enc_string))->language(GW::Constants::Language::English);
            cached_item_names[enc_string] = enc_str;
        }
        return cached_item_names[enc_string];
    }
    void ClearItemNames()
    {
        for (auto i : cached_item_names) {
            i.second->Release();
        }
        cached_item_names.clear();
    }


    GW::Item* GetItemFromPacket(const GW::Packet::StoC::AgentAdd& packet)
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

    std::filesystem::path GetItemDropCSVFilename() {
        // Generate filename with current date
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time);

        wchar_t date_buffer[32];
        std::wcsftime(date_buffer, sizeof(date_buffer) / sizeof(wchar_t), L"%Y-%m-%d", &tm);
        std::wstring drops_basename = std::wstring(date_buffer) + L"_drops.csv";

        return Resources::GetPath("item_drops", drops_basename);
    }

    GW::AgentID GetItemOwner(const GW::ItemID item_id)
    {
        const auto it = std::ranges::find_if(item_owners, [item_id](auto owner) {
            return owner.item == item_id;
        });
        return it == item_owners.end() ? 0 : it->owner;
    }

    bool WantToHide(const GW::Item* item, const bool can_pick_up)
    {
        using GW::Constants::ItemType;
        switch (item->type) {
            case ItemType::Bundle:
            case ItemType::Quest_Item:
            case ItemType::Minipet:
                return false;
            case ItemType::Key:
                if (!item->value)
                    return false; // Dungeon keys
                break;
            case ItemType::Trophy:
                if (!item->value)
                    return false; // Mission keys and Nightfall "treasures"
            default:
                break;
        }

        const auto rarity = GW::Items::GetRarity(item);

        if (can_pick_up) {
            if (dont_hide_for_player.contains(item->model_id)) {
                return false;
            }

            switch (rarity) {
                case GW::Constants::Rarity::White:
                    return hide_player_white;
                case GW::Constants::Rarity::Blue:
                    return hide_player_blue;
                case GW::Constants::Rarity::Purple:
                    return hide_player_purple;
                case GW::Constants::Rarity::Gold:
                    return hide_player_gold;
                case GW::Constants::Rarity::Green:
                    return hide_player_green;
            }
        }

        if (dont_hide_for_party.contains(item->model_id)) {
            return false;
        }

        switch (rarity) {
            case GW::Constants::Rarity::White:
                return hide_party_white;
            case GW::Constants::Rarity::Blue:
                return hide_party_blue;
            case GW::Constants::Rarity::Purple:
                return hide_party_purple;
            case GW::Constants::Rarity::Gold:
                return hide_party_gold;
            case GW::Constants::Rarity::Green:
                return hide_party_green;
        }

        return false;
    }

    void WriteDropToCSV(ItemDrops::PendingDrop* item)
    {
        pending_write_to_csv.push_back(item);
    }
    void WritePendingDropsToFile(bool force = false) {
        if (pending_write_to_csv.empty() || (!force && TIMER_DIFF(last_drops_written) < 5000)) 
            return;
        // Early validation - avoid exceptions from GetItemName
        bool all_decoded = true;
        for (const auto& pending : pending_write_to_csv) {
            auto item_name = GetItemName(pending->item_name_enc);
            if (!item_name || item_name->IsDecoding()) {
                all_decoded = false;
                break;
            }
        }
        if (!all_decoded) {
            return;
        }

        last_drops_written = TIMER_INIT();
        auto drops_filename = GetItemDropCSVFilename();
        std::error_code ec;
        const bool file_exists = std::filesystem::exists(drops_filename, ec);
        if (ec) {
            Log::WarningW(L"std::filesystem::exists for %s failed", drops_filename.wstring().c_str());
            return;
        }
        std::filesystem::create_directories(drops_filename.parent_path(),ec);

        // Open file with nothrow
        std::wofstream my_file;
        my_file.exceptions(std::ios::goodbit); // Disable exceptions
        my_file.open(drops_filename.c_str(), std::ios::app);

        if (!my_file.is_open() || my_file.fail()) {
            Log::WarningW(L"std::wofstream for %s failed", drops_filename.wstring().c_str());
            return;
        }

        // Write header if new file
        if (!file_exists) {
            my_file << ItemDrops::PendingDrop::GetCSVHeader() << L"\n";
            if (my_file.fail()) {
                my_file.close();
                return;
            }
        }

        // Write data
        for (const auto& pending : pending_write_to_csv) {
            my_file << pending->toCSV() << L"\n";
            if (my_file.fail()) {
                my_file.close();
                return;
            }
        }

        pending_write_to_csv.clear();
        my_file.flush();
        my_file.close();
    }

    std::map<uint32_t, bool> already_seen_items;

    bool ShouldTrackItem(GW::Item* item) {
        return track_drops && item && item->type != GW::Constants::ItemType::Bundle;
    }

    void OnAgentAdd(GW::HookStatus* status, const GW::Packet::StoC::AgentAdd* packet)
    {
        auto* item = GetItemFromPacket(*packet);
        if (!item) {
            return;
        }

        const auto my_agent_id = GW::Agents::GetControlledCharacterId();

        const auto owner_id = GetItemOwner(item->item_id);
        const auto can_pick_up = owner_id == 0                    // not reserved
                                 || owner_id == my_agent_id; // reserved for user

        if (ShouldTrackItem(item)) {
            uint32_t hash = packet->agent_id;
            hash ^= static_cast<uint32_t>(packet->position.x * 1000.0f);
            hash ^= (static_cast<uint32_t>(packet->position.y * 1000.0f) << 16);
            if (hash && !already_seen_items.contains(hash)) {
                already_seen_items[hash] = true;
                auto drop = new ItemDrops::PendingDrop(item);
                drop_history.push_back(drop);
                WriteDropToCSV(drop);
            }
        }

        if (WantToHide(item, can_pick_up)) {
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
        already_seen_items.clear();
        suppressed_packets.clear();
        item_owners.clear();
        ClearItemNames();
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

void ItemDrops::Update(float) {
    WritePendingDropsToFile();    
    if (!pending_full_exports.empty()) {
        for (auto pending : drop_history) {
            if (GetItemName(pending->item_name_enc)->IsDecoding()) {
                return;
            }
        }
        
        for (auto it = pending_full_exports.begin(); it != pending_full_exports.end(); ) {
            const auto filename = Resources::GetPath(*it);
            auto path = Resources::GetPath(filename);
            const bool file_exists = std::filesystem::exists(path);
            std::wofstream my_file(filename, std::ios::app);
            if (!my_file.is_open()) {
                return;
            }
            if (!file_exists) {
                my_file << ItemDrops::PendingDrop::GetCSVHeader() << L"\n";
            }
            for (auto pending : drop_history) {
                my_file << pending->toCSV() << L"\n";
            }
            my_file.flush();
            my_file.close();
            it = pending_full_exports.erase(it);
        }
    }
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
    ClearDropHistory();
    ClearItemNames();
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

int ItemDrops::GetTotalGoldValue()
{
    int value = 0;
    for (auto drop : drop_history) {
        if (drop->type == GW::Constants::ItemType::Gold_Coin) {
            value += drop->value;
        }else {
            value += drop->value * drop->quantity;  
        }
    }
    return value;
}

std::vector<ItemDrops::PendingDrop*>& ItemDrops::GetDropHistory()
{
    return drop_history;
}

void ItemDrops::ClearDropHistory()
{
    pending_write_to_csv.clear();
    for (auto drop : drop_history) {
        delete drop;
    }
    drop_history.clear();
}

bool ItemDrops::IsTrackingEnabled() const
{
    return track_drops;
}

ItemDrops::PendingDrop::PendingDrop(GW::Item* _item)
{
    ASSERT(_item);
    auto item = (InventoryManager::Item*)_item;
    instance_time = GW::Map::GetInstanceTime();
    quantity = item->quantity & 0xff;
    type = item->type;
    rarity = GW::Items::GetRarity(item);
    player_count = GW::PartyMgr::GetPartyPlayerCount() & 0xf;
    hero_count = GW::PartyMgr::GetPartyHeroCount() & 0xf;
    henchman_count = GW::PartyMgr::GetPartyHenchmanCount() & 0xf;
    hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
    value = item->value;
    map_id = GW::Map::GetMapID();
    icon = Resources::GetItemImage(item);
    Resources::GetMapName(map_id)->wstring();

    const wchar_t* item_name_pt = L"\x101";
    if (item->single_item_name && *item->single_item_name) {
        item_name_pt = item->single_item_name;
    }
    else if (item->name_enc && *item->name_enc) {
        item_name_pt = item->name_enc;
    }

    const auto len = wcslen(item_name_pt);
    item_name_enc = new wchar_t[len + 1];
    wcscpy(item_name_enc, item_name_pt);

    ::GetItemName(item_name_enc)->wstring(); // Trigger decode.

    time(&system_time);

    auto mod = item->GetModifier(0x2798);
    if (mod) {
        requirement_attribute = (GW::Constants::AttributeByte)mod->arg1();
        requirement_value = mod->arg2() & 0xf;
    }
    mod = item->GetModifier(0xa7a8);
    if (mod) {
        min_damage = mod->arg2() & 0xff;
        max_damage = mod->arg1() & 0xff;
    }
    mod = item->GetModifier(0x24b8);
    if (mod) {
        damage_type = (GW::Constants::DamageType)mod->arg1();
    }
}

ItemDrops::PendingDrop::~PendingDrop() {
    delete[] item_name_enc;
}

const wchar_t* ItemDrops::PendingDrop::GetCSVHeader()
{
    return L"SystemTime,InstanceTime,Map,ItemName,Quantity,Value,"
           L"ItemType,Rarity,DamageType,MinDamage,MaxDamage,"
           L"RequirementAttribute,RequirementValue,"
           L"PlayerCount,HeroCount,HenchmanCount,HardMode";
}

GuiUtils::EncString* ItemDrops::PendingDrop::GetItemName()
{
    return ::GetItemName(item_name_enc);
}

void ItemDrops::AddPendingExport(std::string chosen_path)
{
    pending_full_exports.push_back(chosen_path);
}

const std::wstring ItemDrops::PendingDrop::toCSV()
{
    std::wstringstream ss;
    ss << system_time << L",";
    ss << instance_time << L",";
    ss << (uint32_t)map_id << L",";
    ss << TextUtils::SanitizeForCSV(GetItemName()->wstring()) << L",";
    ss << quantity << L",";
    ss << value << L",";
    ss << (uint32_t)type << L",";
    ss << (uint32_t)rarity << L",";
    ss << (uint32_t)damage_type << L",";
    ss << min_damage << L",";
    ss << max_damage << L",";
    ss << (uint32_t)requirement_attribute << L",";
    ss << requirement_value << L",";
    ss << player_count << L",";
    ss << hero_count << L",";
    ss << henchman_count << L",";
    ss << (hard_mode ? L"1" : L"0");
    return ss.str();
}
