#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Windows/DropTrackerWindow.h>
#include <Modules/ItemDrops.h>
#include <map>
#include <Modules/Resources.h>

#include "Utils/TextUtils.h"


namespace {
    enum class GroupMode { None, ItemName, Map, Rarity, Type, Weapon };

    GroupMode current_group_mode = GroupMode::None;
    const char* group_mode_names[] = {"None", "Item Name", "Map", "Rarity", "Type", "Weapon"};
    float icon_size = 48;
    float run_count = 0;
    
    bool IsWeapon(const ItemDrops::PendingDrop* drop)
    {
        switch (drop->type) {
            case GW::Constants::ItemType::Axe:
            case GW::Constants::ItemType::Sword:
            case GW::Constants::ItemType::Shield:
            case GW::Constants::ItemType::Scythe:
            case GW::Constants::ItemType::Bow:
            case GW::Constants::ItemType::Wand:
            case GW::Constants::ItemType::Staff:
            case GW::Constants::ItemType::Offhand:
            case GW::Constants::ItemType::Daggers:
            case GW::Constants::ItemType::Hammer:
            case GW::Constants::ItemType::Spear:
                return true;
            default:
                return false;
        }
    }

    std::wstring GetWeaponCategory(const ItemDrops::PendingDrop* drop)
    {
        if (!IsWeapon(drop)) {
            return L"Non-Weapon";
        }

        std::wstring category = GW::Items::GetItemTypeName(drop->type);
        if (drop->damage_type != GW::Constants::DamageType::None) {
            category += std::format(L" ({})",GW::Items::GetDamageTypeName(drop->damage_type));
        }
        return category;
    }

