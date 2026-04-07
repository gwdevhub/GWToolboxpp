#include "stdafx.h"

#include <Logger.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>

#include <Modules/InventoryManager.h>
#include <Modules/ItemDescriptionHandler.h>
#include <Modules/ItemTooltipModule.h>
#include <Modules/PriceCheckerModule.h>
#include <Modules/Resources.h>
#include <Windows/DailyQuestsWindow.h>

#include <Constants/EncStrings.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>

#include <Color.h>
#include <Utils/ToolboxUtils.h>

namespace {

    // -------------------------------------------------------------------------
    // Settings
    // -------------------------------------------------------------------------

    // Salvage info
    bool show_salvage_info = true;
    bool show_trader_value_for_mats = false;
    constexpr Color salvage_color_default = GW::Chat::TextColor::ColorItemBasic;
    Color salvage_color = salvage_color_default;

    // Nicholas the Traveler info
    bool show_nicholas_info = true;
    constexpr Color nicholas_color_default = GW::Chat::TextColor::ColorItemUnique;
    Color nicholas_color = nicholas_color_default;

    // Trader price info
    bool show_trader_prices = true;
    constexpr Color price_color_default = GW::Chat::TextColor::ColorItemBasic;
    Color price_color = price_color_default;
    uint32_t high_price_threshold = 1000;

    const auto high_price_color = GW::Chat::TextColor::ColorItemRare;

    bool disable_item_descriptions_in_outpost = false;
    bool disable_item_descriptions_in_explorable = false;

    // Key held to show/hide item descriptions
    constexpr int modifier_key_item_descriptions = VK_MENU;
    int modifier_key_item_descriptions_key_state = 0;



    const wchar_t* material_name_from_slot(GW::Constants::MaterialSlot material_id)
    {
        const auto info = GW::Items::GetMaterialInfo(material_id);
        return info ? info->enc_name : nullptr;
    }

    std::wstring EncodedLiteral(const std::wstring_view str)
    {
        return std::format(L"\x108\x107{}\x1", str);
    }
    const auto EncodedNewLine = L"\x2\x102\x2";
    const auto EncodedNewParagraph = EncodedLiteral(L"<brx>");
    const auto EncodedColorEndTag = EncodedLiteral(L"</c>");

    void NewLineIfNotEmpty(std::wstring& enc_str)
    {
        if (!enc_str.empty()) enc_str += EncodedNewLine;
    }

    std::wstring EncodedColouredString(const std::wstring_view str, const uint32_t col)
    {
        const auto col_str = std::format(L"<c={}>", col);
        return std::format(L"{}\x2{}\x2{}", EncodedLiteral(col_str), str, EncodedColorEndTag);
    }
    std::wstring EncodedColouredString(const std::wstring_view str, const wchar_t* col)
    {
        const auto col_str = std::format(L"<c={}>", col);
        return std::format(L"{}\x2{}\x2{}", EncodedLiteral(col_str), str, EncodedColorEndTag);
    }

    std::wstring EncodedCurrencyString(uint32_t price, bool abbreviated, uint32_t high_threshold, Color high_threshold_color)
    {
        std::wstring currency_string;
        if (abbreviated) {
            std::wstring unencoded;
            if (price > 999) {
                unencoded = TextUtils::FormatFloat((float)(price / 1000.f), 3) + L"k";
            }
            else {
                unencoded = std::format(L"{}g", price);
            }
            currency_string = EncodedLiteral(unencoded);
        }
        else {
            const uint32_t plat = price / 1000;
            const uint32_t gold = price % 1000;
            if (price == 0) {
                currency_string = L"\xAC2\x100";
            }
            else if (plat > 0 && gold > 0) {
                currency_string = std::format(L"\xAC4\x101{}\x102{}", (wchar_t)(0x100 + plat), (wchar_t)(0x100 + gold));
            }
            else if (gold > 0) {
                currency_string = std::format(L"\xAC2\x101{}", (wchar_t)(0x100 + gold));
            }
            else {
                currency_string = std::format(L"\xAC3\x101{}", (wchar_t)(0x100 + plat));
            }
        }

        if (price > high_threshold) {
            currency_string = EncodedColouredString(currency_string, high_threshold_color);
        }
        return currency_string;
    }

    // -------------------------------------------------------------------------
    // Salvage info section
    // -------------------------------------------------------------------------

