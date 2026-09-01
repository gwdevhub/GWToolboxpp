#include "stdafx.h"

#include <unordered_set>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/WorldMapWidget.h>
#include <Color.h>
#include <GWToolbox.h>
#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#define BTN_WIDTH 20.0f

using namespace std::string_literals;

constexpr auto ini_filename = L"Markers.ini";
constexpr auto json_filename = L"Markers.json";

CustomRenderer::CustomLine::CustomLine(const float x1, const float y1, const float x2, const float y2, const GW::Constants::MapID m, const char* _name, bool draw_everywhere)
    : p1(x1, y1, 0),
      p2(x2, y2, 0),
      map(m),
      draw_everywhere(draw_everywhere)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "线条");
}

CustomRenderer::CustomLine::CustomLine(GW::GamePos p1, GW::GamePos p2, GW::Constants::MapID m, const char* n, bool draw_everywhere)
    : p1(p1),
      p2(p2),
      map(m),
      draw_everywhere(draw_everywhere)
{
    std::snprintf(name, sizeof(name), "%s", n ? n : "线条");
}

CustomRenderer::CustomMarker::CustomMarker(const float x, const float y, const float s, const Shape sh, const GW::Constants::MapID m, const char* _name)
    : pos(x, y, 0),
      size(s),
      shape(sh),
      map(m)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "标记");
}

CustomRenderer::CustomPolygon::CustomPolygon(const GW::Constants::MapID m, const char* _name)
    : map(m)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "多边形");
};

void CustomRenderer::RegisterSettings(ToolboxModule* module)
{
    // SettingColor 与 Color 布局兼容；强制转换使注册表能将其持久化为十六进制字符串
    SettingsRegistry::RegisterField(module, "color_custom_markers", reinterpret_cast<Colors::SettingColor*>(&color));
    SettingsRegistry::RegisterField(module, "color_hero_flag_circles", reinterpret_cast<Colors::SettingColor*>(&color_hero_flags_));
    SettingsRegistry::RegisterField(module, "hero_flag_circle_thickness", &hero_flag_line_thickness_);
}

