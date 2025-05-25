#include "stdafx.h"

#include <Logger.h>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>

#include <Modules/ItemDescriptionHandler.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Modules/SalvageInfoModule.h>
#include <Windows/DailyQuestsWindow.h>

#include <Utils/GuiUtils.h>
#include <Constants/EncStrings.h>

using nlohmann::json;
namespace {

    void SignalItemDescriptionUpdated(const wchar_t* enc_name) {
        // Now we've got the wiki info parsed, trigger an item update ui message; this will refresh the item tooltip
        GW::GameThread::Enqueue([enc_name] {
            const auto item = GW::Items::GetHoveredItem();
            if (!item)
                return;
            const auto item_name_without_mods = ItemDescriptionHandler::GetItemEncNameWithoutMods(item);
            if (wcscmp(item_name_without_mods.c_str(), enc_name) == 0) {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(),0xffffffff) ,GW::UI::UIMessage::kItemUpdated, item);
            }
            });
    }

    // Print out a string representation of the relative time from now e.g. 2 years
    std::wstring PrintRelativeTime(time_t timestamp) {
        const auto current_time = time(nullptr);
        if (timestamp < current_time)
            return L"the past"; // @Cleanup; N minutes ago etc

        auto amount = (timestamp - current_time);
        auto time_format = [](const time_t amount, const wchar_t* unit) {
            if (amount != 1)
                return std::format(L"{} {}s", amount, unit);
            return std::format(L"{} {}", amount, unit);
            };
        if (amount < 60)
            return L"less than a minute";

        amount /= 60;
        if (amount < 60)
            return time_format(amount, L"minute");
        amount /= 60;
        if (amount < 24)
            return time_format(amount, L"hour");
        amount /= 24;
        if (amount < 14)
            return time_format(amount, L"day");
        amount /= 7;
        if (amount < 8)
            return time_format(amount, L"week");
        amount /= 4;
        if (amount < 24)
            return time_format(amount, L"month");
        amount /= 12;
        return time_format(amount, L"year");
    }

    void NewLineIfNotEmpty(std::wstring& enc_str) {
        if (!enc_str.empty())
            enc_str += L"\x2\x102\x2";
    }

    const wchar_t* material_name_from_slot(GW::Constants::MaterialSlot material_id) {
        switch (material_id) {
        case GW::Constants::MaterialSlot::Bone:return GW::EncStrings::Bone;
        case GW::Constants::MaterialSlot::IronIngot:return GW::EncStrings::IronIngot;
        case GW::Constants::MaterialSlot::TannedHideSquare:return GW::EncStrings::TannedHideSquare;
        case GW::Constants::MaterialSlot::Scale:return GW::EncStrings::Scale;
        case GW::Constants::MaterialSlot::ChitinFragment:return GW::EncStrings::ChitinFragment;
        case GW::Constants::MaterialSlot::BoltofCloth:return GW::EncStrings::BoltofCloth;
        case GW::Constants::MaterialSlot::WoodPlank:return GW::EncStrings::WoodPlank;
        case GW::Constants::MaterialSlot::GraniteSlab:return GW::EncStrings::GraniteSlab;
        case GW::Constants::MaterialSlot::PileofGlitteringDust:return GW::EncStrings::PileofGlitteringDust;
        case GW::Constants::MaterialSlot::PlantFiber:return GW::EncStrings::PlantFiber;
        case GW::Constants::MaterialSlot::Feather:return GW::EncStrings::Feather;

        case GW::Constants::MaterialSlot::FurSquare:return GW::EncStrings::FurSquare;
        case GW::Constants::MaterialSlot::BoltofLinen:return GW::EncStrings::BoltofLinen;
        case GW::Constants::MaterialSlot::BoltofDamask:return GW::EncStrings::BoltofDamask;
        case GW::Constants::MaterialSlot::BoltofSilk:return GW::EncStrings::BoltofSilk;
        case GW::Constants::MaterialSlot::GlobofEctoplasm:return GW::EncStrings::GlobofEctoplasm;
        case GW::Constants::MaterialSlot::SteelIngot:return GW::EncStrings::SteelIngot;
        case GW::Constants::MaterialSlot::DeldrimorSteelIngot:return GW::EncStrings::DeldrimorSteelIngot;
        case GW::Constants::MaterialSlot::MonstrousClaw:return GW::EncStrings::MonstrousClaw;
        case GW::Constants::MaterialSlot::MonstrousEye:return GW::EncStrings::MonstrousEye;
        case GW::Constants::MaterialSlot::MonstrousFang:return GW::EncStrings::MonstrousFang;
        case GW::Constants::MaterialSlot::Ruby:return GW::EncStrings::Ruby;
        case GW::Constants::MaterialSlot::Sapphire:return GW::EncStrings::Sapphire;
        case GW::Constants::MaterialSlot::Diamond:return GW::EncStrings::Diamond;
        case GW::Constants::MaterialSlot::OnyxGemstone:return GW::EncStrings::OnyxGemstone;
        case GW::Constants::MaterialSlot::LumpofCharcoal:return GW::EncStrings::LumpofCharcoal;
        case GW::Constants::MaterialSlot::ObsidianShard:return GW::EncStrings::ObsidianShard;
        case GW::Constants::MaterialSlot::TemperedGlassVial:return GW::EncStrings::TemperedGlassVial;
        case GW::Constants::MaterialSlot::LeatherSquare:return GW::EncStrings::LeatherSquare;
        case GW::Constants::MaterialSlot::ElonianLeatherSquare:return GW::EncStrings::ElonianLeatherSquare;
        case GW::Constants::MaterialSlot::VialofInk:return GW::EncStrings::VialofInk;
        case GW::Constants::MaterialSlot::RollofParchment:return GW::EncStrings::RollofParchment;
        case GW::Constants::MaterialSlot::RollofVellum:return GW::EncStrings::RollofVellum;
        case GW::Constants::MaterialSlot::SpiritwoodPlank:return GW::EncStrings::SpiritwoodPlank;
        case GW::Constants::MaterialSlot::AmberChunk:return GW::EncStrings::AmberChunk;
        case GW::Constants::MaterialSlot::JadeiteShard:return GW::EncStrings::JadeiteShard;
        }
        return nullptr;
    }

    void AppendSalvageInfoDescription(const uint32_t item_id, std::wstring& description) {
        const auto item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));

        if (!(item && item->name_enc && *item->name_enc && item->IsSalvagable(false))) {
            return;
        }

        const auto formula = GW::Items::GetItemFormula(item);
        if (!(formula && formula->material_cost_count))
            return;

        std::vector<const wchar_t*> common_materials;
        std::vector<const wchar_t*> rare_materials;
        for (size_t i = 0; i < formula->material_cost_count; i++) {
            const auto material_id = formula->material_cost_buffer[i].material;
            if (material_id > GW::Constants::MaterialSlot::Feather) {
                rare_materials.push_back(material_name_from_slot(material_id));
            }
            else {
                common_materials.push_back(material_name_from_slot(material_id));
            }
        }

        if (!common_materials.empty()) {
            std::wstring items;
            for (auto i : common_materials) {
                if(!i) continue;
                if (!items.empty())
                    items += L"\x2\x108\x107, \x1\x2";
                items += i;
            }
            if (!items.empty()) {
                NewLineIfNotEmpty(description);
                description += std::format(L"{}\x10a\x108\x107" L"Common Materials: \x1\x1\x2{}", GW::EncStrings::ItemCommon, items);
            }

        }
        if (!rare_materials.empty()) {
            std::wstring items;
            for (auto i : rare_materials) {
                if (!i) continue;
                if (!items.empty())
                    items += L"\x2\x108\x107, \x1\x2";
                items += i;
            }
            if (!items.empty()) {
                NewLineIfNotEmpty(description);
                description += std::format(L"{}\x10a\x108\x107" L"Rare Materials: \x1\x1\x2{}", GW::EncStrings::ItemRare, items);
            }
        }

    }
    void AppendNicholasInfo(const uint32_t item_id, std::wstring& description) {
        const auto item = GW::Items::GetItemById(item_id);
        const auto nicholas_info = DailyQuests::GetNicholasItemInfo(item->name_enc);
        if (!nicholas_info) return;

        const auto collection_time = DailyQuests::GetTimestampFromNicholasTheTraveller(nicholas_info);
        const auto current_time = time(nullptr);
        NewLineIfNotEmpty(description);
        if (collection_time <= current_time) {
            description += std::format(L"{}\x10a\x108\x107Nicholas The Traveller collects {} of these right now!\x1\x1", GW::EncStrings::ItemUnique, nicholas_info->quantity);
        }
        else {
            description += std::format(L"{}\x10a\x108\x107Nicholas The Traveller collects {} of these in {}!\x1\x1", GW::EncStrings::ItemUnique, nicholas_info->quantity, PrintRelativeTime(collection_time));
        }
    }
    std::wstring tmp_item_description;
    void OnGetItemDescription(uint32_t item_id, uint32_t, uint32_t, uint32_t, wchar_t**, wchar_t** out_desc) 
    {
        if (!out_desc) return;
        if (*out_desc != tmp_item_description.data()) {
            tmp_item_description.assign(*out_desc ? *out_desc : L"");
        }
        AppendSalvageInfoDescription(item_id, tmp_item_description);
        AppendNicholasInfo(item_id, tmp_item_description);
        if (!tmp_item_description.empty()) {
            bool is_valid = GW::UI::IsValidEncStr(tmp_item_description.data());
            if (is_valid) {
                *out_desc = tmp_item_description.data();
            }
        }
    }

    GW::HookEntry UIMessage_HookEntry;
    std::wstring tmp_item_name_tag;
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kSetAgentNameTagAttribs: {
            auto packet = (GW::UI::AgentNameTagInfo*)wParam;
            if (!packet->underline)
                break;
            if (packet->extra_info_enc != tmp_item_name_tag.data()) {
                tmp_item_name_tag.assign(packet->extra_info_enc ? packet->extra_info_enc : L"");
            }
            const auto agent = static_cast<GW::AgentItem*>(GW::Agents::GetAgentByID(packet->agent_id));
            if (!(agent && agent->GetIsItemType()))
                break;
            if (agent->owner && agent->owner != GW::Agents::GetControlledCharacterId())
                break;
            const auto item_id = agent->item_id;
            AppendSalvageInfoDescription(item_id, tmp_item_name_tag);
            AppendNicholasInfo(item_id, tmp_item_name_tag);
            packet->extra_info_enc = tmp_item_name_tag.data();
        }

        }
    }
}

void SalvageInfoModule::Initialize()
{
    ToolboxModule::Initialize();
    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 200);
    GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry, GW::UI::UIMessage::kSetAgentNameTagAttribs, ::OnUIMessage);
}

void SalvageInfoModule::Terminate()
{
    ToolboxModule::Terminate();

    ItemDescriptionHandler::UnregisterDescriptionCallback(OnGetItemDescription);
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
}

void SalvageInfoModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
}

void SalvageInfoModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
}

void SalvageInfoModule::RegisterSettingsContent()
{
}
