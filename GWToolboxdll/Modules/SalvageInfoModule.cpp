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
    std::unordered_map<std::string, GW::ItemID> material_id_lookup {
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

    // The following regexes capture the entire text after commonsalvage or raresalvage: eg. | commonsalvage = [[Pile of Glittering Dust|Piles of Glittering Dust]]
    std::regex common_materials_regex("commonsalvage = (([a-zA-Z [\\]<>|'])*)");
    std::regex rare_materials_regex("raresalvage = (([a-zA-Z [\\]<>|'])*)");
    bool fetch_salvage_info = true;
    std::wstring modified_description;
    std::wstring color = L"@ItemCommon";
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

    void AsyncGetItemSimpleName(const std::wstring& name, std::wstring& res)
    {
        if (name.empty()) return;
        GW::UI::AsyncDecodeStr(name.c_str(), &res, GW::Constants::Language::English);
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

    std::string ConvertMaterialToString(const GW::ItemID itemId)
    {
        return std::to_string(static_cast<uint32_t>(itemId));
    }

    std::string BuildItemListString(const std::vector<GW::ItemID>& items) {
        std::string itemListString = "";
        if (items.size() == 0) {
            return itemListString;
        }

        for (size_t i = 0; i < items.size() - 1; i++) {
            itemListString += std::format("{}, ", ConvertMaterialToString(items[i]));
        }

        itemListString += ConvertMaterialToString(items[items.size() - 1]);
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
            if (material_id_lookup.contains(final_mat)) {
                final_info.push_back(material_id_lookup[final_mat]);
            }
        }

        return final_info;
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

        const auto salvage_info = maybe_info.value();
        std::wstring salvage_description;
        if (!salvage_info.crafting_materials.empty()) {
            salvage_description += std::format(L"\x2\x108\x107\n<c={}>Crafting Materials: {}</c>\x1", color, GuiUtils::StringToWString(BuildItemListString(salvage_info.crafting_materials)));
        }

        if (!salvage_info.rare_crafting_materials.empty()) {
            salvage_description += std::format(L"\x2\x108\x107\n<c={}>Rare Crafting Materials: {}</c>\x1", color, GuiUtils::StringToWString(BuildItemListString(salvage_info.rare_crafting_materials)));
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

        Log::Info(std::format("[{}] Caching crafting materials", base64_name, item_url).c_str());
        SalvageInfo salvageInfo;
        std::smatch matches;
        if (std::regex_search(response, matches, common_materials_regex) && matches.size() > 1) {
            const auto materials_list = GetMaterials(matches[1].str());
            salvageInfo.crafting_materials = materials_list;
        }

        if (std::regex_search(response, matches, rare_materials_regex) && matches.size() > 1) {
            const auto materials_list = GetMaterials(matches[1].str());
            salvageInfo.rare_crafting_materials = materials_list;
        }

        SaveToCache(name_enc, salvageInfo);
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
