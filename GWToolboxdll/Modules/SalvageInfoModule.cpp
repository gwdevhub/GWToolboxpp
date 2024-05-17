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

using nlohmann::json;
namespace GW {
    namespace EncStrings {
        const wchar_t* Bone = L"\x22D0\xBEB5\xC462\x64B5";
        const wchar_t* IronIngot = L"\x22EB\xC6B0\xBD46\x2DAD";
        const wchar_t* TannedHideSquare = L"\x22E3\xD3A9\xC22C\x285A";
        const wchar_t* Scale = L"\x22F0\x832C\xD6A3\x1382";
        const wchar_t* ChitinFragment = L"\x22F1\x9156\x8692\x497D";
        const wchar_t* BoltofCloth = L"\x22D4\x888E\x9089\x6EC8";
        const wchar_t* WoodPlank = L"\x22E8\xE46D\x8FE4\x5F8B";
        const wchar_t* GraniteSlab = L"\x22F2\xA623\xFAE8\xE5A";
        const wchar_t* PileofGlitteringDust = L"\x22D8\xA4A4\xED0D\x4304";
        const wchar_t* PlantFiber = L"\x22DD\xAC69\xDEA5\x73C5";
        const wchar_t* Feather = L"\x22DC\x8209\xAD29\xBBD";
        const wchar_t* FurSquare = L"\x22E4\xE5F4\xBBEF\x5A25";
        const wchar_t* BoltofLinen = L"\x22D5\x8371\x8ED5\x56B4";
        const wchar_t* BoltofDamask = L"\x22D6\xF04C\xF1E5\x5699";
        const wchar_t* BoltofSilk = L"\x22D7\xFD2A\xC85B\x58B3";
        const wchar_t* GlobofEctoplasm = L"\x22D9\xE7B8\xE9DD\x2322";
        const wchar_t* SteelIngot = L"\x22EC\xF12D\x87A7\x6460";
        const wchar_t* DeldrimorSteelIngot = L"\x22ED\xB873\x85A4\x74B";
        const wchar_t* MonstrousClaw = L"\x22D2\xCDC6\xEFC8\x3C99";
        const wchar_t* MonstrousEye = L"\x22DA\x9059\xD163\x2187";
        const wchar_t* MonstrousFang = L"\x22DB\x8DCE\xC3FA\x4A26";
        const wchar_t* Ruby = L"\x22E0\x93CC\x939C\x5286";
        const wchar_t* Sapphire = L"\x22E1\xB785\x866C\x34F6";
        const wchar_t* Diamond = L"\x22DE\xBB93\xABD4\x5439";
        const wchar_t* OnyxGemstone = L"\x22DF\xD425\xC093\x1CF4";
        const wchar_t* LumpofCharcoal = L"\x22D1\xDE2A\xED03\x2625";
        const wchar_t* ObsidianShard = L"\x22EA\xFDA9\xDE53\x2D16";
        const wchar_t* TemperedGlassVial = L"\x22E2\xCE9B\x8771\x7DC7";
        const wchar_t* LeatherSquare = L"\x22E5\x9758\xC5DD\x727";
        const wchar_t* ElonianLeatherSquare = L"\x22E6\xE8F4\xA898\x75CB";
        const wchar_t* VialofInk = L"\x22E7\xC1DA\xF2C1\x452A";
        const wchar_t* RollofParchment = L"\x22EE\xF65A\x86E6\x1C6C";
        const wchar_t* RollofVellum = L"\x22EF\xC588\x861D\x5BD3";
        const wchar_t* SpiritwoodPlank = L"\x22F3\xA11C\xC924\x5E15";
        const wchar_t* AmberChunk = L"\x55D0\xF8B7\xB108\x6018";
        const wchar_t* JadeiteShard = L"\x55D1\xD189\x845A\x7164";
    }
}

namespace {
    struct CraftingMaterial {
        uint32_t model_id; // Used to map to kamadan trade chat
        GuiUtils::EncString en_name; // Used to map to Guild Wars Wiki
        CraftingMaterial(uint32_t _model_id, const wchar_t* enc_name) : model_id(_model_id) {
            en_name.language(GW::Constants::Language::English);
            en_name.reset(enc_name);
            en_name.string(); // Trigger decode
        }
    };

    std::vector<CraftingMaterial*> materials = {};

