#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <base64.h>
#include <ctime>
#include <regex>

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

#include <Modules/ItemDescriptionHandler.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Modules/GameSettings.h>
#include <Modules/SalvageInfoModule.h>

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <Constants/EncStrings.h>

using nlohmann::json;
namespace {
    // If requesting info from gww for an item fails, how long should we wait before retrying?
    constexpr clock_t salvage_info_retry_interval = CLOCKS_PER_SEC * 30;

    struct CraftingMaterial {
        uint32_t model_id; // Used to map to kamadan trade chat
        const wchar_t* enc_name;
        std::wstring en_name; // Used to map to Guild Wars Wiki
        std::wstring en_name_plural;
        bool decoding_en_name = false;
        bool decoding_en_name_plural = false;
        CraftingMaterial(uint32_t _model_id, const wchar_t* _enc_name) : model_id(_model_id), enc_name(_enc_name) {

            decoding_en_name = true;
            GW::UI::AsyncDecodeStr(enc_name, OnNameDecoded, this, GW::Constants::Language::English);

            const auto enc_name_plural = std::format(L"\xA35\x101\x100\x10A{}\x1", enc_name);
            decoding_en_name_plural = true;
            GW::UI::AsyncDecodeStr(enc_name_plural.c_str(), OnPluralNameDecoded, this, GW::Constants::Language::English);
        }
        static void OnNameDecoded(void* param, const wchar_t* s) {
            auto ctx = (CraftingMaterial*)param;
            ctx->en_name = s;
            ctx->decoding_en_name = false;
        }
        static void OnPluralNameDecoded(void* param, const wchar_t* s) {
            auto ctx = (CraftingMaterial*)param;
            ctx->en_name_plural = s;
            ctx->en_name_plural = ctx->en_name_plural.substr(2); // Remove "0 " prefix
            ctx->decoding_en_name_plural = false;
        }
        ~CraftingMaterial() {
            ASSERT(!decoding_en_name_plural && !decoding_en_name);
        }
    };

    std::vector<CraftingMaterial*> materials = {};

    struct SalvageInfo {
        GuiUtils::EncString en_name; // Used to map to Guild Wars Wiki
        std::vector<CraftingMaterial*> common_crafting_materials;
        std::vector<CraftingMaterial*> rare_crafting_materials;
        clock_t last_retry = salvage_info_retry_interval * -1;
        bool loading = false;
        bool success = false;

        ~SalvageInfo()
        {
            ASSERT(!loading);
        }
    };

    std::unordered_map<std::wstring, SalvageInfo*> salvage_info_by_single_item_name;

    const char* ws = " \t\n\r\f\v";

    // trim from end of string (right)
    inline std::string& rtrim(std::string& s, const char* t = ws)
    {
        s.erase(s.find_last_not_of(t) + 1);
        return s;
    }

    // trim from beginning of string (left)
    inline std::string& ltrim(std::string& s, const char* t = ws)
    {
        s.erase(0, s.find_first_not_of(t));
        return s;
    }

    // trim from both ends of string (right then left)
    inline std::string& trim(std::string& s, const char* t = ws)
    {
        return ltrim(rtrim(s, t), t);
    }

    //Converts signed char to unsigned char arrays
    inline std::vector<unsigned char> convert_to_unsigned(const std::vector<char>& input)
    {
        std::vector<unsigned char> output;
        output.reserve(input.size()); // Reserve space to improve efficiency
        for (char c : input) {
            output.push_back(static_cast<unsigned char>(c));
        }
        return output;
    };

    // Converts unsigned char to signed char arrays
    inline std::vector<char> convert_to_signed(const std::vector<unsigned char>& input)
    {
        std::vector<char> output;
        output.reserve(input.size()); // Reserve space to improve efficiency
        for (unsigned char c : input) {
            output.push_back(static_cast<char>(c));
        }
        return output;
    };

