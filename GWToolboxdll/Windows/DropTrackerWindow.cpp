#include <Color.h>
#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Windows/DropTrackerWindow.h>
#include <Modules/ItemDrops.h>
#include <map>
#include <Modules/Resources.h>


namespace {
    enum class GroupMode { None, ItemName, Map, Rarity, Type, Weapon };

    GroupMode current_group_mode = GroupMode::None;
    const char* group_mode_names[] = {"None", "Item Name", "Map", "Rarity", "Type", "Weapon"};
    float icon_size = 48;
    float run_count = 0;
}

void DropTrackerWindow::Terminate()
{
    ToolboxWindow::Terminate();
}

bool IsWeapon(const ItemDrops::PendingDrop& drop)
{
    return drop.type == L"Axe" || drop.type == L"Sword" || drop.type == L"Hammer" || drop.type == L"Bow" || drop.type == L"Staff" || drop.type == L"Wand" || drop.type == L"Daggers" || drop.type == L"Scythe" || drop.type == L"Spear";
}

std::wstring GetWeaponCategory(const ItemDrops::PendingDrop& drop)
{
    if (!IsWeapon(drop)) {
        return L"Non-Weapon";
    }

    std::wstring category = drop.type;
    if (!drop.damage_type.empty()) {
        category += L" (" + drop.damage_type + L")";
    }
    return category;
}

void DrawItemIcon(const ItemDrops::PendingDrop& drop)
{
    if (!drop.item_name.empty()) {
        IDirect3DTexture9** texture_ptr = Resources::GetItemImage(drop.item_name);
        if (texture_ptr && *texture_ptr) {
            ImGui::Image(reinterpret_cast<ImTextureID>(*texture_ptr), ImVec2(icon_size, icon_size));
        }
        else {
            ImGui::Dummy(ImVec2(icon_size, icon_size));
        }
    }
    else {
        ImGui::Dummy(ImVec2(icon_size, icon_size));
    }
}

void DrawDefaultTable(std::vector<ItemDrops::PendingDrop> drops)
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

        for (auto it = drops.rbegin(); it != drops.rend(); ++it) {
            const auto& drop = *it;
            const auto time = std::chrono::system_clock::to_time_t(drop.timestamp);
            std::tm tm_buf{};
            localtime_s(&tm_buf, &time);
            char time_str[32];
            std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            DrawItemIcon(drop);

            ImGui::TableNextColumn();
            ImGui::Text("%s", time_str);
            ImGui::TableNextColumn();
            ImGui::Text("%ls", drop.item_name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%ls", drop.type.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%ls", drop.rarity.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", drop.quantity);
            ImGui::TableNextColumn();
            ImGui::Text("%d", drop.value);
            ImGui::TableNextColumn();
            ImGui::Text("%ls", drop.map_name.c_str());
        }
        ImGui::EndTable();
    }
}

void DrawDefaultGroupTable(const std::map<std::wstring, std::vector<const ItemDrops::PendingDrop*>>& grouped)
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
                for (const auto* drop : items) {
                    ImGui::PushID(item_idx++); // Unique ID for each sub-item

                    const auto time = std::chrono::system_clock::to_time_t(drop->timestamp);
                    std::tm tm_buf{};
                    localtime_s(&tm_buf, &time);
                    char time_str[32];
                    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                    if (current_group_mode == GroupMode::ItemName) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        DrawItemIcon(*drop);
                    }
                    else {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                    }

                    ImGui::Text("  %s - %ls x%d", time_str, drop->item_name.c_str(), drop->quantity);
                    ImGui::TableNextColumn();
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

void DrawWeaponsTable(const std::map<std::wstring, std::vector<const ItemDrops::PendingDrop*>>& grouped)
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
                if (!item->requirement_attribute.empty()) {
                    requirements.insert(std::to_wstring(item->requirement_value) + L" " + item->requirement_attribute);
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
                for (const auto* drop : items) {
                    ImGui::PushID(item_idx++);

                    const auto time = std::chrono::system_clock::to_time_t(drop->timestamp);
                    std::tm tm_buf{};
                    localtime_s(&tm_buf, &time);
                    char time_str[32];
                    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Show item details with weapon stats
                    ImGui::Text("  %s - %ls", time_str, drop->item_name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    if (drop->min_damage > 0 && drop->max_damage > 0) {
                        ImGui::Text("%d-%d", drop->min_damage, drop->max_damage);
                    }

                    ImGui::TableNextColumn();
                    if (!drop->requirement_attribute.empty()) {
                        ImGui::Text("%d %ls", drop->requirement_value, drop->requirement_attribute.c_str());
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
        ImGui::Text("Total drops: %zu", drops.size());
        ImGui::SameLine();
        ImGui::Text("Group By:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("##GroupByCombo", reinterpret_cast<int*>(&current_group_mode), group_mode_names, IM_ARRAYSIZE(group_mode_names));

        ImGui::Separator();

        if (current_group_mode == GroupMode::None) {
            DrawDefaultTable(drops);
        }
        else {
            std::map<std::wstring, std::vector<const ItemDrops::PendingDrop*>> grouped;

            for (const auto& drop : drops) {
                std::wstring key;
                switch (current_group_mode) {
                    case GroupMode::ItemName:
                        key = drop.item_name;
                        break;
                    case GroupMode::Map:
                        key = drop.map_name;
                        break;
                    case GroupMode::Rarity:
                        key = drop.rarity;
                        break;
                    case GroupMode::Type:
                        key = drop.type;
                        break;
                    case GroupMode::Weapon: // ADD THIS CASE
                        key = GetWeaponCategory(drop);
                        break;
                    default:
                        break;
                }
                grouped[key].push_back(&drop);
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