    void DrawItemIcon(const ItemDrops::PendingDrop* drop)
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(*drop->icon), ImVec2(icon_size, icon_size));
    }

    void DrawDefaultTable(std::vector<ItemDrops::PendingDrop*>& drops)
    {
        if (ImGui::BeginTable("drops_table", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Rarity", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Value(Gold)", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto drop : drops) {
                std::tm tm_buf{};
                localtime_s(&tm_buf, &drop->system_time);
                char time_str[32];
                std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                DrawItemIcon(drop);

                ImGui::TableNextColumn();
                ImGui::Text("%s", time_str);
                ImGui::TableNextColumn();

                ImGui::TextColored(GW::Items::GetRarityColor(drop->rarity), "%s", drop->GetItemName()->string().c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%ls", GW::Items::GetItemTypeName(drop->type));
                ImGui::TableNextColumn();
                ImGui::Text("%ls", GW::Items::GetRarityName(drop->rarity));
                ImGui::TableNextColumn();
                ImGui::Text("%d", drop->quantity);
                ImGui::TableNextColumn();
                ImGui::Text("%d", drop->value);
                ImGui::TableNextColumn();
                ImGui::Text("%s", Resources::GetMapName(drop->map_id)->string().c_str());
            }
            ImGui::EndTable();
        }
    }

    void DrawDefaultGroupTable(const std::map<std::wstring, std::vector<ItemDrops::PendingDrop*>>& grouped)
    {
        if (ImGui::BeginTable("grouped_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(group_mode_names[static_cast<int>(current_group_mode)], ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Total Qty", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            int group_idx = 0; // Use a stable index for each group
            for (const auto& [key, items] : grouped) {
                uint32_t total_qty = 0;
                for (const auto* item : items) {
                    total_qty += item->quantity;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Use the index as the ID, not the string content
                ImGui::PushID(group_idx++);

                // Use TreeNodeEx with a simple label
                bool open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_SpanAvailWidth, "%ls", key.empty() ? L"(Unknown)" : key.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%zu", items.size());
                ImGui::TableNextColumn();
                ImGui::Text("%d", total_qty);

                if (open) {
                    int item_idx = 0;
                    for (auto drop : items) {
                        ImGui::PushID(item_idx++); // Unique ID for each sub-item

                        std::tm tm_buf{};
                        localtime_s(&tm_buf, &drop->system_time);
                        char time_str[32];
                        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                        if (current_group_mode == GroupMode::ItemName) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            DrawItemIcon(drop);
                        }
                        else {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                        }
                        ImGui::Text("%s", time_str);
                        ImGui::TextColored(GW::Items::GetRarityColor(drop->rarity), "%s", drop->GetItemName()->string().c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", drop->quantity);
                        ImGui::TableNextColumn();

                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void DrawWeaponsTable(const std::map<std::wstring, std::vector<ItemDrops::PendingDrop*>>& grouped)
    {
        if (ImGui::BeginTable("grouped_table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Weapon Type", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Damage Range", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Requirement", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Total Qty", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Avg Value", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            int group_idx = 0;
            for (const auto& [key, items] : grouped) {
                uint32_t total_qty = 0;
                uint32_t total_value = 0;
                uint16_t min_damage_overall = UINT16_MAX;
                uint16_t max_damage_overall = 0;
                std::set<std::wstring> requirements;

                for (const auto* item : items) {
                    total_qty += item->quantity;
                    total_value += item->value;
                    if (item->min_damage > 0 && item->min_damage < min_damage_overall) {
                        min_damage_overall = item->min_damage;
                    }
                    if (item->max_damage > max_damage_overall) {
                        max_damage_overall = item->max_damage;
                    }
                    if (item->requirement_attribute != GW::Constants::AttributeByte::None) {
                        requirements.insert(std::to_wstring(item->requirement_value) + L" " + GW::Items::GetAttributeName(item->requirement_attribute));
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushID(group_idx++);
                bool open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_SpanAvailWidth, "%ls", key.empty() ? L"(Unknown)" : key.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%zu", items.size());

                ImGui::TableNextColumn();
                if (min_damage_overall != UINT16_MAX && max_damage_overall > 0) {
                    ImGui::Text("%d-%d", min_damage_overall, max_damage_overall);
                }
                else {
                    ImGui::Text("-");
                }

                ImGui::TableNextColumn();
                if (!requirements.empty()) {
                    std::wstring req_text;
                    for (const auto& req : requirements) {
                        if (!req_text.empty()) req_text += L", ";
                        req_text += req;
                    }
                    ImGui::Text("%ls", req_text.c_str());
                }
                else {
                    ImGui::Text("-");
                }

                ImGui::TableNextColumn();
                ImGui::Text("%d", total_qty);

                ImGui::TableNextColumn();
                ImGui::Text("%d", items.empty() ? 0 : total_value / static_cast<uint32_t>(items.size()));

                if (open) {
                    int item_idx = 0;
                    for (auto drop : items) {
                        ImGui::PushID(item_idx++);

                        std::tm tm_buf{};
                        localtime_s(&tm_buf, &drop->system_time);
                        char time_str[32];
                        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        // Show item details with weapon stats
                        ImGui::TextColored(GW::Items::GetRarityColor(drop->rarity), "%s", drop->GetItemName()->string().c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", drop->quantity);
                        ImGui::TableNextColumn();
                        if (drop->min_damage > 0 && drop->max_damage > 0) {
                            ImGui::Text("%d-%d", drop->min_damage, drop->max_damage);
                        }

                        ImGui::TableNextColumn();
                        if (drop->requirement_attribute != GW::Constants::AttributeByte::None) {
                            ImGui::Text("%d %ls", drop->requirement_value, GW::Items::GetAttributeName(drop->requirement_attribute));
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", drop->quantity);

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", drop->value);

                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

}

void DropTrackerWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        auto& drops = ItemDrops::Instance().GetDropHistory();

        if (!ItemDrops::Instance().IsTrackingEnabled()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Drop tracking is disabled. Enable it in Item Settings to use this.");
            ImGui::Separator();
        }

        if (ImGui::Button("Clear")) {
            ItemDrops::Instance().ClearDropHistory();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy to Clipboard")) {
            std::wstring csv = ItemDrops::PendingDrop::GetCSVHeader();
            csv += L"\n";
            for (const auto& drop : drops) {
                csv += drop->toCSV();
                csv += L"\n";
            }
            ImGui::SetClipboardText(TextUtils::WStringToString(csv).c_str());
            Log::Info("Copied %zu drops to clipboard", drops.size());
        }
        ImGui::SameLine();
        ImGui::Text("Total drops: %zu", drops.size());
        ImGui::SameLine();
        ImGui::Text("Group By:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("##GroupByCombo", reinterpret_cast<int*>(&current_group_mode), group_mode_names, IM_ARRAYSIZE(group_mode_names));
        ImGui::SameLine();
        ImGui::Text("Total Gold Value:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%d", ItemDrops::Instance().GetTotalGoldValue());

        ImGui::Separator();

        if (current_group_mode == GroupMode::None) {
            DrawDefaultTable(drops);
        }
        else {
            std::map<std::wstring, std::vector<ItemDrops::PendingDrop*>> grouped;

            for (auto drop : drops) {
                std::wstring key;
                switch (current_group_mode) {
                    case GroupMode::ItemName:
                        key = drop->GetItemName()->wstring();
                        break;
                    case GroupMode::Map:
                        key = Resources::GetMapName(drop->map_id)->wstring();
                        break;
                    case GroupMode::Rarity:
                        key = GW::Items::GetRarityName(drop->rarity);
                        break;
                    case GroupMode::Type:
                        key = GW::Items::GetItemTypeName(drop->type);
                        break;
                    case GroupMode::Weapon:
                        key = GetWeaponCategory(drop);
                        break;
                    default:
                        break;
                }
                grouped[key].push_back(drop);
            }

            if (current_group_mode == GroupMode::Weapon) {
                DrawWeaponsTable(grouped);
            }
            else {
                DrawDefaultGroupTable(grouped);
            }
        }
    }
    ImGui::End();
}

void DropTrackerWindow::DrawSettingsInternal() {
    ImGui::DragFloat("Item Icon Size", &icon_size, 16, 16, 64);
}

void DropTrackerWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_FLOAT(icon_size);
    LOAD_FLOAT(run_count);
}

void DropTrackerWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_FLOAT(icon_size);
    SAVE_FLOAT(run_count);
}
