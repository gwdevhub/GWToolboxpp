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

#include <Modules/PriceCheckerModule.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Modules/GameSettings.h>
#include <Modules/SalvageInfoModule.h>

#include <Utils/GuiUtils.h>

using nlohmann::json;

namespace {
    std::unordered_map<GW::ItemID, std::wstring> id_to_enc_names{
        {GW::Constants::ItemID::Bone, L"\x22D0\xBEB5\xC462\x64B5"},
        {GW::Constants::ItemID::IronIngot, L"\x22EB\xC6B0\xBD46\x2DAD"},
        {GW::Constants::ItemID::TannedHideSquare, L"\x22E3\xD3A9\xC22C\x285A"},
        {GW::Constants::ItemID::ChitinFragment, L"\x22F1\x9156\x8692\x497D"},
        {GW::Constants::ItemID::BoltofCloth, L"\x22D4\x888E\x9089\x6EC8"},
        {GW::Constants::ItemID::WoodPlank, L"\x22E8\xE46D\x8FE4\x5F8B"},
        {GW::Constants::ItemID::GraniteSlab, L"\x22F2\xA623\xFAE8\xE5A"},
        {GW::Constants::ItemID::PileofGlitteringDust, L"\x22D8\xA4A4\xED0D\x4304"},
        {GW::Constants::ItemID::PlantFiber, L"\x22DD\xAC69\xDEA5\x73C5"},
        {GW::Constants::ItemID::Feather, L"\x22DC\x8209\xAD29\xBBD"},
        {GW::Constants::ItemID::FurSquare, L"\x22E4\xE5F4\xBBEF\x5A25"},
        {GW::Constants::ItemID::BoltofLinen, L"\x22D5\x8371\x8ED5\x56B4"},
        {GW::Constants::ItemID::BoltofDamask, L"\x22D6\xF04C\xF1E5\x5699"},
        {GW::Constants::ItemID::BoltofSilk, L"\x22D7\xFD2A\xC85B\x58B3"},
        {GW::Constants::ItemID::GlobofEctoplasm, L"\x22D9\xE7B8\xE9DD\x2322"},
        {GW::Constants::ItemID::SteelIngot, L"\x22EC\xF12D\x87A7\x6460"},
        {GW::Constants::ItemID::DeldrimorSteelIngot, L"\x22ED\xB873\x85A4\x74B"},
        {GW::Constants::ItemID::MonstrousClaw, L"\x22D2\xCDC6\xEFC8\x3C99"},
        {GW::Constants::ItemID::MonstrousEye, L"\x22DA\x9059\xD163\x2187"},
        {GW::Constants::ItemID::MonstrousFang, L"\x22DB\x8DCE\xC3FA\x4A26"},
        {GW::Constants::ItemID::Ruby, L"\x22E0\x93CC\x939C\x5286"},
        {GW::Constants::ItemID::Sapphire, L"\x22E1\xB785\x866C\x34F6"},
        {GW::Constants::ItemID::Diamond, L"\x22DE\xBB93\xABD4\x5439"},
        {GW::Constants::ItemID::OnyxGemstone, L"\x22DF\xD425\xC093\x1CF4"},
        {GW::Constants::ItemID::LumpofCharcoal, L"\x22D1\xDE2A\xED03\x2625"},
        {GW::Constants::ItemID::ObsidianShard, L"\x22EA\xFDA9\xDE53\x2D16"},
        {GW::Constants::ItemID::TemperedGlassVial, L"\x22E2\xCE9B\x8771\x7DC7"},
        {GW::Constants::ItemID::LeatherSquare, L"\x22E5\x9758\xC5DD\x727"},
        {GW::Constants::ItemID::ElonianLeatherSquare, L"\x22E6\xE8F4\xA898\x75CB"},
        {GW::Constants::ItemID::VialofInk, L"\x22E7\xC1DA\xF2C1\x452A"},
        {GW::Constants::ItemID::RollofParchment, L"\x22EE\xF65A\x86E6\x1C6C"},
        {GW::Constants::ItemID::RollofVellum, L"\x22EF\xC588\x861D\x5BD3"},
        {GW::Constants::ItemID::SpiritwoodPlank, L"\x22F3\xA11C\xC924\x5E15"},
        {GW::Constants::ItemID::AmberChunk, L"\x55D0\xF8B7\xB108\x6018"},
        {GW::Constants::ItemID::JadeiteShard, L"\x55D1\xD189\x845A\x7164"}
    };

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

