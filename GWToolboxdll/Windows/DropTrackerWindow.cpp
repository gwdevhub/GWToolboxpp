#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <ImGuiAddons.h>
#include <Windows/DropTrackerWindow.h>
#include <Modules/ItemDrops.h>
#include <map>
#include <fstream>
#include <unordered_map>
#include <Modules/Resources.h>

#include "Utils/TextUtils.h"
#include "Utils/TextUtils_Time.h"


namespace {
    using GroupMode = ItemDrops::GroupMode;

    GroupMode current_group_mode = GroupMode::None;
    const char* group_mode_names[] = {"None", "Item Name", "Map", "Rarity", "Type", "Weapon"};
    DropTrackerWindow::Settings settings;

    // Uniform view over a drop row, whether it's a live in-memory PendingDrop (recent window) or a row
    // parsed back out of the session CSV (older, already trimmed from memory). icon is null for
    // disk-sourced rows — the original GW::Item is long gone, so there's nothing to look an icon up from.
    struct DisplayRow {
        time_t system_time = 0;
        IDirect3DTexture9** icon = nullptr;
        std::wstring item_name;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        uint32_t quantity = 0;
        uint32_t value = 0;
        GW::Constants::ItemType type = GW::Constants::ItemType::Unknown;
        GW::Constants::Rarity rarity = GW::Constants::Rarity::Unknown;
        GW::Constants::DamageType damage_type = GW::Constants::DamageType::None;
        uint16_t min_damage = 0;
        uint16_t max_damage = 0;
        GW::Constants::AttributeByte requirement_attribute = GW::Constants::AttributeByte::None;
        uint8_t requirement_value = 0;
    };

    // Called only for rows an ImGuiListClipper actually draws (see DrawDefaultTable) - GetIcon() does a
    // cache lookup/fetch, so this keeps that scoped to what's visible rather than the whole window.
    DisplayRow ToDisplayRow(ItemDrops::PendingDrop* drop)
    {
        return {
            drop->system_time, drop->GetIcon(), drop->GetItemName()->wstring(), drop->map_id,
            drop->quantity, drop->value, drop->type, drop->rarity, drop->damage_type,
            drop->min_damage, drop->max_damage, drop->requirement_attribute, drop->requirement_value
        };
    }

    // Per-key-type overloads instead of a GroupMode switch: each tally dimension now uses its native
    // game identifier as the key (see ItemDrops::GetTallyBy*), so there's no single "key" type to
    // switch on any more. LabelFor turns a key into the text a group's row shows; RowMatchesKey filters
    // parsed CSV rows during drill-down without needing to format/decode anything just to compare.
    std::string LabelFor(const std::string& key) { return key.empty() ? "(Unknown)" : key; }
    std::string LabelFor(GW::Constants::MapID key) { auto name = Resources::GetMapName(key)->string(); return name.empty() ? "(Unknown)" : name; }
    std::string LabelFor(GW::Constants::Rarity key) { return GW::Items::GetRarityName(key); }
    std::string LabelFor(GW::Constants::ItemType key) { return GW::Items::GetItemTypeName(key); }
    std::string LabelFor(const ItemDrops::WeaponKey& key) { return ItemDrops::WeaponCategoryName(key.type, key.damage_type); }

    bool RowMatchesKey(const DisplayRow& row, const std::string& key) { return TextUtils::WStringToString(row.item_name) == key; }
    bool RowMatchesKey(const DisplayRow& row, GW::Constants::MapID key) { return row.map_id == key; }
    bool RowMatchesKey(const DisplayRow& row, GW::Constants::Rarity key) { return row.rarity == key; }
    bool RowMatchesKey(const DisplayRow& row, GW::Constants::ItemType key) { return row.type == key; }
    bool RowMatchesKey(const DisplayRow& row, const ItemDrops::WeaponKey& key) { return row.type == key.type && row.damage_type == key.damage_type; }

