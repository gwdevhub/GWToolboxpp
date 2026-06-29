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
#include <Windows/DailyQuestsWindow.h>

#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>

#include <Color.h>
#include <Utils/ToolboxUtils.h>

namespace {

    // -------------------------------------------------------------------------
    // Settings
    // -------------------------------------------------------------------------

    constexpr Color salvage_color_default = GW::Chat::TextColor::ColorItemBasic;
    constexpr Color nicholas_color_default = GW::Chat::TextColor::ColorItemUnique;
    constexpr Color price_color_default = GW::Chat::TextColor::ColorItemBasic;

    const auto high_price_color = GW::Chat::TextColor::ColorItemRare;

    // Key held to show/hide item descriptions
    constexpr int modifier_key_item_descriptions = VK_MENU;
    int modifier_key_item_descriptions_key_state = 0;
}

// MSVC can't reflect member names of internal-linkage types, so this lives in a named namespace
namespace ItemTooltipSettings {
    struct Settings {
        bool show_salvage_info = true;
        bool show_trader_value_for_mats = false;
        Colors::SettingColor salvage_color = salvage_color_default;
        bool show_nicholas_info = true;
        Colors::SettingColor nicholas_color = nicholas_color_default;
        bool show_trader_prices = true;
        Colors::SettingColor price_color = price_color_default;
        uint32_t high_price_threshold = 1000;
        bool disable_item_descriptions_in_outpost = false;
        bool disable_item_descriptions_in_explorable = false;
    };
}