void CustomRenderer::LoadMarkers()
{
    lines.clear();
    markers.clear();
    polygons.clear();

    const auto json_path = Resources::GetSettingFile(json_filename);
    std::error_code ec;
    if (std::filesystem::exists(json_path, ec)) {
        std::ifstream file(json_path, std::ios::binary);
        const std::string json_buf{std::istreambuf_iterator(file), {}};
        MarkersFile data;
        if (!file || glz::read<glz::opts{.error_on_unknown_keys = false}>(data, json_buf)) {
            // 保持 markers_loaded 未设置，以便保存不会覆盖不可读的文件
            Log::Error("解析 Markers.json 失败");
            markers_changed = true;
            return;
        }
        for (const auto& entry : data.lines) {
            const auto line = new CustomLine(entry.name.c_str());
            line->p1 = {entry.x1, entry.y1, 0};
            line->p2 = {entry.x2, entry.y2, 0};
            line->map = static_cast<GW::Constants::MapID>(entry.map);
            line->color = entry.color;
            line->visible = entry.visible;
            line->draw_on_terrain = entry.draw_on_terrain;
            lines.push_back(line);
        }
        for (const auto& entry : data.markers) {
            auto& marker = markers.emplace_back(entry.x, entry.y, entry.size, static_cast<Shape>(entry.shape), static_cast<GW::Constants::MapID>(entry.map), entry.name.c_str());
            marker.color = entry.color;
            marker.color_sub = entry.color_sub;
            marker.visible = entry.visible;
            marker.draw_on_terrain = entry.draw_on_terrain;
        }
        for (const auto& entry : data.polygons) {
            auto& polygon = polygons.emplace_back(static_cast<GW::Constants::MapID>(entry.map), entry.name.c_str());
            for (const auto& point : entry.points) {
                if (polygon.points.size() >= CustomPolygon::max_points) {
                    break;
                }
                polygon.points.emplace_back(point.x, point.y, 0);
            }
            polygon.filled = entry.filled;
            polygon.color = entry.color;
            polygon.color_sub = entry.color_sub;
            polygon.visible = entry.visible;
            polygon.draw_on_terrain = entry.draw_on_terrain;
        }
    }
    else {
        // 旧版回退；Markers.ini 从此处读取，下次保存时写入 json
        ToolboxIni inifile;
        ASSERT(inifile.LoadIfExists(Resources::GetLegacySettingFile(ini_filename).c_str()) == SI_OK);

        TNamesDepend entries;
        inifile.GetAllSections(entries);
        for (const auto& entry : entries) {
            const char* section = entry.pItem;
            if (!section) {
                continue;
            }
            if (strncmp(section, "customline", "customline"s.length()) == 0) {
                auto line = new CustomLine(inifile.GetValue(section, "name", "线条"));
                line->p1.x = static_cast<float>(inifile.GetDoubleValue(section, "x1", 0.0));
                line->p1.y = static_cast<float>(inifile.GetDoubleValue(section, "y1", 0.0));
                line->p2.x = static_cast<float>(inifile.GetDoubleValue(section, "x2", 0.0));
                line->p2.y = static_cast<float>(inifile.GetDoubleValue(section, "y2", 0.0));
                line->map = static_cast<GW::Constants::MapID>(inifile.GetLongValue(section, "map", 0));
                line->color = Colors::Load(&inifile, section, "color", line->color);
                line->visible = inifile.GetBoolValue(section, "visible", true);
                line->draw_on_terrain = inifile.GetBoolValue(section, "draw_on_terrain", false);
                lines.push_back(line);
            }
            else if (strncmp(section, "custommarker", "custommarker"s.length()) == 0) {
                auto marker = CustomMarker(inifile.GetValue(section, "name", "标记"));
                marker.pos.x = static_cast<float>(inifile.GetDoubleValue(section, "x", 0.0));
                marker.pos.y = static_cast<float>(inifile.GetDoubleValue(section, "y", 0.0));
                marker.size = static_cast<float>(inifile.GetDoubleValue(section, "size", 0.0));
                marker.shape = static_cast<Shape>(inifile.GetLongValue(section, "shape", 0));
                marker.map = static_cast<GW::Constants::MapID>(inifile.GetLongValue(section, "map", 0));
                marker.color = Colors::Load(&inifile, section, "color", marker.color);
                marker.color_sub = Colors::Load(&inifile, section, "color_sub", marker.color_sub);
                marker.visible = inifile.GetBoolValue(section, "visible", true);
                marker.draw_on_terrain = inifile.GetBoolValue(section, "draw_on_terrain", false);
                markers.push_back(marker);
            }
            else if (strncmp(section, "custompolygon", "custompolygon"s.length()) == 0) {
                auto polygon = CustomPolygon(inifile.GetValue(section, "name", "多边形"));
                for (auto i = 0; i < CustomPolygon::max_points; i++) {
                    GW::Vec2f vec;
                    vec.x = static_cast<float>(
                        inifile.GetDoubleValue(section, ("point["s + std::to_string(i) + "].x").c_str(), std::numeric_limits<float>::max()));
                    vec.y = static_cast<float>(
                        inifile.GetDoubleValue(section, ("point["s + std::to_string(i) + "].y").c_str(), std::numeric_limits<float>::max()));
                    if (vec.x != std::numeric_limits<float>::max() && vec.y != std::numeric_limits<float>::max()) {
                        polygon.points.emplace_back(vec);
                    }
                    else {
                        break;
                    }
                }
                polygon.filled = inifile.GetBoolValue(section, "filled", polygon.filled);
                polygon.color = Colors::Load(&inifile, section, "color", polygon.color);
                polygon.color_sub = Colors::Load(&inifile, section, "color_sub", polygon.color_sub);
                polygon.map = static_cast<GW::Constants::MapID>(inifile.GetLongValue(section, "map", 0));
                polygon.visible = inifile.GetBoolValue(section, "visible", true);
                polygon.draw_on_terrain = inifile.GetBoolValue(section, "draw_on_terrain", false);
                polygons.push_back(polygon);
            }
        }
    }

    marker_file_dirty = false;
    markers_changed = true;
    markers_loaded = true;
}

