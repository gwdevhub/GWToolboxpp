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
        // TODO: fill this in
    }
}

namespace {

    bool fetch_salvage_info = true;

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
        bool loading = false;
    };

    std::unordered_map<std::wstring, SalvageInfo*> salvage_info_by_model_id;

    std::wstring item_description_appended; // Not thread safe

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


    void OnWikiContentDownloaded(bool success, const std::string& response, void* wparam) {
        if (!success) {
            Log::Error("Failed to download wiki content for item");
            return;
        }
        auto* info = (SalvageInfo*)wparam;
        // TODO
        Log::Info("TODO: Parse wiki content for %s", info->en_name.string().c_str());

        const auto parse_materials = [](const std::string& cell_content, std::vector<CraftingMaterial*>& vec) {
                const std::regex link_regex("<a[^>]+title=\"([^\"]+)");
                auto links_begin = std::sregex_iterator(cell_content.begin(), cell_content.end(), link_regex);
                auto links_end = std::sregex_iterator();
                for (std::sregex_iterator j = links_begin; j != links_end; ++j)
                {
                    const auto material_name = j->str(1);
                    const auto found = std::ranges::find_if(materials, [material_name](auto* c) {
                        return c->en_name.string() == material_name;
                        });
                    if (found != materials.end()) {
                        vec.push_back(*found);
                    }
                }
            };

        const std::regex infobox_regex("<table[^>]+>([\\s\\S]*)</table>");
        std::smatch m;
        if (std::regex_search(response, m, infobox_regex)) {
            std::string infobox_content = m[1].str();

            // Iterate over all skills in this list.
            const auto infobox_row_regex = std::regex(
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
        // Now we've got the wiki info parsed, trigger an item update ui message; this will refresh the item tooltip
        GW::GameThread::Enqueue([info]() {
            const auto item = GW::Items::GetHoveredItem();
            if (item && wcscmp(item->name_enc, info->en_name.encoded().c_str()) == 0) {
                GW::UI::SendUIMessage(GW::UI::UIMessage::kItemUpdated, item);
            }
            });
        (response);
    }

    // Run on worker thread, so we can Sleep!
    void FetchSalvageInfoFromGuildWarsWiki(SalvageInfo* info) {
        info->en_name.string();
        while (info->en_name.IsDecoding()) {
            // @Cleanup: this should never hang, but should we handle it?
            Sleep(16);
        }
        const auto url = GuiUtils::WikiUrl(info->en_name.string().c_str());
        Resources::Download(url, OnWikiContentDownloaded, info);
    }


    SalvageInfo* GetSalvageInfo(const wchar_t* single_item_name) {
        if (!single_item_name)
            return nullptr;
        const auto found = salvage_info_by_model_id.find(single_item_name);
        if (found == salvage_info_by_model_id.end()) {

            // Need to fetch info for this item.
            auto salvage_info = new SalvageInfo();
            salvage_info->en_name.language(GW::Constants::Language::English);
            salvage_info->en_name.reset(single_item_name);
            salvage_info_by_model_id[single_item_name] = salvage_info;


            salvage_info->loading = true;
            Resources::EnqueueWorkerTask([salvage_info]() {
                FetchSalvageInfoFromGuildWarsWiki(salvage_info);
                });

        }
        return salvage_info_by_model_id[single_item_name];
    }

    void AppendSalvageInfoDescription(const uint32_t item_id, wchar_t** description_out) {
        if (!(description_out && *description_out))
            return;
        const auto item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));

        if (!(item && item->name_enc && *item->name_enc && item->IsSalvagable())) {
            return;
        }

        const auto salvage_info = GetSalvageInfo(item->name_enc);
        if (!salvage_info)
            return;

        item_description_appended = *description_out;

        if (salvage_info->loading) {
            item_description_appended += L"\x2\x102\x2\x108\x107" L"Fetching salvage info...\x1";
        }

        if (!salvage_info->common_crafting_materials.empty()) {
            std::wstring items;
            for (auto i : salvage_info->common_crafting_materials) {
                if (!items.empty())
                    items += L"\x2\x108\x107, \x1\x2";
                items += i->en_name.encoded();
            }

            item_description_appended += std::format(L"\x2\x102\x2\x108\x107<c=@ItemCommon>Common Materials:</c> \x1\x2{}", items);
        }
        if (!salvage_info->rare_crafting_materials.empty()) {
            std::wstring items;
            for (auto i : salvage_info->rare_crafting_materials) {
                if (!items.empty())
                    items += L"\x2\x108\x107, \x1";
                items += std::format(L"\x2{}", i->en_name.encoded());
            }

            item_description_appended += std::format(L"\x2\x102\x2\x108\x107<c=@ItemRare>Rare Materials:</c> \x1\x2{}", items);
        }
        *description_out = item_description_appended.data();
    }

    using GetItemDescription_pt = void(__cdecl*)(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out, wchar_t** out2);
    GetItemDescription_pt GetItemDescription_Func = nullptr, GetItemDescription_Ret = nullptr;
    // Block full item descriptions
    void OnGetItemDescription(const uint32_t item_id, const uint32_t flags, const uint32_t quantity, const uint32_t unk, wchar_t** name_out, wchar_t** description_out)
    {
        GW::Hook::EnterHook();
        GetItemDescription_Ret(item_id, flags, quantity, unk, name_out, description_out);
        if (fetch_salvage_info && description_out && *description_out) {
            AppendSalvageInfoDescription(item_id, description_out);
        }

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
    GW::GameThread::Enqueue([]() {
        materials = {
            new CraftingMaterial(GW::Constants::ItemID::Bone, GW::EncStrings::Bone),
            new CraftingMaterial(GW::Constants::ItemID::IronIngot, GW::EncStrings::IronIngot)
            // TODO: fill this in
        };
        });

}

void SalvageInfoModule::Terminate()
{
    ToolboxModule::Terminate();

    if (GetItemDescription_Func) {
        GW::HookBase::DisableHooks(GetItemDescription_Func);
    }

    for (auto m : materials) {
        delete m;
    }
    materials.clear();
}

void SalvageInfoModule::Update(const float)
{
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