namespace {
    ItemTooltipSettings::Settings settings;



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
            if (settings.show_trader_value_for_mats) {
                const auto sell_price = PriceCheckerModule::GetTraderSellPrice(material_id);
                if (sell_price) {
                    write_to->back().append(std::format(L"\x2{}\x2{}\x2{}", EncodedLiteral(L" ("), EncodedCurrencyString(sell_price,true,settings.high_price_threshold,high_price_color), EncodedLiteral(L")")));
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
        description += EncodedNewParagraph + L"\x2" + EncodedColouredString(items, settings.salvage_color);
    }

    // -------------------------------------------------------------------------
    // Nicholas the Traveler section
    // -------------------------------------------------------------------------
    void AppendNicholasInfo(const uint32_t item_id, std::wstring& description)
    {
        const auto item = GW::Items::GetItemById(item_id);
        if (!(item && item->name_enc)) return;

        static std::wstring last_item_name;
        static std::wstring last_nicholas_text;

        auto append = [&](std::wstring text) {
            if (!description.empty()) description += L"\x2";
            description += EncodedNewParagraph + L"\x2" + EncodedColouredString(EncodedLiteral(text), settings.nicholas_color);
            last_nicholas_text = text;
        };

        if (item->name_enc == last_item_name) {
            append(last_nicholas_text);
            return;
        }
        last_nicholas_text.clear();
        last_item_name = item->name_enc;

        const auto current_time = time(nullptr);

        if (GW::Map::IsPreSearing()) {
            const auto info = DailyQuests::GetNicholasSandfordItemInfo(item->name_enc);
            if (!info) return;
            const auto collection_time = DailyQuests::GetTimestampFromNicholasSandford(info);
            if (collection_time <= current_time)
                append(std::format(L"Nicholas Sandford collects {} of these right now!", 5));
            else
                append(std::format(L"Nicholas Sandford collects {} of these {}!", 5, TextUtils::RelativeTimeW(collection_time)));
            return;
        }

        const auto info = DailyQuests::GetNicholasItemInfo(item->name_enc);
        const auto ingredient = DailyQuests::GetNicholasIngredientInfo(item->name_enc);
        if (!info && !ingredient) return;

        const auto ingredient_nick_info = ingredient ? DailyQuests::GetNicholasItemInfo(ingredient->nicholas_item) : nullptr;

        const auto info_time = info ? DailyQuests::GetTimestampFromNicholasTheTraveller(info) : std::numeric_limits<time_t>::max();
        const auto ingredient_time = ingredient_nick_info ? DailyQuests::GetTimestampFromNicholasTheTraveller(ingredient_nick_info) : std::numeric_limits<time_t>::max();

        if (info && info_time <= ingredient_time) {
            const auto quantity_for_total_gifts = info->quantity * 5;
            if (info_time <= current_time)
                append(std::format(L"Nicholas The Traveler collects {} of these right now!", quantity_for_total_gifts));
            else
                append(std::format(L"Nicholas The Traveler collects {} of these {}!", quantity_for_total_gifts, TextUtils::RelativeTimeW(info_time)));
        }
        else if (ingredient_nick_info) {
            const auto quantity_for_total_gifts = ingredient_nick_info->quantity * 5;
            const auto total_qty = ingredient->ingredient_quantity * quantity_for_total_gifts;
            if (ingredient_time <= current_time)
                append(std::format(L"Collect {} of these to craft {} Nicholas The Traveler items right now!", total_qty, quantity_for_total_gifts));
            else
                append(std::format(L"Collect {} of these to craft {} Nicholas The Traveler items {}!", total_qty, quantity_for_total_gifts, TextUtils::RelativeTimeW(ingredient_time)));
        }
    }

    // -------------------------------------------------------------------------
    // Trader price section
    // -------------------------------------------------------------------------



    std::wstring PrintPrice(const uint32_t price, const char* name = nullptr)
    {
        const auto subject = EncodedLiteral(name && *name ? TextUtils::StringToWString(name) : L"Trader Value");
        const auto currency = EncodedCurrencyString(price, false, settings.high_price_threshold, high_price_color);
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
            description += EncodedNewParagraph + L"\x2" + EncodedColouredString(prices_out, settings.price_color);
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
            if (!settings.disable_item_descriptions_in_explorable) {
                return;
            }
        }
        else if (ToolboxUtils::IsOutpost()) {
            if (!settings.disable_item_descriptions_in_outpost) {
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
        bool block_description = settings.disable_item_descriptions_in_outpost && ToolboxUtils::IsOutpost() || settings.disable_item_descriptions_in_explorable && ToolboxUtils::IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;

        if (block_description && out_desc) {
            *out_desc = nullptr;
            return;
        }

        if (!out_desc) return;
        if (*out_desc != tmp_item_description.data()) {
            tmp_item_description.assign(*out_desc ? *out_desc : L"");
        }
        if (settings.show_salvage_info) AppendSalvageInfo(item_id, tmp_item_description);
        if (settings.show_trader_prices) AppendPriceInfo(item_id, tmp_item_description);
        if (settings.show_nicholas_info) AppendNicholasInfo(item_id, tmp_item_description);

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
        if (settings.show_salvage_info) AppendSalvageInfo(item_id, tmp_item_name_tag);
        if (settings.show_trader_prices) AppendPriceInfo(item_id, tmp_item_name_tag);
        if (settings.show_nicholas_info) AppendNicholasInfo(item_id, tmp_item_name_tag);

        packet->extra_info_enc = tmp_item_name_tag.data();
    }

} // namespace

// =============================================================================
// ItemTooltipModule
// =============================================================================

void ItemTooltipModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
    // Priority 200 keeps us after PriceCheckerModule (100), ensuring prices
    // are already fetched before we try to display them.
    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 200);
    RegisterUIMessageCallback(&UIMessage_HookEntry, GW::UI::UIMessage::kSetAgentNameTagAttribs, ::OnUIMessage);
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

void ItemTooltipModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    if (!legacy) {
        return;
    }

    // One-time migration from the old split modules; only when the user never saved the new key.
    const char* salvage_section = "SalvageInfoModule";
    const char* price_section = "PriceCheckerModule";
    const auto new_key_unset = [&](const char* key) {
        return !doc.Has(Name(), key) && !legacy->KeyExists(Name(), key);
    };

    if (new_key_unset("show_salvage_info") && legacy->KeyExists(salvage_section, "add_salvage_info_to_description")) {
        settings.show_salvage_info = legacy->GetBoolValue(salvage_section, "add_salvage_info_to_description", settings.show_salvage_info);
    }
    if (new_key_unset("show_trader_value_for_mats") && legacy->KeyExists(salvage_section, "show_trader_value_for_salvage_items")) {
        settings.show_trader_value_for_mats = legacy->GetBoolValue(salvage_section, "show_trader_value_for_salvage_items", settings.show_trader_value_for_mats);
    }
    if (new_key_unset("show_nicholas_info") && legacy->KeyExists(salvage_section, "add_nicholas_info_to_description")) {
        settings.show_nicholas_info = legacy->GetBoolValue(salvage_section, "add_nicholas_info_to_description", settings.show_nicholas_info);
    }

