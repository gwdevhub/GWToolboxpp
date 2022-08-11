#include "stdafx.h"

#include "GWCA/Context/GameContext.h"
#include "GWCA/Context/ItemContext.h"
#include "GWCA/GameEntities/Agent.h"
#include "GWCA/Managers/AgentMgr.h"
#include "GWCA/Managers/GameThreadMgr.h"
#include "GWCA/Managers/StoCMgr.h"
#include "GWCA/Packets/StoC.h"
#include <Modules/ItemFilter.h>

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);

namespace {

    template <typename T>
    void EmulatePacket(T const& packet) {
        GW::GameThread::Enqueue([cpy = packet]() mutable {
            cpy.header = T::STATIC_HEADER;
            GW::StoC::EmulatePacket(reinterpret_cast<GW::Packet::StoC::PacketBase*>(&cpy));
        });
    }

    Rarity GetRarity(GW::Item const& item) {
        if (item.complete_name_enc == nullptr) return Rarity::Unknown;

        switch (item.complete_name_enc[0]) {
            case 2621: return Rarity::White;
            case 2623: return Rarity::Blue;
            case 2626: return Rarity::Purple;
            case 2624: return Rarity::Gold;
            case 2627: return Rarity::Green;
            default: return Rarity::Unknown;
        }
    }

    const GW::Item* IsItem(const GW::Packet::StoC::AgentAdd& packet) {
        // filter non-item-agents
        if (packet.type != 4 || packet.unk3 != 0) return nullptr;

        auto const& items = GW::GameContext::instance()->items->item_array;
        auto const item_id = packet.agent_type;
        if (item_id >= items.size()) return nullptr;

        return items[item_id];
    }

    const std::vector<ItemModelID> never_hide_for_user = {
        // Rare and Valuable Items
        GW::Constants::ItemID::DSR,
        GW::Constants::ItemID::EternalBlade,
        GW::Constants::ItemID::MiniDhuum,
        GW::Constants::ItemID::VoltaicSpear,
        GW::Constants::ItemID::CrystallineSword,
        GW::Constants::ItemID::ArmbraceOfTruth,
        GW::Constants::ItemID::MargoniteGem,
        GW::Constants::ItemID::StygianGem,
        GW::Constants::ItemID::TitanGem,
        GW::Constants::ItemID::TormentGem,

        // Crafting Items
        GW::Constants::ItemID::Diamond,
        GW::Constants::ItemID::Ruby,
        GW::Constants::ItemID::Sapphire,
        GW::Constants::ItemID::GlobofEctoplasm,
        GW::Constants::ItemID::ObsidianShard,

        // Consumables
        GW::Constants::ItemID::Cupcakes,
        GW::Constants::ItemID::Apples,
        GW::Constants::ItemID::Corns,
        GW::Constants::ItemID::Pies,
        GW::Constants::ItemID::Eggs,
        GW::Constants::ItemID::Warsupplies,
        GW::Constants::ItemID::SkalefinSoup,
        GW::Constants::ItemID::PahnaiSalad,
        GW::Constants::ItemID::Kabobs,
        GW::Constants::ItemID::PumpkinCookie,

        // Rock Candy
        GW::Constants::ItemID::GRC,
        GW::Constants::ItemID::BRC,
        GW::Constants::ItemID::RRC,

        // Conset
        GW::Constants::ItemID::ConsEssence,
        GW::Constants::ItemID::ConsArmor,
        GW::Constants::ItemID::ConsGrail,

        // Lunars
        GW::Constants::ItemID::LunarDragon,
        GW::Constants::ItemID::LunarHorse,
        GW::Constants::ItemID::LunarMonkey,
        GW::Constants::ItemID::LunarOx,
        GW::Constants::ItemID::LunarRabbit,
        GW::Constants::ItemID::LunarRat,
        GW::Constants::ItemID::LunarRooster,
        GW::Constants::ItemID::LunarSheep,
        GW::Constants::ItemID::LunarSnake,
        GW::Constants::ItemID::LunarTiger,
        GW::Constants::ItemID::LunarDog,
        GW::Constants::ItemID::LunarPig,
        GW::Constants::ItemID::LunarMonkey,

        // Alcohol
        GW::Constants::ItemID::Absinthe,
        GW::Constants::ItemID::AgedDwarvenAle,
        GW::Constants::ItemID::AgedHuntersAle,
        GW::Constants::ItemID::BottleOfJuniberryGin,
        GW::Constants::ItemID::BottleOfVabbianWine,
        GW::Constants::ItemID::Cider,
        GW::Constants::ItemID::DwarvenAle,
        GW::Constants::ItemID::Eggnog,
        GW::Constants::ItemID::FlaskOfFirewater,
        GW::Constants::ItemID::Grog,
        GW::Constants::ItemID::HuntersAle,
        GW::Constants::ItemID::Keg,
        GW::Constants::ItemID::KrytanBrandy,
        GW::Constants::ItemID::Ricewine,
        GW::Constants::ItemID::ShamrockAle,
        GW::Constants::ItemID::SpikedEggnog,
        GW::Constants::ItemID::WitchsBrew,

        // Summons
        GW::Constants::ItemID::GhastlyStone,
        GW::Constants::ItemID::GakiSummon,
        GW::Constants::ItemID::TurtleSummon,

        // Summons x3
        GW::Constants::ItemID::TenguSummon,
        GW::Constants::ItemID::ImperialGuardSummon,
        GW::Constants::ItemID::WarhornSummon,

        // Other
        GW::Constants::ItemID::IdentificationKit,
        GW::Constants::ItemID::IdentificationKit_Superior,
        GW::Constants::ItemID::SalvageKit,
        GW::Constants::ItemID::SalvageKit_Expert,
        GW::Constants::ItemID::SalvageKit_Superior,
        GW::Constants::ItemID::Lockpick,
        GW::Constants::ItemID::ResScrolls,
        GW::Constants::ItemID::GoldCoin,

        // Quest Items
        GW::Constants::ItemID::UnholyText,
    };