    void AppendSalvageInfo(const uint32_t item_id, std::wstring& description)
    {
        const auto item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));
        if (!(item && item->name_enc && *item->name_enc && item->IsSalvagable(false))) return;

        const auto formula = GW::Items::GetItemFormula(item);
        if (!(formula && formula->material_cost_count)) return;

        std::vector<std::wstring> common_materials;
        std::vector<std::wstring> rare_materials;
        for (size_t i = 0; i < formula->material_cost_count; i++) {
            const auto material_id = formula->material_cost_buffer[i].material;
            const auto name = material_name_from_slot(material_id);
            if (!name) continue;
            auto* write_to = material_id > GW::Constants::MaterialSlot::Feather ? &rare_materials : &common_materials;
            write_to->push_back(name);
            if (show_trader_value_for_mats) {
                const auto sell_price = PriceCheckerModule::GetTraderSellPrice(material_id);
                if (sell_price) {
                    write_to->back().append(std::format(L"\x2{}\x2{}\x2{}", EncodedLiteral(L" ("), EncodedCurrencyString(sell_price,true,high_price_threshold,high_price_color), EncodedLiteral(L")")));
                }
            }
        }
        if (common_materials.empty() && rare_materials.empty()) return;

        std::wstring items;
        static auto encoded_sep = std::format(L"\x2{}\x2",EncodedLiteral(L", "));
        if (!common_materials.empty()) {
            NewLineIfNotEmpty(items);
            items += std::format(L"{}\x2{}", EncodedLiteral(L"Common Materials: "), TextUtils::Join(common_materials, encoded_sep));
        }
        if (!rare_materials.empty()) {
            NewLineIfNotEmpty(items);
            items += std::format(L"{}\x2{}", EncodedLiteral(L"Rare Materials: "), TextUtils::Join(rare_materials, encoded_sep));
        }

        if (!description.empty()) description += L"\x2";
        description += EncodedNewParagraph + L"\x2" + EncodedColouredString(items, salvage_color);
    }

    // -------------------------------------------------------------------------
    // Nicholas the Traveler section
    // -------------------------------------------------------------------------

    void AppendNicholasInfo(const uint32_t item_id, std::wstring& description)
    {
        const auto item = GW::Items::GetItemById(item_id);
        const auto nicholas_info = DailyQuests::GetNicholasItemInfo(item->name_enc);
        if (!nicholas_info) return;

        const auto collection_time = DailyQuests::GetTimestampFromNicholasTheTraveller(nicholas_info);
        const auto current_time = time(nullptr);

        std::wstring text;
        if (collection_time <= current_time) {
            text = std::format(L"Nicholas The Traveler collects {} of these right now!", nicholas_info->quantity);
        }
        else {
            text = std::format(L"Nicholas The Traveler collects {} of these {}!", nicholas_info->quantity, TextUtils::RelativeTimeW(collection_time));
        }
        if (!description.empty())
            description += L"\x2";
        description += EncodedNewParagraph + L"\x2" + EncodedColouredString(EncodedLiteral(text), nicholas_color);
    }

    // -------------------------------------------------------------------------
    // Trader price section
    // -------------------------------------------------------------------------



    std::wstring PrintPrice(const uint32_t price, const char* name = nullptr)
    {
        const auto subject = EncodedLiteral(name && *name ? TextUtils::StringToWString(name) : L"Trader Value");
        const auto currency = EncodedCurrencyString(price, false, high_price_threshold, high_price_color);
        return std::format(L"{}\x2{}\x2{}", subject, EncodedLiteral(L": "), currency);
    }

    void AppendPriceInfo(const uint32_t item_id, std::wstring& description)
    {
        const auto item = GW::Items::GetItemById(item_id);
        if (!item) return;

        std::string first_item_name;
        std::string second_item_name;
        const auto price_first = PriceCheckerModule::GetPriceByItem(item, &first_item_name, 0);
        const auto price_second = PriceCheckerModule::GetPriceByItem(item, &second_item_name, 1);
        if (!price_first && !price_second) return;

        std::wstring prices_out;
        if (price_first > 0) {
            if (!prices_out.empty()) prices_out += EncodedNewLine;
            prices_out.append(PrintPrice(price_first, first_item_name.empty() ? nullptr : first_item_name.c_str()));
        }
        if (price_second > 0 && first_item_name != second_item_name) {
            if (!prices_out.empty()) prices_out += EncodedNewLine;
            prices_out.append(PrintPrice(price_second, second_item_name.empty() ? nullptr : second_item_name.c_str()));
        }
        if (!prices_out.empty()) {
            if (!description.empty()) description += L"\x2";
            description += EncodedNewParagraph + L"\x2" + EncodedColouredString(prices_out, price_color);
        }
    }

        // Check and re-render item tooltips if modifier key held
    void UpdateItemTooltip()
    {
        if (GetKeyState(modifier_key_item_descriptions) == modifier_key_item_descriptions_key_state) {
            return;
        }
        modifier_key_item_descriptions_key_state = GetKeyState(modifier_key_item_descriptions);
        if (ToolboxUtils::IsExplorable()) {
            if (!disable_item_descriptions_in_explorable) {
                return;
            }
        }
        else if (ToolboxUtils::IsOutpost()) {
            if (!disable_item_descriptions_in_outpost) {
                return;
            }
        }
        else {
            return; // Loading
        }
        const auto tooltip = GW::UI::GetCurrentTooltip();
        if (!tooltip) return;
        // Trigger re-render of item tooltip
        const auto hovered_item = GW::Items::GetHoveredItem();
        if (!hovered_item) {
            return;
        }
        uint32_t items_triggered[2]{};
        const auto inv = GW::Items::GetInventory();
        if (hovered_item == inv->weapon_set0 || hovered_item == inv->offhand_set0) {
            items_triggered[0] = inv->weapon_set0 ? inv->weapon_set0->item_id : 0;
            items_triggered[1] = inv->offhand_set0 ? inv->offhand_set0->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set1 || hovered_item == inv->offhand_set1) {
            items_triggered[0] = inv->weapon_set1 ? inv->weapon_set1->item_id : 0;
            items_triggered[1] = inv->offhand_set1 ? inv->offhand_set1->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set2 || hovered_item == inv->offhand_set2) {
            items_triggered[0] = inv->weapon_set2 ? inv->weapon_set2->item_id : 0;
            items_triggered[1] = inv->offhand_set2 ? inv->offhand_set2->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set3 || hovered_item == inv->offhand_set3) {
            items_triggered[0] = inv->weapon_set3 ? inv->weapon_set3->item_id : 0;
            items_triggered[1] = inv->offhand_set3 ? inv->offhand_set3->item_id : 0;
        }
        else {
            items_triggered[0] = hovered_item->item_id;
            items_triggered[1] = 0;
        }
        GW::GameThread::Enqueue([items = items_triggered] {
            if (items[0]) {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, &items[0]);
            }
            if (items[1]) {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, &items[1]);
            }
        });
    }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    std::wstring tmp_item_description;
    void OnGetItemDescription(uint32_t item_id, uint32_t, uint32_t, uint32_t, wchar_t**, wchar_t** out_desc)
    {
        bool block_description = disable_item_descriptions_in_outpost && ToolboxUtils::IsOutpost() || disable_item_descriptions_in_explorable && ToolboxUtils::IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;

        if (block_description && out_desc) {
            *out_desc = nullptr;
            return;
        }

        if (!out_desc) return;
        if (*out_desc != tmp_item_description.data()) {
            tmp_item_description.assign(*out_desc ? *out_desc : L"");
        }
        if (show_salvage_info) AppendSalvageInfo(item_id, tmp_item_description);
        if (show_trader_prices) AppendPriceInfo(item_id, tmp_item_description);
        if (show_nicholas_info) AppendNicholasInfo(item_id, tmp_item_description);

        if (!tmp_item_description.empty()) {
            if (GW::UI::IsValidEncStr(tmp_item_description.data())) {
                *out_desc = tmp_item_description.data();
            }
        }
    }

    GW::HookEntry UIMessage_HookEntry;
    std::wstring tmp_item_name_tag;
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (message_id != GW::UI::UIMessage::kSetAgentNameTagAttribs) return;

        auto* packet = static_cast<GW::UI::AgentNameTagInfo*>(wParam);
        if (!packet->underline) return;
        if (packet->extra_info_enc != tmp_item_name_tag.data()) {
            tmp_item_name_tag.assign(packet->extra_info_enc ? packet->extra_info_enc : L"");
        }
        const auto agent = static_cast<GW::AgentItem*>(GW::Agents::GetAgentByID(packet->agent_id));
        if (!(agent && agent->GetIsItemType())) return;
        if (agent->owner && agent->owner != GW::Agents::GetControlledCharacterId()) return;

        const auto item_id = agent->item_id;
        if (show_salvage_info) AppendSalvageInfo(item_id, tmp_item_name_tag);
        if (show_trader_prices) AppendPriceInfo(item_id, tmp_item_name_tag);
        if (show_nicholas_info) AppendNicholasInfo(item_id, tmp_item_name_tag);

        packet->extra_info_enc = tmp_item_name_tag.data();
    }

} // namespace