    if (new_key_unset("show_trader_prices") && legacy->KeyExists(price_section, "show_prices_in_item_description")) {
        settings.show_trader_prices = legacy->GetBoolValue(price_section, "show_prices_in_item_description", settings.show_trader_prices);
    }
    if (new_key_unset("price_color") && legacy->KeyExists(price_section, "text_color")) {
        settings.price_color = Colors::Load(legacy, price_section, "text_color", settings.price_color);
    }
    if (new_key_unset("high_price_threshold") && legacy->KeyExists(price_section, "high_price_threshold")) {
        settings.high_price_threshold = static_cast<uint32_t>(legacy->GetLongValue(price_section, "high_price_threshold", settings.high_price_threshold));
    }

    const char* show_descriptions_section = "Game Settings";
    if (new_key_unset("disable_item_descriptions_in_outpost") && legacy->KeyExists(show_descriptions_section, "disable_item_descriptions_in_outpost")) {
        settings.disable_item_descriptions_in_outpost = legacy->GetBoolValue(show_descriptions_section, "disable_item_descriptions_in_outpost", settings.disable_item_descriptions_in_outpost);
    }
    if (new_key_unset("disable_item_descriptions_in_outpost") && legacy->KeyExists(show_descriptions_section, "disable_item_descriptions_in_explorable")) {
        settings.disable_item_descriptions_in_explorable = legacy->GetBoolValue(show_descriptions_section, "disable_item_descriptions_in_explorable", settings.disable_item_descriptions_in_explorable);
    }
}

void ItemTooltipModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void ItemTooltipModule::DrawSettingsInternal()
{
    ImGui::NewLine();
    ImGui::TextDisabled("Control what information appears in item tooltips");
    ImGui::Separator();
    ImGui::Text("Hide item descriptions in:");
    ImGui::ShowHelp("When hovering an item in inventory or weapon sets,\nonly show the item name in the tooltip that appears.");
    ImGui::Indent();
    ImGui::Checkbox("Explorable Area###disable_item_descriptions_in_explorable", &settings.disable_item_descriptions_in_explorable);
    ImGui::Unindent();
    ImGui::SameLine();
    ImGui::Checkbox("Outpost###disable_item_descriptions_in_outpost", &settings.disable_item_descriptions_in_outpost);
    if (settings.disable_item_descriptions_in_explorable || settings.disable_item_descriptions_in_outpost) {
        ImGui::Indent();
        ImGui::TextDisabled("Hold Alt when hovering an item to show full description");
        ImGui::Unindent();
    }

    // --- Salvage info --------------------------------------------------------
    ImGui::CheckboxWithHelp("Show salvage materials in item tooltip", &settings.show_salvage_info, "When hovering over a salvageable item, display which common and rare materials can be salvaged from it");
    if (settings.show_salvage_info) {
        ImGui::Indent();

        ImGui::Checkbox("Show trader value next to salvage materials", &settings.show_trader_value_for_mats);

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Salvage info text color", &settings.salvage_color.value);
        ImGui::SameLine();
        bool reset_salvage = false;
        if (ImGui::ConfirmButton("Reset##salvage", &reset_salvage)) settings.salvage_color = salvage_color_default;

        ImGui::Unindent();
    }

    // --- Nicholas info -------------------------------------------------------
    ImGui::CheckboxWithHelp("Show Nicholas the Traveler info in item tooltip", &settings.show_nicholas_info, "When hovering over an item that Nicholas collects, display when he will collect it and how many he wants");
    if (settings.show_nicholas_info) {
        ImGui::Indent();

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Nicholas info text color", &settings.nicholas_color.value);
        ImGui::SameLine();
        bool reset_nicholas = false;
        if (ImGui::ConfirmButton("Reset##nicholas", &reset_nicholas)) settings.nicholas_color = nicholas_color_default;

        ImGui::Unindent();
    }

    // --- Trader prices -------------------------------------------------------
    ImGui::CheckboxWithHelp("Show trader prices in item tooltip", &settings.show_trader_prices, "Current rune, dye and mod prices are fetched from https://kamadan.gwtoolbox.com");
    if (settings.show_trader_prices) {
        ImGui::Indent();

        ImGui::TextUnformatted("Text color:");
        ImGui::SameLine();
        ImGui::ColorButtonPicker("Trader price text color", &settings.price_color.value);
        ImGui::SameLine();
        bool reset_price = false;
        if (ImGui::ConfirmButton("Reset##price", &reset_price)) settings.price_color = price_color_default;



        ImGui::Unindent();
    }
    ImGui::SliderInt("High price threshold", (int*)&settings.high_price_threshold, 100, 50000);
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