    const std::vector<ItemModelID> never_hide_for_party = {
        GW::Constants::ItemID::EternalBlade,
        GW::Constants::ItemID::VoltaicSpear,
        GW::Constants::ItemID::MiniDhuum,
        GW::Constants::ItemID::CrystallineSword,
        GW::Constants::ItemID::DSR,
        GW::Constants::ItemID::GoldCoin,
    };

} // namespace

void ItemFilter::Initialize() {
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback(&OnAgentAdd_Entry, GW::Packet::StoC::AgentAdd::STATIC_HEADER, OnAgentAdd);
    GW::StoC::RegisterPacketCallback(&OnAgentRemove_Entry, GW::Packet::StoC::AgentRemove::STATIC_HEADER, OnAgentRemove);
    GW::StoC::RegisterPacketCallback(&OnMapLoad_Entry, GW::Packet::StoC::MapLoaded::STATIC_HEADER, OnMapLoad);
    GW::StoC::RegisterPacketCallback(
        &OnItemReuseId_Entry, GW::Packet::StoC::ItemGeneral_ReuseID::STATIC_HEADER, OnItemReuseId);
    GW::StoC::RegisterPacketCallback(
        &OnItemUpdateOwner_Entry, GW::Packet::StoC::UpdateItemOwner::STATIC_HEADER, OnItemUpdateOwner);
}

void ItemFilter::SignalTerminate() {
    ToolboxModule::SignalTerminate();

    SpawnSuppressedItems();
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&OnMapLoad_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&OnItemReuseId_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::UpdateItemOwner>(&OnItemUpdateOwner_Entry);
}

void ItemFilter::LoadSettings(CSimpleIniA* ini) {
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
}

void ItemFilter::SaveSettings(CSimpleIniA* ini) {
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
}

void ItemFilter::SpawnSuppressedItems() {
    for (auto const& packet : suppressed_packets)
        EmulatePacket(packet);

    suppressed_packets.clear();
}
void ItemFilter::OnAgentAdd(GW::HookStatus* status, GW::Packet::StoC::PacketBase* basepacket) {
    const auto packet = reinterpret_cast<GW::Packet::StoC::AgentAdd*>(basepacket);
    const auto* item = IsItem(*packet);
    if (!item) return;

    const auto* player = GW::Agents::GetCharacter();
    if (player == nullptr) return;

    if (player->max_energy == 0 || player->login_number == 0) {
        // we're spectating, not sure what our own player is
        if (Instance().WantToHide(*item, false) && Instance().WantToHide(*item, true)) {
            // only block items that we want to block for player and party
            status->blocked = true;
            Instance().suppressed_packets.push_back(*packet);
        }
        return;
    }

    const auto owner_id = Instance().GetItemOwner(item->item_id);
    const auto can_pick_up = owner_id == 0                    // not reserved
                             || owner_id == player->agent_id; // reserved for user

    if (Instance().WantToHide(*item, can_pick_up)) {
        status->blocked = true;
        Instance().suppressed_packets.push_back(*packet);
    }
}