    // The following regex captures the text between <gallery></gallery> in the disambiguation page: eg. <gallery>Image : Inscribed Chakram(metal).jpg | [[Inscribed Chakram(metal)]] Image : Inscribed Chakram(wooden).jpg | [[Inscribed Chakram(wooden)]] Image : Inscribed Chakram(diamond).jpg |[[Inscribed Chakram(diamond)]]</ gallery>
    const static std::regex gallery_regex(R"(<gallery>([\s\S]*?)</gallery>)");
    // The follwing regex captures the text between [[ ]] in the disambiguation page: eg. Image:Eerie Focus (Nightfall).jpg|[[Eerie Focus (Nightfall)]]
    const static std::regex disambig_entries_regex(R"(\[\[([^\[\]]+)\]\])");
    // The following regexes capture the entire text after commonsalvage or raresalvage: eg. | commonsalvage = [[Pile of Glittering Dust|Piles of Glittering Dust]]
    const static std::regex common_materials_regex("commonsalvage = (([a-zA-Z [\\]<>|'])*)");
    const static std::regex rare_materials_regex("raresalvage = (([a-zA-Z [\\]<>|'])*)");
    const static std::string disambig = "{{disambig}}";
    const static std::wstring color = L"@ItemCommon";
    const static std::wstring high_price_color = L"@ItemRare";
    bool fetch_salvage_info = true;
    float high_price_threshold = 300;
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
        const auto iter = salvage_info_cache.find(key_str);
        if (iter == salvage_info_cache.end() || !iter->is_object()) {
            return std::optional<SalvageInfo>();
        }

        try {
            SalvageInfo salvageInfo;
            from_json(iter.value(), salvageInfo);
            return salvageInfo;
        }
        catch (std::exception ex) {
            Log::Error(std::format("[{}] Exception when fetching salvage info for key. Details:{}", key_str, ex.what()).c_str());
            return std::optional<SalvageInfo>();
        }
    }

    void SaveToCache(std::wstring key, SalvageInfo value) {
        const auto key_str = GetBase64EncodedName(key);
        salvage_info_cache[key_str] = value;
    }

    std::wstring ConvertMaterialToString(const GW::ItemID item_id, const GW::Constants::Language language)
    {
        const auto mat_name_iter = id_to_enc_names.find(item_id);
        if (mat_name_iter == id_to_enc_names.end()) {
            return L"";
        }

        const auto &enc_name = mat_name_iter->second;
        std::wstring simple_name = L"";
        AsyncGetItemSimpleName(enc_name, simple_name, language);
        while (enc_name.empty()) {
        }

        return simple_name;
    }

    std::wstring BuildItemListString(const std::vector<GW::ItemID>& items, const GW::Constants::Language language) {
        std::wstring itemListString = L"";
        if (items.size() == 0) {
            return itemListString;
        }

        for (size_t i = 0; i < items.size(); i++) {
            const auto material = items[i];
            const auto price = PriceCheckerModule::Instance().GetPriceById(std::to_string((uint32_t)material).c_str());
            itemListString += std::format(L"\x2\x108\x107\n<c={}>{}</c>\x1", price > high_price_threshold ? high_price_color : color, ConvertMaterialToString(material, language));
        }

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

        const auto salvage_info = maybe_info.value();
        std::wstring salvage_description;
        if (!salvage_info.crafting_materials.empty()) {
            salvage_description += BuildItemListString(salvage_info.crafting_materials, language);
        }

        if (!salvage_info.rare_crafting_materials.empty()) {
            salvage_description += BuildItemListString(salvage_info.rare_crafting_materials, language);
        }

        return salvage_description;
    }

    void UpdateDescription(const uint32_t item_id, wchar_t** description_out)
    {
        if (!(description_out && *description_out)) return;
        if (!fetch_salvage_info) return;

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
    // Async Get Name sometimes hangs if not run from the game thread
    GW::GameThread::Enqueue([name_enc] {
        AsyncGetItemSimpleName(name_enc, name_cache);
    });

    // Free the main thread and move the work on a background thread. When the work finishes, the main thread can process another item from the queue
    Resources::EnqueueWorkerTask([name_enc, base64_name] {
        while (name_cache.empty()) {
            Sleep(16);
        }

        const auto item_url = GuiUtils::WikiTemplateUrlFromTitle(name_cache);
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
            std::smatch matches;
            if (!std::regex_search(response, matches, gallery_regex) || matches.size() <= 1) {
                fetching_info = false;
                return;
            }

            auto sub_names = matches[1].str();
            while (std::regex_search(sub_names, matches, disambig_entries_regex))
            {
                if (matches.size() > 0) {
                    const auto sub_item_url = GuiUtils::WikiTemplateUrlFromTitle(matches[matches.size() - 1].str());
                    std::string sub_response; // We need to keep the previous response so that we can process the disambig entries one by one
                    if (!Resources::Download(sub_item_url, sub_response)) {
                        Log::Info(std::format("Failed to fetch item data from {}. Error: {}", sub_item_url, sub_response).c_str());
                        continue;
                    }

                    ParseItemWikiResponse(sub_response, salvage_info);
                }

                sub_names = matches.suffix().str();
            }
        }
        else {
            ParseItemWikiResponse(response, salvage_info);
        }

        Log::Info(std::format("[{}] Retrieved materials", base64_name, item_url).c_str());
        SaveToCache(name_enc, salvage_info);
        SaveLocalCache();
        fetching_info = false;
    });
}

void SalvageInfoModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(fetch_salvage_info);
    SAVE_FLOAT(high_price_threshold);
}

void SalvageInfoModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    LOAD_BOOL(fetch_salvage_info);
    LOAD_FLOAT(high_price_threshold);
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
            ImGui::SliderFloat("Salvage material price threshold", &high_price_threshold, 100, 5000);
            ImGui::ShowHelp("The item description will color gold the possible salvage material if its price is above the threshold");
        },
        0.9f
    );
}