    struct SalvageInfo {
        GuiUtils::EncString en_name; // Used to map to Guild Wars Wiki
        std::vector<CraftingMaterial*> common_crafting_materials;
        std::vector<CraftingMaterial*> rare_crafting_materials;
        std::chrono::time_point<std::chrono::steady_clock> retry_after;
        bool loading = false;
        bool failed = false;
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

    // Erases specified substring from string
    inline std::string& remove_substring(std::string& str, const std::string& toRemove)
    {
        size_t pos = 0;
        while ((pos = str.find(toRemove, pos)) != std::string::npos) {
            str.erase(pos, toRemove.length());
        }

        return str;
    }

    inline std::string& remove_char(std::string& str, char charToRemove)
    {
        str.erase(std::remove(str.begin(), str.end(), charToRemove), str.end());
        return str;
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
            Log::Error(std::format("Failed to fetch salvage info. Response: {}", response).c_str());
            info->loading = false;
            info->failed = true;
            SignalItemDescriptionUpdated(info->en_name.encoded().c_str());
            return;
        }

        const auto parse_materials = [](const std::string& cell_content, std::vector<CraftingMaterial*>& vec) {
                const std::regex link_regex("<a[^>]+title=\"([^\"]+)");
                auto links_begin = std::sregex_iterator(cell_content.begin(), cell_content.end(), link_regex);
                auto links_end = std::sregex_iterator();
                for (std::sregex_iterator j = links_begin; j != links_end; ++j)
                {
                    const auto material_name = j->str(1);
                    const auto found = std::ranges::find_if(materials, [material_name](auto* c) {
                        /*
                        * We need to do a pretty lax compare here for materials.
                        * We'll compare case-insensitive.
                        * We deal with Ruby/Rubies by removing "ies" and 'y'.
                        * Finally, to deal with plural versions, we remove all occurences of 's'.
                        * After all the transformations, we'll have Bone => bone, Bones => bone, Monstrous Eye => montrou ee, Ruby => rub, Rubies => rub, etc.
                        */
                        auto single_material_name = c->en_name.string();
                        auto parsed_material_name = material_name;
                        std::transform(single_material_name.begin(), single_material_name.end(), single_material_name.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        std::transform(parsed_material_name.begin(), parsed_material_name.end(), parsed_material_name.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        single_material_name = remove_char(remove_char(remove_substring(single_material_name, "ies"), 'y'), 's');
                        parsed_material_name = remove_char(remove_char(remove_substring(parsed_material_name, "ies"), 'y'), 's');
                        return single_material_name == parsed_material_name;
                        });
                    if (found != materials.end()) {
                        // Add the material to the list only if the material doesn't already exist
                        const auto in_list = std::find_if(vec.begin(), vec.end(), [material_name](auto* c) {
                            return c->en_name.string() == material_name;
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
        info->retry_after = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        Resources::Download(url, OnWikiContentDownloaded, info, std::chrono::days(1));
    }

    SalvageInfo* GetSalvageInfo(const wchar_t* single_item_name) {
        if (!single_item_name)
            return nullptr;
        const auto found = salvage_info_by_single_item_name.find(single_item_name);
        // If the item does not exist, or it exists but the fetching failed and a timeout has passed, attempt to fetch salvage info
        if (found == salvage_info_by_single_item_name.end() ||
            (found->second->failed && std::chrono::steady_clock::now() >= found->second->retry_after)) {
            if (found != salvage_info_by_single_item_name.end()) {
                delete found->second;
            }
            // Need to fetch info for this item.
            auto salvage_info = new SalvageInfo();
            salvage_info->en_name.language(GW::Constants::Language::English);
            salvage_info->en_name.reset(single_item_name);
            salvage_info_by_single_item_name[single_item_name] = salvage_info;
            salvage_info->loading = true;
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

        const auto salvage_info = GetSalvageInfo(item->name_enc);
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
                items += i->en_name.encoded();
            }

            description += std::format(L"\x2\x102\x2\x108\x107<c=@ItemCommon>Common Materials:</c> \x1\x2{}", items);
        }
        if (!salvage_info->rare_crafting_materials.empty()) {
            std::wstring items;
            for (auto i : salvage_info->rare_crafting_materials) {
                if (!items.empty()) 
                    items += L"\x2\x108\x107, \x1\x2";
                items += i->en_name.encoded();
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