    // Make sure you pass valid html e.g. start with a < tag
    std::string& strip_tags(std::string& html) {
        while (1)
        {
            auto startpos = html.find("<");
            if (startpos == std::string::npos)
                break;
            auto endpos = html.find(">", startpos) + 1;
            if (endpos == std::string::npos)
                break;
            html.erase(startpos, endpos - startpos);
        }

        return html;
    }
    std::string& from_html(std::string& html) {
        strip_tags(html);
        trim(html);
        return html;
    }

    void SignalItemDescriptionUpdated(const wchar_t* enc_name) {
        // Now we've got the wiki info parsed, trigger an item update ui message; this will refresh the item tooltip
        GW::GameThread::Enqueue([enc_name] {
            const auto item = GW::Items::GetHoveredItem();
            if (item && wcscmp(item->name_enc, enc_name) == 0) {
                GW::UI::SendUIMessage(GW::UI::UIMessage::kItemUpdated, item);
            }
            });
    }

    void OnWikiContentDownloaded(bool success, const std::string& response, void* wparam) {
        auto* info = (SalvageInfo*)wparam;
        if (!success) {
            Log::Error("Failed to fetch salvage info. Response: %s", response.c_str());
            info->loading = false;
            SignalItemDescriptionUpdated(info->en_name.encoded().c_str());
            return;
        }

        const auto parse_materials = [](const std::string& cell_content, std::vector<CraftingMaterial*>& vec) {
                const std::regex link_regex("<a[^>]+title=\"([^\"]+)");
                auto links_begin = std::sregex_iterator(cell_content.begin(), cell_content.end(), link_regex);
                auto links_end = std::sregex_iterator();
                for (std::sregex_iterator j = links_begin; j != links_end; ++j)
                {
                    const auto material_name = GuiUtils::StringToWString(j->str(1));

                    const auto found = std::ranges::find_if(materials, [material_name](auto* c) {
                        return c->en_name == material_name || c->en_name_plural == material_name;
                        });
                    if (found != materials.end()) {
                        // Add the material to the list only if the material doesn't already exist
                        const auto in_list = std::find_if(vec.begin(), vec.end(), [material_name](auto* c) {
                            return c->en_name == material_name || c->en_name_plural == material_name;
                        });
                        if (in_list == vec.end()) {
                            vec.push_back(*found);
                        }
                    }
                }
            };

        const std::regex infobox_regex("<table[^>]+>([\\s\\S]*)</table>");
        std::smatch m;
        if (std::regex_search(response, m, infobox_regex)) {
            std::string infobox_content = m[1].str();

            const auto sub_links_regex = std::regex(R"delim(<a href="\/wiki\/File:[\s\S]*?title="([\s\S]*?)">)delim");
            const auto disambig_regex = std::regex("This disambiguation page");
            if (std::regex_search(infobox_content, m, disambig_regex)) {
                // Detected a disambiguation page. We need to search for materials in the hrefs listed on this page
                std::unordered_set<std::string> sub_urls;
                std::string::const_iterator sub_search_start(response.cbegin());
                while (std::regex_search(sub_search_start, response.cend(), m, sub_links_regex)) {
                    const auto entry = m[1].str();
                    // Search for entries that contain the name of the item, skipping the SpecialWhatLinksHere entry
                    if (entry.contains(info->en_name.string()) && !entry.contains("Special:WhatLinksHere")) {
                        sub_urls.emplace(entry);
                    }

                    sub_search_start = m.suffix().first; // Update the start position
                }

                // Fetch materials of the sub urls and add them to the salvage info struct
                if (sub_urls.size() > 0) {
                    for (const auto sub_name : sub_urls) {
                        const auto url = GuiUtils::WikiUrl(sub_name);
                        Resources::Download(url, OnWikiContentDownloaded, info, std::chrono::days(30));
                    }
                    
                    return;
                }
            }

            const auto weapon_page_regex = std::regex(R"(This article is about [\s\S]*? type[\s\S]*?same name)");
            if (std::regex_search(response, m, weapon_page_regex)) {
                // Detected weapon type page. We need to go to the weapon with same name page and fetch materials from there
                const auto expected_token = std::format("{} (weapon)", info->en_name.string());
                std::unordered_set<std::string> sub_urls;
                auto sub_search_start(response.cbegin());
                while (std::regex_search(sub_search_start, response.cend(), m, sub_links_regex)) {
                    const auto entry = m[1].str();
                    // Search for entries that match [WeaponName] (weapon) token
                    if (entry == expected_token) {
                        sub_urls.emplace(entry);
                    }

                    sub_search_start = m.suffix().first; // Update the start position
                }

                // Fetch materials of the sub urls and add them to the salvage info struct
                if (sub_urls.size() > 0) {
                    for (const auto sub_name : sub_urls) {
                        const auto url = GuiUtils::WikiUrl(sub_name);
                        Resources::Download(url, OnWikiContentDownloaded, info, std::chrono::days(30));
                    }

                    return;
                }
            }

            // Iterate over all skills in this list.
            static const std::regex infobox_row_regex(
                "(?:<tr>|<tr[^>]+>)[\\s\\S]*?(?:<th>|<th[^>]+>)([\\s\\S]*?)</th>[\\s\\S]*?(?:<td>|<td[^>]+>)([\\s\\S]*?)</td>[\\s\\S]*?</tr>"
            );
            auto words_begin = std::sregex_iterator(infobox_content.begin(), infobox_content.end(), infobox_row_regex);
            auto words_end = std::sregex_iterator();
            for (std::sregex_iterator i = words_begin; i != words_end; ++i)
            {
                auto key = i->str(1);
                from_html(key);
                const auto val = i->str(2);
                if (key == "Common salvage") {
                    parse_materials(val, info->common_crafting_materials);
                }
                if (key == "Rare salvage") {
                    parse_materials(val, info->rare_crafting_materials);
                }
            }
        }
        info->success = true;
        info->loading = false;
        SignalItemDescriptionUpdated(info->en_name.encoded().c_str());
    }