void CustomRenderer::SaveMarkers()
{
    if ((marker_file_dirty || GWToolbox::SettingsFolderChanged()) && markers_loaded) {
        MarkersFile data;
        for (const auto line : lines) {
            if (line->created_by_toolbox) {
                continue;
            }
            auto& entry = data.lines.emplace_back();
            entry.name = line->name;
            entry.x1 = line->p1.x;
            entry.y1 = line->p1.y;
            entry.x2 = line->p2.x;
            entry.y2 = line->p2.y;
            entry.color = line->color;
            entry.map = static_cast<uint32_t>(line->map);
            entry.visible = line->visible;
            entry.draw_on_terrain = line->draw_on_terrain;
        }
        for (const auto& marker : markers) {
            auto& entry = data.markers.emplace_back();
            entry.name = marker.name;
            entry.x = marker.pos.x;
            entry.y = marker.pos.y;
            entry.size = marker.size;
            entry.shape = static_cast<int>(marker.shape);
            entry.map = static_cast<uint32_t>(marker.map);
            entry.visible = marker.visible;
            entry.draw_on_terrain = marker.draw_on_terrain;
            entry.color = marker.color;
            entry.color_sub = marker.color_sub;
        }
        for (const auto& polygon : polygons) {
            auto& entry = data.polygons.emplace_back();
            entry.name = polygon.name;
            for (const auto& point : polygon.points) {
                entry.points.push_back({point.x, point.y});
            }
            entry.color = polygon.color;
            entry.color_sub = polygon.color_sub;
            entry.map = static_cast<uint32_t>(polygon.map);
            entry.visible = polygon.visible;
            entry.draw_on_terrain = polygon.draw_on_terrain;
            entry.filled = polygon.filled;
        }

        std::string json_buf;
        ASSERT(!glz::write<glz::opts{.prettify = true}>(data, json_buf));
        std::ofstream file(Resources::GetSettingFile(json_filename), std::ios::binary | std::ios::trunc);
        file.write(json_buf.data(), static_cast<std::streamsize>(json_buf.size()));
        ASSERT(file.good());
        marker_file_dirty = false;
    }
}

void CustomRenderer::Invalidate()
{
    D3DVertexBuffer::Invalidate();
    hero_circles_.Invalidate();
    for (auto& m : markers) {
        m.Invalidate();
    }
    for (auto& m : polygons) {
        m.Invalidate();
    }
}

void CustomRenderer::SetTooltipMapID(const GW::Constants::MapID& map_id)
{
    ImGui::SetTooltip(std::format("地图 ID（{}）", Resources::GetMapName(map_id)->string()).c_str());
}

bool CustomRenderer::RemoveCustomLine(CustomLine* line)
{
    const auto found = std::ranges::find(lines, line);
    if (found != lines.end()) {
        delete *found;
        lines.erase(found);
        markers_changed = true;
        return true;
    }
    return false;
}

void CustomRenderer::RemoveCustomLines(const std::vector<CustomLine*>& lines_to_remove)
{
    if (lines_to_remove.empty()) return;
    const std::unordered_set<CustomLine*> dead(lines_to_remove.begin(), lines_to_remove.end());
    const size_t before = lines.size();
    std::erase_if(lines, [&dead](CustomLine* l) {
        if (dead.contains(l)) {
            delete l;
            return true;
        }
        return false;
    });
    if (lines.size() != before) markers_changed = true;
}

CustomRenderer::CustomLine* CustomRenderer::AddCustomLine(const GW::GamePos& from, const GW::GamePos& to, const char* _name, bool draw_everywhere)
{
    const auto line = new CustomLine(from, to, GW::Map::GetMapID(), _name, draw_everywhere);
    lines.push_back(line);
    markers_changed = true;
    return line;
}