    std::string CacheKeyFor(GroupMode mode, const std::string& key) { return std::to_string(static_cast<int>(mode)) + "|" + key; }
    std::string CacheKeyFor(GroupMode mode, GW::Constants::MapID key) { return std::to_string(static_cast<int>(mode)) + "|" + std::to_string(static_cast<int>(key)); }
    std::string CacheKeyFor(GroupMode mode, GW::Constants::Rarity key) { return std::to_string(static_cast<int>(mode)) + "|" + std::to_string(static_cast<int>(key)); }
    std::string CacheKeyFor(GroupMode mode, GW::Constants::ItemType key) { return std::to_string(static_cast<int>(mode)) + "|" + std::to_string(static_cast<int>(key)); }
    std::string CacheKeyFor(GroupMode mode, const ItemDrops::WeaponKey& key)
    {
        return std::to_string(static_cast<int>(mode)) + "|" + std::to_string(static_cast<int>(key.type)) + "," + std::to_string(static_cast<int>(key.damage_type));
    }

    // RFC4180-ish: only ItemName is ever quoted (TextUtils::SanitizeForCSV), but a generic quote-aware
    // splitter handles that without needing to know which column it is.
    std::vector<std::wstring> SplitCsvLine(const std::wstring& line)
    {
        std::vector<std::wstring> cols;
        std::wstring cur;
        bool in_quotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            const wchar_t c = line[i];
            if (in_quotes) {
                if (c == L'"') {
                    if (i + 1 < line.size() && line[i + 1] == L'"') {
                        cur += L'"';
                        ++i;
                    }
                    else {
                        in_quotes = false;
                    }
                }
                else {
                    cur += c;
                }
            }
            else if (c == L'"') {
                in_quotes = true;
            }
            else if (c == L',') {
                cols.push_back(std::move(cur));
                cur.clear();
            }
            else {
                cur += c;
            }
        }
        cols.push_back(std::move(cur));
        return cols;
    }

    // Column order per ItemDrops::PendingDrop::GetCSVHeader():
    // SystemTime,InstanceTime,Map,ItemName,Quantity,Value,ItemType,Rarity,DamageType,MinDamage,MaxDamage,
    // RequirementAttribute,RequirementValue,PlayerCount,HeroCount,HenchmanCount,HardMode,ModelFileID
    bool ParseCsvRow(const std::wstring& line, DisplayRow& out)
    {
        const auto cols = SplitCsvLine(line);
        if (cols.size() < 13) {
            return false;
        }
        try {
            out.system_time = static_cast<time_t>(std::stoll(cols[0]));
            out.map_id = static_cast<GW::Constants::MapID>(std::stoul(cols[2]));
            out.item_name = cols[3];
            out.quantity = static_cast<uint32_t>(std::stoul(cols[4]));
            out.value = static_cast<uint32_t>(std::stoul(cols[5]));
            out.type = static_cast<GW::Constants::ItemType>(std::stoul(cols[6]));
            out.rarity = static_cast<GW::Constants::Rarity>(std::stoul(cols[7]));
            out.damage_type = static_cast<GW::Constants::DamageType>(std::stoul(cols[8]));
            out.min_damage = static_cast<uint16_t>(std::stoul(cols[9]));
            out.max_damage = static_cast<uint16_t>(std::stoul(cols[10]));
            out.requirement_attribute = static_cast<GW::Constants::AttributeByte>(std::stoul(cols[11]));
            out.requirement_value = static_cast<uint8_t>(std::stoul(cols[12]));
        }
        catch (const std::exception&) {
            return false; // malformed/partial line (e.g. a write caught mid-flush); skip it
        }
        return true;
    }

    struct DrilldownCacheEntry {
        uint64_t cached_count = 0;
        std::vector<DisplayRow> rows;
    };
    std::unordered_map<std::string, DrilldownCacheEntry> drilldown_cache; // key: "<mode>|<group key>"

    // Individual rows for one group, for the expandable per-item list. Sourced entirely from the
    // session CSV (the single durable record of every drop, not just what's still in memory) rather
    // than from drop_history, so drill-down works the same whether the group's drops are recent or
    // long since trimmed. Cached per group and only re-read when that group's tally count changes.
    template <typename Key>
    const std::vector<DisplayRow>& GetDrilldownRows(GroupMode mode, const Key& key, uint64_t expected_count)
    {
        auto& entry = drilldown_cache[CacheKeyFor(mode, key)];
        if (entry.cached_count == expected_count) {
            return entry.rows;
        }

        entry.rows.clear();
        entry.cached_count = expected_count;

        const auto path = ItemDrops::Instance().GetSessionCsvPath();
        std::wifstream file(path);
        if (!file.is_open()) {
            return entry.rows;
        }
        std::wstring line;
        std::getline(file, line); // header
        while (std::getline(file, line)) {
            DisplayRow row;
            if (!ParseCsvRow(line, row)) {
                continue;
            }
            if (RowMatchesKey(row, key)) {
                entry.rows.push_back(std::move(row));
            }
        }
        return entry.rows;
    }

    void DrawItemIcon(const DisplayRow& row)
    {
        if (settings.icon_size > 0 && row.icon) {
            ImGui::Image((ImTextureID)(intptr_t)*row.icon, ImVec2(settings.icon_size, settings.icon_size));
        }
    }

    void DrawItemNameCell(const DisplayRow& drop)
    {
        ImGui::TextColoredUnformatted(GW::Items::GetRarityColor(drop.rarity), TextUtils::WStringToString(drop.item_name).c_str());
    }

    // Takes the live PendingDrop list (not pre-converted DisplayRows) so ToDisplayRow - and the icon
    // fetch/cache lookup inside it - only runs for rows the clipper actually decides to draw, instead
    // of for the whole (up to ~2000-entry) rolling window every frame regardless of scroll position.
    void DrawDefaultTable(const std::vector<ItemDrops::PendingDrop*>& drops)
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

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(drops.size()));
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const DisplayRow drop = ToDisplayRow(drops[i]);

                    std::tm tm_buf = TextUtils::Time::SafeLocaltime(drop.system_time);
                    char time_str[32];
                    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawItemIcon(drop);

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(time_str);
                    ImGui::TableNextColumn();

                    DrawItemNameCell(drop);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(GW::Items::GetItemTypeName(drop.type));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(GW::Items::GetRarityName(drop.rarity));
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", drop.quantity);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", drop.value);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(Resources::GetMapName(drop.map_id)->string().c_str());
                }
            }
            clipper.End();
            ImGui::EndTable();
        }
    }

    template <typename Key>
    void DrawDefaultGroupTable(GroupMode mode, const std::unordered_map<Key, ItemDrops::DropTally>& tally)
    {
        if (ImGui::BeginTable("grouped_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(group_mode_names[static_cast<int>(mode)], ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Total Qty", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            int group_idx = 0;
            for (const auto& [key, totals] : tally) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushID(group_idx++);

                const std::string label = LabelFor(key);
                bool open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(totals.count));
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(totals.quantity));

                if (open) {
                    const auto& items = GetDrilldownRows(mode, key, totals.count);
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(items.size()));
                    while (clipper.Step()) {
                        for (int item_idx = clipper.DisplayStart; item_idx < clipper.DisplayEnd; ++item_idx) {
                            const auto& drop = items[item_idx];
                            ImGui::PushID(item_idx);

                            std::tm tm_buf = TextUtils::Time::SafeLocaltime(drop.system_time);
                            char time_str[32];
                            std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (mode == GroupMode::ItemName) {
                                DrawItemIcon(drop);
                            }
                            ImGui::TextUnformatted(time_str);
                            DrawItemNameCell(drop);
                            ImGui::TableNextColumn();
                            ImGui::Text("%d", drop.quantity);
                            ImGui::TableNextColumn();

                            ImGui::PopID();
                        }
                    }
                    clipper.End();
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void DrawWeaponsTable(GroupMode mode, const std::unordered_map<ItemDrops::WeaponKey, ItemDrops::DropTally, ItemDrops::WeaponKeyHash>& tally)
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
            for (const auto& [key, totals] : tally) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushID(group_idx++);
                const std::string label = LabelFor(key);
                bool open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(totals.count));

                // Damage range / requirement come straight from the running tally now - correct whether
                // this row is expanded or not, no disk read needed just to show the summary.
                ImGui::TableNextColumn();
                if (totals.min_damage != UINT16_MAX && totals.max_damage > 0) {
                    ImGui::Text("%d-%d", totals.min_damage, totals.max_damage);
                }
                else {
                    ImGui::TextUnformatted("-");
                }

                ImGui::TableNextColumn();
                if (!totals.requirements.empty()) {
                    const std::string req_text = TextUtils::Join(std::vector(totals.requirements.begin(), totals.requirements.end()), ", ");
                    ImGui::TextUnformatted(req_text.c_str());
                }
                else {
                    ImGui::TextUnformatted("-");
                }

                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(totals.quantity));

                ImGui::TableNextColumn();
                ImGui::Text("%lld", totals.count ? totals.gold_value / static_cast<int64_t>(totals.count) : 0);

                // The individual rows are still only needed (and only read from disk) when expanded.
                if (open) {
                    const auto& items = GetDrilldownRows(mode, key, totals.count);
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(items.size()));
                    while (clipper.Step()) {
                        for (int item_idx = clipper.DisplayStart; item_idx < clipper.DisplayEnd; ++item_idx) {
                            const auto& drop = items[item_idx];
                            ImGui::PushID(item_idx);

                            std::tm tm_buf = TextUtils::Time::SafeLocaltime(drop.system_time);
                            char time_str[32];
                            std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            DrawItemNameCell(drop);
                            ImGui::TableNextColumn();
                            ImGui::Text("%d", drop.quantity);
                            ImGui::TableNextColumn();
                            if (drop.min_damage > 0 && drop.max_damage > 0) {
                                ImGui::Text("%d-%d", drop.min_damage, drop.max_damage);
                            }

                            ImGui::TableNextColumn();
                            if (drop.requirement_attribute != GW::Constants::AttributeByte::None) {
                                ImGui::Text("%d %s", drop.requirement_value, GW::Items::GetAttributeName(drop.requirement_attribute));
                            }

                            ImGui::TableNextColumn();
                            ImGui::Text("%d", drop.quantity);

                            ImGui::TableNextColumn();
                            ImGui::Text("%d", drop.value);

                            ImGui::PopID();
                        }
                    }
                    clipper.End();
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
        auto& recent_drops = ItemDrops::Instance().GetDropHistory(); // rolling window; full session is in the tallies + on-disk CSV

        if (!ItemDrops::Instance().IsTrackingEnabled()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Drop tracking is disabled. Enable it in Item Settings to use this.");
            ImGui::Separator();
        }

        if (ImGui::Button("Clear")) {
            ItemDrops::Instance().ClearDropHistory();
            drilldown_cache.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to disk")) {
            std::filesystem::path filename = "drops.csv";
            filename = Resources::GetPath(filename);
            Resources::SaveFileDialog([](const char* chosen_path) {
                if (chosen_path) {
                    ItemDrops::Instance().AddPendingExport(std::string(chosen_path));
                }
            }, "csv", filename.string().c_str());
        }
        ImGui::SameLine();
        ImGui::Text("Total drops: %llu", static_cast<unsigned long long>(ItemDrops::Instance().GetSessionDropCount()));
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
            DrawDefaultTable(recent_drops);
        }
        else {
            switch (current_group_mode) {
                case GroupMode::ItemName:
                    DrawDefaultGroupTable(current_group_mode, ItemDrops::Instance().GetTallyByItemName());
                    break;
                case GroupMode::Map:
                    DrawDefaultGroupTable(current_group_mode, ItemDrops::Instance().GetTallyByMap());
                    break;
                case GroupMode::Rarity:
                    DrawDefaultGroupTable(current_group_mode, ItemDrops::Instance().GetTallyByRarity());
                    break;
                case GroupMode::Type:
                    DrawDefaultGroupTable(current_group_mode, ItemDrops::Instance().GetTallyByType());
                    break;
                case GroupMode::Weapon:
                    DrawWeaponsTable(current_group_mode, ItemDrops::Instance().GetTallyByWeapon());
                    break;
                default:
                    break;
            }
        }
    }
    ImGui::End();
}

void DropTrackerWindow::DrawSettingsInternal() {
    ImGui::DragFloat("Item Icon Size", &settings.icon_size, 16, 0, 64);
}

void DropTrackerWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
}

void DropTrackerWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void DropTrackerWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}