    // Run on worker thread, so we can Sleep!
    void FetchSalvageInfoFromGuildWarsWiki(SalvageInfo* info) {
        info->en_name.string();
        while (info->en_name.IsDecoding()) {
            // @Cleanup: this should never hang, but should we handle it?
            Sleep(16);
        }
        const auto url = GuiUtils::WikiUrl(info->en_name.string());
        Resources::Download(url, OnWikiContentDownloaded, info, std::chrono::days(1));
    }

    SalvageInfo* GetSalvageInfo(const wchar_t* single_item_name) {
        if (!single_item_name)
            return nullptr;
        auto found = salvage_info_by_single_item_name.find(single_item_name);
        // If the item does not exist, or it exists but the fetching failed and a timeout has passed, attempt to fetch salvage info
        if (found == salvage_info_by_single_item_name.end()) {
            // Need to fetch info for this item.
            auto salvage_info = new SalvageInfo();
            salvage_info->en_name.language(GW::Constants::Language::English);
            salvage_info->en_name.reset(single_item_name);
            salvage_info_by_single_item_name[single_item_name] = salvage_info;
            found = salvage_info_by_single_item_name.find(single_item_name);
        }
        const auto salvage_info = found->second;
        // If the item exists but it failed and the timeout has expired, attempt to fetch salvage info again
        if (!salvage_info->loading && !salvage_info->success && TIMER_DIFF(salvage_info->last_retry) > salvage_info_retry_interval) {
            salvage_info->loading = true;
            salvage_info->last_retry = TIMER_INIT();
            Resources::EnqueueWorkerTask([salvage_info] {
                FetchSalvageInfoFromGuildWarsWiki(salvage_info);
            });

        }
        return salvage_info_by_single_item_name[single_item_name];
    }