// =============================================================================
// ItemTooltipModule
// =============================================================================

void ItemTooltipModule::Initialize()
{
    ToolboxModule::Initialize();
    // Priority 200 keeps us after PriceCheckerModule (100), ensuring prices
    // are already fetched before we try to display them.
    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 200);
    GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry, GW::UI::UIMessage::kSetAgentNameTagAttribs, ::OnUIMessage);
}

void ItemTooltipModule::Terminate()
{
    ToolboxModule::Terminate();
    ItemDescriptionHandler::UnregisterDescriptionCallback(OnGetItemDescription);
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
}

void ItemTooltipModule::Update(float) {
    UpdateItemTooltip();
}

void ItemTooltipModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(show_salvage_info);
    SAVE_BOOL(show_trader_value_for_mats);
    SAVE_COLOR(salvage_color);
    SAVE_BOOL(show_nicholas_info);
    SAVE_COLOR(nicholas_color);
    SAVE_BOOL(show_trader_prices);
    SAVE_COLOR(price_color);
    SAVE_FLOAT(high_price_threshold);
    SAVE_BOOL(disable_item_descriptions_in_outpost);
    SAVE_BOOL(disable_item_descriptions_in_explorable);
}

void ItemTooltipModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    // Load current settings (new keys under [ItemTooltipModule]).
    LOAD_BOOL(show_salvage_info);
    LOAD_BOOL(show_trader_value_for_mats);
    LOAD_COLOR(salvage_color);
    LOAD_BOOL(show_nicholas_info);
    LOAD_COLOR(nicholas_color);
    LOAD_BOOL(show_trader_prices);
    LOAD_COLOR(price_color);
    LOAD_UINT(high_price_threshold);
    LOAD_BOOL(disable_item_descriptions_in_outpost);
    LOAD_BOOL(disable_item_descriptions_in_explorable);

    // One-time migration from the old split modules.
    // Each key is only migrated if it hasn't been written to the new section yet
    // (i.e. the new key is still at its compile-time default value), so we don't
    // clobber settings that the user has already adjusted in the new module.
    const char* salvage_section = "SalvageInfoModule";
    const char* price_section = "PriceCheckerModule";

    // SalvageInfoModule: add_salvage_info_to_description -> show_salvage_info
    if (!ini->KeyExists(Name(), "show_salvage_info") && ini->KeyExists(salvage_section, "add_salvage_info_to_description")) {
        show_salvage_info = ini->GetBoolValue(salvage_section, "add_salvage_info_to_description", show_salvage_info);
    }
    // SalvageInfoModule: show_trader_value_for_salvage_items -> show_trader_value_for_mats
    if (!ini->KeyExists(Name(), "show_trader_value_for_mats") && ini->KeyExists(salvage_section, "show_trader_value_for_salvage_items")) {
        show_trader_value_for_mats = ini->GetBoolValue(salvage_section, "show_trader_value_for_salvage_items", show_trader_value_for_mats);
    }
    // SalvageInfoModule: add_nicholas_info_to_description -> show_nicholas_info
    if (!ini->KeyExists(Name(), "show_nicholas_info") && ini->KeyExists(salvage_section, "add_nicholas_info_to_description")) {
        show_nicholas_info = ini->GetBoolValue(salvage_section, "add_nicholas_info_to_description", show_nicholas_info);
    }

    // PriceCheckerModule: show_prices_in_item_description -> show_trader_prices
    if (!ini->KeyExists(Name(), "show_trader_prices") && ini->KeyExists(price_section, "show_prices_in_item_description")) {
        show_trader_prices = ini->GetBoolValue(price_section, "show_prices_in_item_description", show_trader_prices);
    }
    // PriceCheckerModule: text_color -> price_color
    if (!ini->KeyExists(Name(), "price_color") && ini->KeyExists(price_section, "text_color")) {
        price_color = Colors::Load(ini, price_section, "text_color", price_color);
    }
    // PriceCheckerModule: high_price_threshold -> high_price_threshold (same name, different section)
    if (!ini->KeyExists(Name(), "high_price_threshold") && ini->KeyExists(price_section, "high_price_threshold")) {
        high_price_threshold = static_cast<uint32_t>(ini->GetLongValue(price_section, "high_price_threshold", high_price_threshold));
    }

    const char* show_descriptions_section = "Game Settings";
    // Hide/show item descriptions
    if (!ini->KeyExists(Name(), "disable_item_descriptions_in_outpost") && ini->KeyExists(show_descriptions_section, "disable_item_descriptions_in_outpost")) {
        disable_item_descriptions_in_outpost = ini->GetBoolValue(show_descriptions_section, "disable_item_descriptions_in_outpost", disable_item_descriptions_in_outpost);
    }
    if (!ini->KeyExists(Name(), "disable_item_descriptions_in_outpost") && ini->KeyExists(show_descriptions_section, "disable_item_descriptions_in_explorable")) {
        disable_item_descriptions_in_explorable = ini->GetBoolValue(show_descriptions_section, "disable_item_descriptions_in_explorable", disable_item_descriptions_in_explorable);
    }
    
}