void CustomRenderer::DrawLineSettings()
{
    if (Colors::DrawSettingHueWheel("颜色", &color)) {
        Invalidate();
    }
    
    size_t n_lines = std::count_if(lines.begin(), lines.end(), [](const auto& line) {
        return !line->created_by_toolbox;
    });
    
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("lines");
    for (size_t i = 0; i < lines.size(); i++) {
        CustomLine& line = *lines[i];
        if (line.created_by_toolbox)
            continue;
        ImGui::PushID(static_cast<int>(i));
        markers_changed |= ImGui::Checkbox("##visible", &line.visible);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("可见");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth((ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 5) / 5);

        markers_changed |= ImGui::DragFloat("##x1", &line.p1.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条 X 1");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y1", &line.p1.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条 Y 1");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##x2", &line.p2.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条 X 2");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y2", &line.p2.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条 Y 2");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::ColorButtonPicker("##color", &line.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条颜色");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::InputInt("##map", (int*)&line.map, 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(line.map);
        }
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 4 - BTN_WIDTH * 4);
        markers_changed |= ImGui::InputText("##name", line.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("名称");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &line.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在游戏内地形上绘制");
        }
        ImGui::SameLine(0.0f, spacing);

        if (i > 0) {
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("上移");
            }
            if (move_up) {
                std::swap(lines[i], lines[i - 1]);
                markers_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        if (i < n_lines - 1) {
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("下移");
            }
            if (move_down) {
                std::swap(lines[i], lines[i + 1]);
                markers_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        const bool remove = ImGui::Button("x##delete", ImVec2(BTN_WIDTH, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("删除");
        }
        ImGui::PopID();
        if (remove) {
            lines.erase(lines.begin() + static_cast<int>(i));
            markers_changed = true;
        }
    }
    ImGui::PopID();
    if (ImGui::Button("添加线条")) {
        char buf[32];
        snprintf(buf, 32, "线条%zu", lines.size());
        lines.push_back(new CustomLine(buf));
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_lines = false;
    if (ImGui::ConfirmButton("按名称排序 A-Z##lines", &sort_lines, "按名称字母顺序排序所有线条？\n此操作不可撤销。")) {
        std::sort(lines.begin(), lines.end(), [](const CustomLine* a, const CustomLine* b) {
            return strcmp(a->name, b->name) < 0;
        });
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_lines_by_map = false;
    if (ImGui::ConfirmButton("按地图排序##lines", &sort_lines_by_map, "按地图 ID 排序所有线条？\n此操作不可撤销。")) {
        std::sort(lines.begin(), lines.end(), [](const CustomLine* a, const CustomLine* b) {
            if (a->map != b->map)
                return static_cast<uint32_t>(a->map) < static_cast<uint32_t>(b->map);
            return strcmp(a->name, b->name) < 0;
        });
        markers_changed = true;
    }
}

void CustomRenderer::DrawMarkerSettings()
{
    if (Colors::DrawSettingHueWheel("颜色", &color)) {
        Invalidate();
    }
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("markers");
    const auto input_item_width = (ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 8) / 8;
    for (size_t i = 0; i < markers.size(); i++) {
        CustomMarker& marker = markers[i];
        bool marker_changed = false;
        ImGui::PushID(static_cast<int>(i));
        marker_changed |= ImGui::Checkbox("##visible", &marker.visible);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("可见");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);
        marker_changed |= ImGui::DragFloat("##x", &marker.pos.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("标记 X 位置");
        }
        ImGui::SameLine(0.0f, spacing);
        marker_changed |= ImGui::DragFloat("##y", &marker.pos.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("标记 Y 位置");
        }
        ImGui::SameLine(0.0f, spacing);
        marker_changed |= ImGui::DragFloat("##size", &marker.size, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("大小");
        }
        ImGui::SameLine(0.0f, spacing);

        constexpr const char* types[] = {"圆形", "填充圆形"};
        marker_changed |= ImGui::Combo("##type", reinterpret_cast<int*>(&marker.shape), types, 2);
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::ColorButtonPicker("##colorsub", &marker.color_sub);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("此多边形内敌对单位的颜色。\n注意：Alpha 通道为 0 将禁用此颜色。");
        }
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::ColorButtonPicker("##color", &marker.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "圆形的颜色。\n注意：Alpha 通道为 0 将回退到默认颜色。");
        }
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::InputInt("##map", reinterpret_cast<int*>(&marker.map), 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(marker.map);
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 4 - BTN_WIDTH * 4);
        marker_changed |= ImGui::InputText("##name", marker.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("名称");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &marker.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在游戏内地形上绘制");
        }
        ImGui::SameLine(0.0f, spacing);

        if (i > 0) {
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("上移");
            }
            if (move_up) {
                std::swap(markers[i], markers[i - 1]);
                markers_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        if (i < markers.size() - 1) {
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("下移");
            }
            if (move_down) {
                std::swap(markers[i], markers[i + 1]);
                markers_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        const bool remove = ImGui::Button("x##delete", ImVec2(BTN_WIDTH, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("删除");
        }
        ImGui::PopID();
        if (marker_changed) {
            marker.Invalidate();
            markers_changed = true;
        }
        if (remove) {
            markers.erase(markers.begin() + static_cast<int>(i));
            for (auto& mark : markers) {
                mark.Invalidate();
            }
            markers_changed = true;
        }
    }
    ImGui::PopID();
    if (ImGui::Button("添加标记")) {
        char buf[32];
        snprintf(buf, 32, "标记%zu", markers.size());
        markers.push_back(CustomMarker(buf));
        // 向量大小增加并重新分配数组时使标记失效
        for (auto& mark : markers) {
            mark.Invalidate();
        }
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_markers = false;
    if (ImGui::ConfirmButton("按名称排序 A-Z##markers", &sort_markers, "按名称字母顺序排序所有标记？\n此操作不可撤销。")) {
        std::sort(markers.begin(), markers.end(), [](const CustomMarker& a, const CustomMarker& b) {
            return strcmp(a.name, b.name) < 0;
        });
        for (auto& mark : markers) {
            mark.Invalidate();
        }
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_markers_by_map = false;
    if (ImGui::ConfirmButton("按地图排序##markers", &sort_markers_by_map, "按地图 ID 排序所有标记？\n此操作不可撤销。")) {
        std::sort(markers.begin(), markers.end(), [](const CustomMarker& a, const CustomMarker& b) {
            if (a.map != b.map)
                return static_cast<uint32_t>(a.map) < static_cast<uint32_t>(b.map);
            return strcmp(a.name, b.name) < 0;
        });
        for (auto& mark : markers) {
            mark.Invalidate();
        }
        markers_changed = true;
    }
}

CustomRenderer::CustomMarker::CustomMarker(const char* name)
    : CustomMarker(0, 0, 100.0f, Shape::LineCircle, GW::Map::GetMapID(), name)
{
    if (const auto player = GW::Agents::GetControlledCharacter()) {
        pos.x = player->pos.x;
        pos.y = player->pos.y;
    }
}

CustomRenderer::CustomPolygon::CustomPolygon(const char* name)
    : CustomPolygon(GW::Map::GetMapID(), name) {}

void CustomRenderer::DrawPolygonSettings()
{
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("polygons");
    const float input_item_width = (ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 8) / 8;
    for (size_t i = 0; i < polygons.size(); i++) {
        bool polygon_changed = false;
        const auto signed_idx = static_cast<int>(i);
        const bool show_details = signed_idx == show_polygon_details;
        CustomPolygon& polygon = polygons.at(i);
        ImGui::PushID(signed_idx);
        polygon_changed |= ImGui::Checkbox("##visible", &polygon.visible);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("可见");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);

        if (show_details) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        char show_points_label_buf[40];
        snprintf(show_points_label_buf, sizeof(show_points_label_buf), "显示点（%d）##show_polygon_details", polygon.points.size());
        if (ImGui::Button(show_points_label_buf, ImVec2(input_item_width * 3 + spacing * 2, 0.f))) {
            show_polygon_details = show_details ? -1 : signed_idx;
        }
        if (show_details) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::Checkbox("##filled", &polygon.filled);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("填充 - 仅对最多 {} 个点生效！", CustomPolygon::max_points_filled);
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##colorsub", &polygon.color_sub);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("此多边形内敌对单位的颜色。\n\n注意：Alpha 通道为 0 将禁用此颜色。");
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##color", &polygon.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("地图上多边形的颜色。\n注意：Alpha 通道为 0 将禁用此颜色。");
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::InputInt("##map", reinterpret_cast<int*>(&polygon.map), 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(polygon.map);
        }
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 4 - BTN_WIDTH * 4);
        markers_changed |= ImGui::InputText("##name", polygon.name, 128);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("名称");
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, spacing);
        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &polygon.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在游戏内地形上绘制");
        }
        ImGui::SameLine(0.0f, spacing);

        if (i > 0) {
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("上移");
            }
            if (move_up) {
                std::swap(polygons[i], polygons[i - 1]);
                polygon_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        if (i < polygons.size() - 1) {
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("下移");
            }
            if (move_down) {
                std::swap(polygons[i], polygons[i + 1]);
                polygon_changed = true;
            }
            ImGui::SameLine(0.0f, spacing);
        }
        else {
            ImGui::SameLine(0.0f, BTN_WIDTH + spacing * 2);
        }

        const bool remove = ImGui::Button("x##delete", ImVec2(BTN_WIDTH, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("删除");
        }

        if (show_details) {
            ImGui::Indent();
            if (polygon.points.size() < CustomPolygon::max_points && ImGui::Button("添加多边形点##add")) {
                if (const auto player = GW::Agents::GetControlledCharacter()) {
                    polygon.points.emplace_back(player->pos);
                    polygon_changed = true;
                }
            }
            int remove_point = -1;
            for (auto j = 0u; j < polygon.points.size(); j++) {
                polygon_changed |= ImGui::InputFloat2(("##point"s + std::to_string(j)).c_str(),
                                                      reinterpret_cast<float*>(&polygon.points.at(j)), "%.0f");
                ImGui::SameLine();
                if (ImGui::Button(("x##"s + std::to_string(j)).c_str())) {
                    remove_point = j;
                }
            }
            if (remove_point > -1) {
                polygon.points.erase(polygon.points.begin() + remove_point);
                polygon_changed = true;
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
        if (remove) {
            polygons.erase(polygons.begin() + signed_idx);
            for (auto& poly : polygons) {
                poly.Invalidate();
            }
            markers_changed = true;
            break;
        }
        if (polygon_changed) {
            polygon.Invalidate();
        }
        markers_changed |= polygon_changed;
    }
    ImGui::PopID();
    if (ImGui::Button("添加多边形")) {
        char buf[32];
        snprintf(buf, 32, "多边形%zu", polygons.size());
        polygons.emplace_back(buf);
        // 向量大小增加并重新分配数组时使多边形失效
        for (auto& poly : polygons) {
            poly.Invalidate();
        }
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_polygons = false;
    if (ImGui::ConfirmButton("按名称排序 A-Z##polygons", &sort_polygons, "按名称字母顺序排序所有多边形？\n此操作不可撤销。")) {
        std::sort(polygons.begin(), polygons.end(), [](const CustomPolygon& a, const CustomPolygon& b) {
            return strcmp(a.name, b.name) < 0;
        });
        for (auto& poly : polygons) {
            poly.Invalidate();
        }
        markers_changed = true;
    }
    ImGui::SameLine();
    bool sort_polygons_by_map = false;
    if (ImGui::ConfirmButton("按地图排序##polygons", &sort_polygons_by_map, "按地图 ID 排序所有多边形？\n此操作不可撤销。")) {
        std::sort(polygons.begin(), polygons.end(), [](const CustomPolygon& a, const CustomPolygon& b) {
            if (a.map != b.map)
                return static_cast<uint32_t>(a.map) < static_cast<uint32_t>(b.map);
            return strcmp(a.name, b.name) < 0;
        });
        for (auto& poly : polygons) {
            poly.Invalidate();
        }
        markers_changed = true;
    }
}

void CustomRenderer::DrawSettings()
{
    if (ImGui::TreeNodeEx("英雄标记圆圈", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        bool changed = false;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::PushItemWidth(60.f);
        changed |= ImGui::DragFloat("##hero_flag_thickness", &hero_flag_line_thickness_, 0.1f, 0.1f, 20.f, "%.1fpx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("线条粗细（像素）");
        }
        ImGui::SameLine(0.f, spacing);
        changed |= ImGui::ColorButtonPicker("##hero_flag_color", &color_hero_flags_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("英雄标记圆圈颜色");
        }
        if (changed) {
            hero_circles_.Invalidate();
        }
        ImGui::TreePop();
    }
    const auto draw_note = [] {
        ImGui::Text("注意：自定义标记存储在设置文件夹的 'Markers.json' 中。你可以与其他玩家共享此文件，或将他人的标记粘贴到其中。");
    };
    if (ImGui::TreeNodeEx("自定义线条", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_lines", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 75.f + lines.size() * 25.f)});
        draw_note();
        DrawLineSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("自定义圆形", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_circles", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 75.f + markers.size() * 25.f)});
        draw_note();
        DrawMarkerSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("自定义多边形", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_polygons", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 50.f + polygons.size() * 25.f)});
        draw_note();
        DrawPolygonSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
}

void CustomRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_LINELIST;
    D3DVertexBuffer::Initialize(device);
}

void CustomRenderer::Terminate()
{
    D3DVertexBuffer::Terminate();
    hero_circles_.Terminate();
    for (const auto l : lines) {
        delete l;
    }
    lines.clear();
    for (auto& p : polygons) {
        p.D3DVertexBuffer::Terminate();
    }
    polygons.clear();
    for (auto& m : markers) {
        m.Terminate();
    }
    markers.clear();
}
void CustomRenderer::HeroCircles::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLESTRIP;
    vertices.clear();
    const auto BuildCircle = [&](const float radius) {
        const float diff = thickness / std::max(gwinches_per_pixel, 1e-4f);
        for (auto i = 0; i <= static_cast<int>(circle_triangles); i += 2) {
            const float angle = i / static_cast<float>(circle_triangles) * DirectX::XM_2PI;
            vertices.push_back({radius * cosf(angle), radius * sinf(angle), 0.f, color});
            vertices.push_back({(radius + diff) * cosf(angle), (radius + diff) * sinf(angle), 0.f, color});
        }
    };
    BuildCircle(200.f);
    BuildCircle(300.f);
    D3DVertexBuffer::Initialize(device);
}

void CustomRenderer::HeroCircles::Update(const DWORD c, const float t, const float gpp)
{
    if (color == c && thickness == t && gwinches_per_pixel == gpp) return;
    color = c;
    thickness = t;
    gwinches_per_pixel = gpp;
    Invalidate();
}

void CustomRenderer::HeroCircles::RenderAt(IDirect3DDevice9* device, const float x, const float y, const bool is_allflag)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (!buffer) return;
    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
    const auto translate = DirectX::XMMatrixTranslation(x, y, 0.0f);
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
    const auto offset = static_cast<UINT>(is_allflag ? circle_points : 0);
    device->DrawPrimitive(D3DPT_TRIANGLESTRIP, offset, static_cast<UINT>(circle_triangles));
}

void CustomRenderer::Render(IDirect3DDevice9* device, const float gwinches_per_pixel)
{
    gwinches_per_pixel_ = gwinches_per_pixel;
    Render(device);
}

void CustomRenderer::CustomPolygon::Initialize(IDirect3DDevice9* device)
{
    vertices.clear();
    if (filled) {
        if (points.size() < 3) return;
        type = D3DPT_TRIANGLELIST;
        const auto poly = std::vector{{points}};
        const auto point_indices = mapbox::earcut<unsigned>(poly);
        vertices.reserve(point_indices.size());
        for (const auto idx : point_indices) {
            vertices.push_back({points[idx].x, points[idx].y, 0.f, color});
        }
    }
    else {
        if (points.size() < 2) return;
        type = D3DPT_LINESTRIP;
        vertices.reserve(points.size() + 1);
        for (const auto& p : points) {
            vertices.push_back({p.x, p.y, 0.f, color});
        }
        vertices.push_back(vertices.front());
    }
    D3DVertexBuffer::Initialize(device);
}

void CustomRenderer::CustomPolygon::Render(IDirect3DDevice9* device)
{
    if (filled ? points.size() < 3 : points.size() < 2) return;
    if (!visible) return;
    if (map != GW::Constants::MapID::None && map != GW::Map::GetMapID()) return;
    D3DVertexBuffer::Render(device);
}
void CustomRenderer::CustomMarker::SyncGeometry()
{
    const Color colour = (color & IM_COL32_A_MASK) == 0 ? CustomRenderer::color : color;
    if (shape == Shape::FullCircle) {
        const Color centre_color = Colors::Sub(colour, Colors::ARGB(50, 0, 0, 0));
        fill_circle.SetColor(colour);
        fill_circle.SetCenterColor(centre_color);
        fill_circle.SetRadius(1.f);
    }
    else {
        line_circle.SetColor(colour);
        line_circle.SetRadius(1.f);
    }
}

void CustomRenderer::CustomMarker::Invalidate()
{
    fill_circle.Invalidate();
    line_circle.Invalidate();
}

void CustomRenderer::CustomMarker::Terminate()
{
    fill_circle.Terminate();
    line_circle.Terminate();
}

void CustomRenderer::CustomMarker::Render(IDirect3DDevice9* device)
{
    if (!visible || (map != GW::Constants::MapID::None && map != GW::Map::GetMapID())) {
        return;
    }
    SyncGeometry();

    const auto translate = DirectX::XMMatrixTranslation(pos.x, pos.y, 0.0f);
    const auto scale = DirectX::XMMatrixScaling(size, size, 1.0f);
    const auto world = scale * translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));

    if (shape == Shape::FullCircle)
        fill_circle.Render(device);
    else
        line_circle.Render(device);
}

void CustomRenderer::Render(IDirect3DDevice9* device)
{
    if (markers_changed) {
        GameWorldRenderer::TriggerSyncAllMarkers();
        marker_file_dirty = true;
        markers_changed = false;
        Invalidate();
        // 不要返回：下面的绘制会在本帧重新上传缓冲区。跳过它会在清除并重新添加路径时导致一帧闪烁。
    }

    DrawCustomMarkers(device);

    DrawCustomLines(device);

    D3DVertexBuffer::Render(device);
}

void CustomRenderer::DrawCustomMarkers(IDirect3DDevice9* device)
{
    if (!Minimap::ShouldMarkersDrawOnMap()) {
        return;
    }

    for (CustomPolygon& polygon : polygons) {
        polygon.Render(device);
    }

    for (CustomMarker& marker : markers) {
        marker.Render(device);
    }

    hero_circles_.Update(color_hero_flags_, hero_flag_line_thickness_, gwinches_per_pixel_);
    if (GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags; flags.valid()) {
        for (const auto& flag : flags) {
            if (!std::isfinite(flag.flag.x)) continue;
            hero_circles_.RenderAt(device, flag.flag.x, flag.flag.y, false);
        }
    }
    if (const GW::Vec3f allflag = GW::GetGameContext()->world->all_flag; std::isfinite(allflag.x)) {
        hero_circles_.RenderAt(device, allflag.x, allflag.y, true);
    }
    const auto xmi = DirectX::XMMatrixIdentity();
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&xmi));
}

void CustomRenderer::DrawCustomLines(const IDirect3DDevice9*)
{
    // 以 30fps 重建，而非每帧：重建将缓冲区标记为脏并强制 Lock/memcpy 重新上传。
    static clock_t last_check = 0;
    if (!ToolboxUtils::FrameRateCheck(last_check, 30)) return;

    const auto doa_outpost = GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable && GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish;
    const auto my_pos = GW::PlayerMgr::GetPlayerPosition();
    vertices.clear();
    for (const auto line : lines) {
        if (!line->visible || !line->draw_on_minimap) continue;
        if (line->map != GW::Constants::MapID::None && line->map != GW::Map::GetMapID()) continue;
        if (doa_outpost && !line->draw_everywhere) continue;

        if (line->world_coords) {
            GW::GamePos g1, g2;
            if (!WorldMapWidget::WorldMapToGamePos({line->p1.x, line->p1.y}, g1) || !WorldMapWidget::WorldMapToGamePos({line->p2.x, line->p2.y}, g2)) continue;
            vertices.push_back({g1.x, g1.y, 0.f, line->color});
            vertices.push_back({g2.x, g2.y, 0.f, line->color});
            dirty = true;
            continue;
        }

        if (line->from_player_pos && my_pos) {
            vertices.push_back({my_pos->x, my_pos->y, 0.f, line->color});
        }
        else {
            vertices.push_back({line->p1.x, line->p1.y, 0.f, line->color});
        }
        vertices.push_back({line->p2.x, line->p2.y, 0.f, line->color});
        dirty = true;
    }
}