    void AppendSalvageInfoDescription(const uint32_t item_id, std::wstring& description) {
        const auto item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));

        if (!(item && item->name_enc && *item->name_enc && item->IsSalvagable(false))) {
            return;
        }

        if (item->type == GW::Constants::ItemType::Boots ||
            item->type == GW::Constants::ItemType::Gloves ||
            item->type == GW::Constants::ItemType::Leggings ||
            item->type == GW::Constants::ItemType::Headpiece ||
            item->type == GW::Constants::ItemType::Chestpiece) {
            return;
        }

        const auto name_without_mods = ItemDescriptionHandler::GetItemEncNameWithoutMods(item);

        const auto salvage_info = GetSalvageInfo(name_without_mods.c_str());
        if (!salvage_info)
            return;

        if (description.empty())
            description += L"\x101";

        if (salvage_info->loading) {
            description += L"\x2\x102\x2\x108\x107" L"Fetching salvage info...\x1";
        }

        if (!salvage_info->common_crafting_materials.empty()) {
            std::wstring items;
            for (auto i : salvage_info->common_crafting_materials) {
                if (!items.empty())
                    items += L"\x2\x108\x107, \x1\x2";
                items += i->enc_name;
            }

            description += std::format(L"\x2\x102\x2\x108\x107<c=@ItemCommon>Common Materials:</c> \x1\x2{}", items);
        }
        if (!salvage_info->rare_crafting_materials.empty()) {
            std::wstring items;
            for (auto i : salvage_info->rare_crafting_materials) {
                if (!items.empty()) 
                    items += L"\x2\x108\x107, \x1\x2";
                items += i->enc_name;
            }

            description += std::format(L"\x2\x102\x2\x108\x107<c=@ItemRare>Rare Materials:</c> \x1\x2{}", items);
        }
    }
    std::wstring tmp_item_description;
    void OnGetItemDescription(uint32_t item_id, uint32_t, uint32_t, uint32_t, wchar_t**, wchar_t** out_desc) 
    {
        if (!(out_desc && *out_desc)) return;
        if (*out_desc != tmp_item_description.data()) {
            tmp_item_description.assign(*out_desc);
        }
        AppendSalvageInfoDescription(item_id, tmp_item_description);
        *out_desc = tmp_item_description.data();
    }
}