void ItemTooltipModule::DrawSettingsInternal()
{
    ImGui::NewLine();
    ImGui::TextDisabled("Control what information appears in item tooltips");
    ImGui::Separator();
    ImGui::Text("Hide item descriptions in:");
    ImGui::ShowHelp("When hovering an item in inventory or weapon sets,\nonly show the item name in the tooltip that appears.");
    ImGui::Indent();
    ImGui::Checkbox("Explorable Area###disable_item_descriptions_in_explorable", &disable_item_descriptions_in_explorable);
    ImGui::Unindent();
    ImGui::SameLine();
    ImGui::Checkbox("Outpost###disable_item_descriptions_in_outpost", &disable_item_descriptions_in_outpost);
    if (disable_item_descriptions_in_explorable || disable_item_descriptions_in_outpost) {
        ImGui::Indent();
        ImGui::TextDisabled("Hold Alt when hovering an item to show full description");
        ImGui::Unindent();
    }

    // --- Salvage info --------------------------------------------------------
    ImGui::Checkbox("Show salvage materials in item tooltip", &show_salvage_info);
    ImGui::ShowHelp("When hovering over a salvageable item, display which common and rare materials can be salvaged from it");
    if (show_salvage_info) {
        ImGui::Indent();

        ImGui::Checkbox("Show trader value next to salvage materials", &show_trader_value_for_mats);

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Salvage info text color", &salvage_color);
        ImGui::SameLine();
        bool reset_salvage = false;
        if (ImGui::ConfirmButton("Reset##salvage", &reset_salvage)) salvage_color = salvage_color_default;

        ImGui::Unindent();
    }

    // --- Nicholas info -------------------------------------------------------
    ImGui::Checkbox("Show Nicholas the Traveler info in item tooltip", &show_nicholas_info);
    ImGui::ShowHelp("When hovering over an item that Nicholas collects, display when he will collect it and how many he wants");
    if (show_nicholas_info) {
        ImGui::Indent();

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Nicholas info text color", &nicholas_color);
        ImGui::SameLine();
        bool reset_nicholas = false;
        if (ImGui::ConfirmButton("Reset##nicholas", &reset_nicholas)) nicholas_color = nicholas_color_default;

        ImGui::Unindent();
    }

    // --- Trader prices -------------------------------------------------------
    ImGui::Checkbox("Show trader prices in item tooltip", &show_trader_prices);
    ImGui::ShowHelp("Current rune, dye and mod prices are fetched from https://kamadan.gwtoolbox.com");
    if (show_trader_prices) {
        ImGui::Indent();

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Trader price text color", &price_color);
        ImGui::SameLine();
        bool reset_price = false;
        if (ImGui::ConfirmButton("Reset##price", &reset_price)) price_color = price_color_default;



        ImGui::Unindent();
    }
    ImGui::SliderInt("High price threshold", (int*)&high_price_threshold, 100, 50000);
    ImGui::ShowHelp("Prices above this threshold will be highlighted in gold");
    ImGui::NewLine();
}

void ItemTooltipModule::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent(
        "Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) DrawSettingsInternal();
        },
        1.1f
    );
}
