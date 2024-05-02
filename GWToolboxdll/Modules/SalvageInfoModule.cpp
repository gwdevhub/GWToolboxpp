#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <ctime>
#include <regex>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>

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
    std::queue<std::tuple<uint32_t, std::wstring>> fetch_queue;

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

    void LoadFromCache()
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

    void SaveToCache()
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

    std::string CleanMaterialsInfo(std::string raw_materials_info) {
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
        const auto mats = Split(raw_materials_info, " <br> ");
        
        // If the material info contains a '|', we want to split by that and choose the second option (plural)
        std::vector<std::string> final_mats;
        for (size_t i = 0; i < mats.size(); i++) {
            const auto tokens = Split(mats[i], "|");
            final_mats.push_back(tokens[tokens.size() - 1]);
        }

        // Generate the final string, adding a ',' for all elements between the first and last
        std::string final_info;
        for (size_t i = 0; i < final_mats.size() - 1; i++) {
            final_info.append(std::format("{}, ", final_mats[i]));
        }

        if (final_mats.size() > 0) {
            final_info.append(final_mats[final_mats.size() - 1]);
        }

        return final_info;
    }

    std::wstring FetchDescription(GW::Item* item) {
        if (!item) {
            return L"";
        }

        const auto iter = salvage_info_cache.find(std::to_string((uint32_t)item->item_id));
        // Could not find the item. We need to fetch the item name and then fetch the salvage info from wiki
        if (iter == salvage_info_cache.end()) {
            const auto item_id = static_cast<uint32_t>(item->item_id);
            const auto name_str = std::wstring(item->name_enc);
            fetch_queue.push(std::make_tuple(item_id, name_str));
            return L"";
        }
        
        const auto value = static_cast<std::string>(iter.value());
        return GuiUtils::StringToWString(value);
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

    LoadFromCache();
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

    const auto tuple = fetch_queue.front();
    const auto item_id = std::get<0>(tuple);
    const auto name_enc = std::get<1>(tuple);
    fetch_queue.pop();

    //Detect duplicate fetch requests and ignore them
    if (salvage_info_cache.find(std::to_string(item_id)) != salvage_info_cache.end()) {
        return;
    }

    fetching_info = true;
    name_cache.clear();
    Log::Info(std::format("[{}] Fetching name", item_id).c_str());
    // Async Get Name sometimes hangs if not run from the game thread
    GW::GameThread::Enqueue([name_enc] {
        AsyncGetItemSimpleName(name_enc, name_cache);
    });

    // Free the main thread and move the work on a background thread. When the work finishes, the main thread can process another item from the queue
    Resources::EnqueueWorkerTask([item_id] {
        while (name_cache.empty()) {
            Sleep(16);
        }

        Log::Info(std::format("[{}] Fetching crafting materials for {}", item_id, GuiUtils::WStringToString(name_cache)).c_str());
        const auto item_url = GuiUtils::WikiTemplateUrlFromTitle(name_cache);

        Log::Info(std::format("[{}] Fetching crafting materials from {}", item_id, item_url).c_str());
        std::string response;
        if (!Resources::Download(item_url, response)) {
            Log::Info(std::format("Failed to fetch item data from {}. Error: {}", item_url, response).c_str());
            // salvage_info_cache[std::to_string(item_id)] = ""; // Sometimes download fails for items
            fetching_info = false;
            return;
        }

        Log::Info(std::format("[{}] Caching crafting materials", item_id, item_url).c_str());
        std::wstring salvage_info;
        std::smatch matches;
        if (std::regex_search(response, matches, common_materials_regex) && matches.size() > 1) {
            const auto mat_info = CleanMaterialsInfo(matches[1].str());
            salvage_info += std::format(L"\x2\x108\x107\n<c={}>Common Materials: {}\x1", color, GuiUtils::StringToWString(mat_info));
        }

        if (std::regex_search(response, matches, rare_materials_regex) && matches.size() > 1) {
            auto mat_info = CleanMaterialsInfo(matches[1].str());
            salvage_info += std::format(L"\x2\x108\x107\n<c={}>Rare Materials: {}\x1", color, GuiUtils::StringToWString(mat_info));
        }

        salvage_info_cache[std::to_string(item_id)] = GuiUtils::WStringToString(salvage_info);
        SaveToCache();
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