void SalvageInfoModule::Initialize()
{
    ToolboxModule::Initialize();

    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 200);
    GW::GameThread::Enqueue([]() {
        materials = {
            new CraftingMaterial(GW::Constants::ItemID::Bone, GW::EncStrings::Bone),
            new CraftingMaterial(GW::Constants::ItemID::IronIngot, GW::EncStrings::IronIngot),
            new CraftingMaterial(GW::Constants::ItemID::TannedHideSquare, GW::EncStrings::TannedHideSquare),
            new CraftingMaterial(GW::Constants::ItemID::Scale, GW::EncStrings::Scale),
            new CraftingMaterial(GW::Constants::ItemID::ChitinFragment, GW::EncStrings::ChitinFragment),
            new CraftingMaterial(GW::Constants::ItemID::BoltofCloth, GW::EncStrings::BoltofCloth),
            new CraftingMaterial(GW::Constants::ItemID::WoodPlank, GW::EncStrings::WoodPlank),
            new CraftingMaterial(GW::Constants::ItemID::GraniteSlab, GW::EncStrings::GraniteSlab),
            new CraftingMaterial(GW::Constants::ItemID::PileofGlitteringDust, GW::EncStrings::PileofGlitteringDust),
            new CraftingMaterial(GW::Constants::ItemID::PlantFiber, GW::EncStrings::PlantFiber),
            new CraftingMaterial(GW::Constants::ItemID::Feather, GW::EncStrings::Feather),
            new CraftingMaterial(GW::Constants::ItemID::FurSquare, GW::EncStrings::FurSquare),
            new CraftingMaterial(GW::Constants::ItemID::BoltofLinen, GW::EncStrings::BoltofLinen),
            new CraftingMaterial(GW::Constants::ItemID::BoltofDamask, GW::EncStrings::BoltofDamask),
            new CraftingMaterial(GW::Constants::ItemID::BoltofSilk, GW::EncStrings::BoltofSilk),
            new CraftingMaterial(GW::Constants::ItemID::GlobofEctoplasm, GW::EncStrings::GlobofEctoplasm),
            new CraftingMaterial(GW::Constants::ItemID::SteelIngot, GW::EncStrings::SteelIngot),
            new CraftingMaterial(GW::Constants::ItemID::DeldrimorSteelIngot, GW::EncStrings::DeldrimorSteelIngot),
            new CraftingMaterial(GW::Constants::ItemID::MonstrousClaw, GW::EncStrings::MonstrousClaw),
            new CraftingMaterial(GW::Constants::ItemID::MonstrousEye, GW::EncStrings::MonstrousEye),
            new CraftingMaterial(GW::Constants::ItemID::MonstrousFang, GW::EncStrings::MonstrousFang),
            new CraftingMaterial(GW::Constants::ItemID::Ruby, GW::EncStrings::Ruby),
            new CraftingMaterial(GW::Constants::ItemID::Sapphire, GW::EncStrings::Sapphire),
            new CraftingMaterial(GW::Constants::ItemID::Diamond, GW::EncStrings::Diamond),
            new CraftingMaterial(GW::Constants::ItemID::OnyxGemstone, GW::EncStrings::OnyxGemstone),
            new CraftingMaterial(GW::Constants::ItemID::LumpofCharcoal, GW::EncStrings::LumpofCharcoal),
            new CraftingMaterial(GW::Constants::ItemID::ObsidianShard, GW::EncStrings::ObsidianShard),
            new CraftingMaterial(GW::Constants::ItemID::TemperedGlassVial, GW::EncStrings::TemperedGlassVial),
            new CraftingMaterial(GW::Constants::ItemID::LeatherSquare, GW::EncStrings::LeatherSquare),
            new CraftingMaterial(GW::Constants::ItemID::ElonianLeatherSquare, GW::EncStrings::ElonianLeatherSquare),
            new CraftingMaterial(GW::Constants::ItemID::VialofInk, GW::EncStrings::VialofInk),
            new CraftingMaterial(GW::Constants::ItemID::RollofParchment, GW::EncStrings::RollofParchment),
            new CraftingMaterial(GW::Constants::ItemID::RollofVellum, GW::EncStrings::RollofVellum),
            new CraftingMaterial(GW::Constants::ItemID::SpiritwoodPlank, GW::EncStrings::SpiritwoodPlank),
            new CraftingMaterial(GW::Constants::ItemID::AmberChunk, GW::EncStrings::AmberChunk),
            new CraftingMaterial(GW::Constants::ItemID::JadeiteShard, GW::EncStrings::JadeiteShard)
        };
    });
}

void SalvageInfoModule::Terminate()
{
    ToolboxModule::Terminate();

    ItemDescriptionHandler::UnregisterDescriptionCallback(OnGetItemDescription);

    for (auto m : materials) {
        delete m;
    }
    materials.clear();

    for (const auto &tuple : salvage_info_by_single_item_name) {
        delete tuple.second;
    }
    salvage_info_by_single_item_name.clear();
}

bool SalvageInfoModule::CanTerminate() {
    // Cannot terminate while waiting for the item fetching to finish
    for (const auto &tuple : salvage_info_by_single_item_name) {
        if (tuple.second->loading) {
            return false;
        }
    }

    return true;
}

void SalvageInfoModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
}

void SalvageInfoModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
}

void SalvageInfoModule::RegisterSettingsContent()
{
}