void ItemFilter::OnAgentRemove(GW::HookStatus* status, GW::Packet::StoC::PacketBase* basepacket) {
    // Block despawning the agent if the client never spawned it.
    const auto packet = reinterpret_cast<GW::Packet::StoC::AgentRemove*>(basepacket);

    auto const found = std::find_if(Instance().suppressed_packets.begin(), Instance().suppressed_packets.end(),
        [&packet](auto const& suppressed_packet) { return suppressed_packet.agent_id == packet->agent_id; });

    if (found == Instance().suppressed_packets.end()) return;

    Instance().suppressed_packets.erase(found);
    status->blocked = true;
}

void ItemFilter::OnMapLoad(GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
    Instance().suppressed_packets.clear();
    Instance().item_owners.clear();
}

void ItemFilter::OnItemReuseId(GW::HookStatus*, GW::Packet::StoC::PacketBase* basepacket) {
    const auto packet = reinterpret_cast<GW::Packet::StoC::ItemGeneral_ReuseID*>(basepacket);
    const auto it = std::find(Instance().item_owners.begin(), Instance().item_owners.end(), packet->item_id);

    if (it != Instance().item_owners.end()) Instance().item_owners.erase(it);
}

GW::AgentID ItemFilter::GetItemOwner(const GW::ItemID id) const {
    const auto it = std::find(item_owners.begin(), item_owners.end(), id);
    if (it == item_owners.end()) return 0;

    return it->owner;
}

void ItemFilter::OnItemUpdateOwner(GW::HookStatus*, GW::Packet::StoC::PacketBase* basepacket) {
    const auto packet = reinterpret_cast<GW::Packet::StoC::UpdateItemOwner*>(basepacket);
    auto const it = std::find(Instance().item_owners.begin(), Instance().item_owners.end(), packet->item_id);

    if (it == Instance().item_owners.end()) {
        Instance().item_owners.push_back({packet->item_id, packet->owner_agent_id});
    } else {
        it->owner = packet->owner_agent_id;
    }
}

bool ItemFilter::WantToHide(const GW::Item& item, const bool can_pick_up) const {
    const auto rarity = GetRarity(item);
    if (can_pick_up) {
        if (std::find(never_hide_for_user.begin(), never_hide_for_user.end(), item.model_id) !=
            never_hide_for_user.end())
            return false;

        switch (rarity) {
            case Rarity::White: return hide_player_white;
            case Rarity::Blue: return hide_player_blue;
            case Rarity::Purple: return hide_player_purple;
            case Rarity::Gold: return hide_player_gold;
            case Rarity::Green: return hide_player_green;
            case Rarity::Unknown: return false;
        }
    }

    if (std::find(never_hide_for_party.begin(), never_hide_for_party.end(), item.model_id) !=
        never_hide_for_party.end())
        return false;

    switch (rarity) {
        case Rarity::White: return hide_party_white;
        case Rarity::Blue: return hide_party_blue;
        case Rarity::Purple: return hide_party_purple;
        case Rarity::Gold: return hide_party_gold;
        case Rarity::Green: return hide_party_green;
        case Rarity::Unknown: return false;
    }

    return false;
}

void ItemFilter::DrawSettingInternal() {
    ImGui::Text("Block the following item drops:");
    ImGui::Separator();
    ImGui::TextDisabled("First column is for items you can pick up, second for items reserved for a party member");
    ImGui::TextDisabled("Certain rare items will never be blocked");
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

    const std::string itembtn = "Spawn all blocked items (" + std::to_string(suppressed_packets.size()) + ")";

    if (ImGui::Button(itembtn.c_str())) {
        SpawnSuppressedItems();
    }
}
