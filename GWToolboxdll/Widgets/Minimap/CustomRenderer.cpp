#include "stdafx.h"

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
#include <Color.h>
#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

using namespace std::string_literals;

constexpr auto ini_filename = L"Markers.ini";

namespace {
    ToolboxIni inifile{};
}

CustomRenderer::CustomLine::CustomLine(const float x1, const float y1, const float x2, const float y2, const GW::Constants::MapID m, const char* _name, bool draw_everywhere)
    : p1(x1, y1, 0),
      p2(x2, y2, 0),
      map(m),
      draw_everywhere(draw_everywhere)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "line");
}

CustomRenderer::CustomLine::CustomLine(GW::GamePos p1, GW::GamePos p2, GW::Constants::MapID m, const char* n, bool draw_everywhere)
    : p1(p1),
      p2(p2),
      map(m),
      draw_everywhere(draw_everywhere)
{
    std::snprintf(name, sizeof(name), "%s", n ? n : "line");
}

CustomRenderer::CustomMarker::CustomMarker(const float x, const float y, const float s, const Shape sh, const GW::Constants::MapID m, const char* _name)
    : pos(x, y, 0),
      size(s),
      shape(sh),
      map(m)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "marker");
}

CustomRenderer::CustomPolygon::CustomPolygon(const GW::Constants::MapID m, const char* _name)
    : map(m)
{
    std::snprintf(name, sizeof(name), "%s", _name ? _name : "polygon");
};

void CustomRenderer::LoadSettings(const ToolboxIni* ini, const char* section)
{
    color = Colors::Load(ini, section, "color_custom_markers", 0xFFFFFFFF);
    Invalidate();
    LoadMarkers();
}

void CustomRenderer::LoadMarkers()
{
    // clear current markers
    lines.clear();
    markers.clear();
    polygons.clear();

    ASSERT(inifile.LoadIfExists(Resources::GetSettingFile(ini_filename).c_str()) == SI_OK);

    // then load new
    ToolboxIni::TNamesDepend entries;
    inifile.GetAllSections(entries);
    for (const ToolboxIni::Entry& entry : entries) {
        const char* section = entry.pItem;
        if (!section) {
            continue;
        }
        if (strncmp(section, "customline", "customline"s.length()) == 0) {
            auto line = new CustomLine(inifile.GetValue(section, "name", "line"));
            line->p1.x = static_cast<float>(inifile.GetDoubleValue(section, "x1", 0.0));
            line->p1.y = static_cast<float>(inifile.GetDoubleValue(section, "y1", 0.0));
            line->p2.x = static_cast<float>(inifile.GetDoubleValue(section, "x2", 0.0));
            line->p2.y = static_cast<float>(inifile.GetDoubleValue(section, "y2", 0.0));
            line->map = static_cast<GW::Constants::MapID>(inifile.GetLongValue(section, "map", 0));
            line->color = Colors::Load(&inifile, section, "color", line->color);
            line->visible = inifile.GetBoolValue(section, "visible", true);
            line->draw_on_terrain = inifile.GetBoolValue(section, "draw_on_terrain", false);
            lines.push_back(line);
            inifile.Delete(section, nullptr);
        }
        else if (strncmp(section, "custommarker", "custommarker"s.length()) == 0) {
            auto marker = CustomMarker(inifile.GetValue(section, "name", "marker"));
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
            inifile.Delete(section, nullptr);
        }
        else if (strncmp(section, "custompolygon", "custompolygon"s.length()) == 0) {
            auto polygon = CustomPolygon(inifile.GetValue(section, "name", "polygon"));
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
            inifile.Delete(section, nullptr);
        }
    }

    marker_file_dirty = false;
    markers_changed = true;
}

void CustomRenderer::SaveSettings(ToolboxIni* ini, const char* section)
{
    Colors::Save(ini, section, "color_custom_markers", color);
    SaveMarkers();
}

void CustomRenderer::SaveMarkers()
{
    // clear markers from ini
    // then load new
    if (marker_file_dirty || GWToolbox::SettingsFolderChanged()) {
        ToolboxIni::TNamesDepend entries;
        inifile.GetAllSections(entries);
        for (const ToolboxIni::Entry& entry : entries) {
            const char* section = entry.pItem;
            if (strncmp(section, "customline", "customline"s.length()) == 0) {
                inifile.Delete(section, nullptr);
            }
            if (strncmp(section, "custommarker", "custommarker"s.length()) == 0) {
                inifile.Delete(section, nullptr);
            }
            if (strncmp(section, "custompolygon", "custompolygon"s.length()) == 0) {
                inifile.Delete(section, nullptr);
            }
        }

        // then save
        for (auto i = 0u; i < lines.size(); i++) {
            const CustomLine& line = *lines[i];
            if (line.created_by_toolbox)
                continue;
            char section[32];
            snprintf(section, 32, "customline%03d", i);
            inifile.SetValue(section, "name", line.name);
            inifile.SetDoubleValue(section, "x1", line.p1.x);
            inifile.SetDoubleValue(section, "y1", line.p1.y);
            inifile.SetDoubleValue(section, "x2", line.p2.x);
            inifile.SetDoubleValue(section, "y2", line.p2.y);
            Colors::Save(&inifile, section, "color", line.color);
            inifile.SetLongValue(section, "map", static_cast<long>(line.map));
            inifile.SetBoolValue(section, "visible", line.visible);
            inifile.SetBoolValue(section, "draw_on_terrain", line.draw_on_terrain);
        }
        for (auto i = 0u; i < markers.size(); i++) {
            const CustomMarker& marker = markers[i];
            char section[32];
            snprintf(section, 32, "custommarker%03d", i);
            inifile.SetValue(section, "name", marker.name);
            inifile.SetDoubleValue(section, "x", marker.pos.x);
            inifile.SetDoubleValue(section, "y", marker.pos.y);
            inifile.SetDoubleValue(section, "size", marker.size);
            inifile.SetLongValue(section, "shape", static_cast<long>(marker.shape));
            inifile.SetLongValue(section, "map", static_cast<long>(marker.map));
            inifile.SetBoolValue(section, "visible", marker.visible);
            inifile.SetBoolValue(section, "draw_on_terrain", marker.draw_on_terrain);
            Colors::Save(&inifile, section, "color", marker.color);
            Colors::Save(&inifile, section, "color_sub", marker.color_sub);
        }
        for (auto i = 0u; i < polygons.size(); i++) {
            const CustomPolygon& polygon = polygons[i];
            char section[32];
            snprintf(section, 32, "custompolygon%03d", i);
            for (auto j = 0u; j < polygon.points.size(); j++) {
                inifile.SetDoubleValue(
                    section, ("point["s + std::to_string(j) + "].x").c_str(), polygon.points.at(j).x);
                inifile.SetDoubleValue(
                    section, ("point["s + std::to_string(j) + "].y").c_str(), polygon.points.at(j).y);
            }
            Colors::Save(&inifile, section, "color", polygon.color);
            Colors::Save(&inifile, section, "color_sub", polygon.color_sub);
            inifile.SetValue(section, "name", polygon.name);
            inifile.SetLongValue(section, "map", static_cast<long>(polygon.map));
            inifile.SetBoolValue(section, "visible", polygon.visible);
            inifile.SetBoolValue(section, "draw_on_terrain", polygon.draw_on_terrain);
            inifile.SetBoolValue(section, "filled", polygon.filled);
        }

        ASSERT(inifile.SaveFile(Resources::GetSettingFile(ini_filename).c_str()) == SI_OK);
        marker_file_dirty = false;
    }
}

void CustomRenderer::Invalidate()
{
    VBuffer::Invalidate();
    linecircle.Invalidate();
}

void CustomRenderer::SetTooltipMapID(const GW::Constants::MapID& map_id)
{
    ImGui::SetTooltip(std::format("Map ID ({})", Resources::GetMapName(map_id)->string()).c_str());
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

CustomRenderer::CustomLine* CustomRenderer::AddCustomLine(const GW::GamePos& from, const GW::GamePos& to, const char* _name, bool draw_everywhere)
{
    const auto line = new CustomLine(from, to, GW::Map::GetMapID(), _name, draw_everywhere);
    lines.push_back(line);
    markers_changed = true;
    return line;
}

void CustomRenderer::DrawLineSettings()
{
    if (Colors::DrawSettingHueWheel("Color", &color)) {
        Invalidate();
    }
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("lines");
    for (size_t i = 0; i < lines.size(); i++) {
        CustomLine& line = *lines[i];
        if (line.created_by_toolbox)
            continue;
        ImGui::PushID(static_cast<int>(i));
        markers_changed |= ImGui::Checkbox("##visible", &line.visible);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Visible");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth((ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 5) / 5);

        markers_changed |= ImGui::DragFloat("##x1", &line.p1.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line X 1");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y1", &line.p1.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line Y 1");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##x2", &line.p2.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line X 2");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y2", &line.p2.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line Y 2");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::ColorButtonPicker("##color", &line.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line Color");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::InputInt("##map", (int*)&line.map, 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(line.map);
        }
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 2 - 20.0f * 2);
        markers_changed |= ImGui::InputText("##name", line.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Name");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &line.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Draw on in-game terrain");
        }
        ImGui::SameLine(0.0f, spacing);

        const bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete");
        }
        ImGui::PopID();
        if (remove) {
            lines.erase(lines.begin() + static_cast<int>(i));
            markers_changed = true;
        }
    }
    ImGui::PopID();
    if (ImGui::Button("Add Line")) {
        char buf[32];
        snprintf(buf, 32, "line%zu", lines.size());
        lines.push_back(new CustomLine(buf));
        markers_changed = true;
    }
}

void CustomRenderer::DrawMarkerSettings()
{
    if (Colors::DrawSettingHueWheel("Color", &color)) {
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
            ImGui::SetTooltip("Visible");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);
        marker_changed |= ImGui::DragFloat("##x", &marker.pos.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Marker X Position");
        }
        ImGui::SameLine(0.0f, spacing);
        marker_changed |= ImGui::DragFloat("##y", &marker.pos.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Marker Y Position");
        }
        ImGui::SameLine(0.0f, spacing);
        marker_changed |= ImGui::DragFloat("##size", &marker.size, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size");
        }
        ImGui::SameLine(0.0f, spacing);

        constexpr const char* types[] = {"Circle", "FillCircle"};
        marker_changed |= ImGui::Combo("##type", reinterpret_cast<int*>(&marker.shape), types, 2);
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::ColorButtonPicker("##colorsub", &marker.color_sub);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Color in which hostile agents inside this polygon are drawn.\nNOTE: An alpha channel of 0 will disable this color.");
        }
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::ColorButtonPicker("##color", &marker.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Color of the circle.\nNOTE: An alpha channel of 0 will fall back to the default color.");
        }
        ImGui::SameLine(0.0f, spacing);

        marker_changed |= ImGui::InputInt("##map", reinterpret_cast<int*>(&marker.map), 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(marker.map);
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 2 - 20.0f * 2);
        marker_changed |= ImGui::InputText("##name", marker.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Name");
        }
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &marker.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Draw on in-game terrain");
        }
        ImGui::SameLine(0.0f, spacing);

        const bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete");
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
    if (ImGui::Button("Add Marker")) {
        char buf[32];
        snprintf(buf, 32, "marker%zu", markers.size());
        markers.push_back(CustomMarker(buf));
        // invalidate in crease vector size increased and reallocated array
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
            ImGui::SetTooltip("Visible");
        }
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);

        if (show_details) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        char show_points_label_buf[40];
        snprintf(show_points_label_buf, sizeof(show_points_label_buf), "Show Points (%d)##show_polygon_details", polygon.points.size());
        if (ImGui::Button(show_points_label_buf, ImVec2(input_item_width * 3 + spacing * 2, 0.f))) {
            show_polygon_details = show_details ? -1 : signed_idx;
        }
        if (show_details) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::Checkbox("##filled", &polygon.filled);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Filled - this is only respected for a maximum of {} points!", CustomPolygon::max_points_filled);
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##colorsub", &polygon.color_sub);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Color in which hostile agents inside this polygon are drawn.\n\nNOTE: An alpha channel of 0 will disable this color.");
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##color", &polygon.color);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Color of the polygon on the map.\nNOTE: An alpha channel of 0 will disable this color.");
        }
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::InputInt("##map", reinterpret_cast<int*>(&polygon.map), 0);
        if (ImGui::IsItemHovered()) {
            SetTooltipMapID(polygon.map);
        }
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - spacing * 2 - 20.0f * 2);
        markers_changed |= ImGui::InputText("##name", polygon.name, 128);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Name");
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, spacing);
        markers_changed |= ImGui::Checkbox("##draw_on_terrain", &polygon.draw_on_terrain);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Draw on in-game terrain");
        }

        ImGui::SameLine(0.0f, spacing);
        const bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete");
        }

        if (show_details) {
            ImGui::Indent();
            if (polygon.points.size() < CustomPolygon::max_points && ImGui::Button("Add Polygon Point##add")) {
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
    if (ImGui::Button("Add Polygon")) {
        char buf[32];
        snprintf(buf, 32, "polygon%zu", polygons.size());
        polygons.emplace_back(buf);
        // invalidate in crease vector size increased and reallocated array
        for (auto& poly : polygons) {
            poly.Invalidate();
        }
        markers_changed = true;
    }
}

void CustomRenderer::DrawSettings()
{
    const auto draw_note = [] {
        ImGui::Text("Note: custom markers are stored in 'Markers.ini' in settings folder. You can share the file with other players or paste other people's markers into it.");
    };
    if (ImGui::TreeNodeEx("Custom Lines", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_lines", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 75.f + lines.size() * 25.f)});
        draw_note();
        DrawLineSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Custom Circles", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_circles", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 75.f + markers.size() * 25.f)});
        draw_note();
        DrawMarkerSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Custom Polygons", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::BeginChild("##custom_polygons", {0.f, std::min(ImGui::GetWindowSize().y * 0.7f, 50.f + polygons.size() * 25.f)});
        draw_note();
        DrawPolygonSettings();
        ImGui::EndChild();
        ImGui::TreePop();
    }
}

void CustomRenderer::Initialize(IDirect3DDevice9* device)
{
    if (!buffer) {
        initialized = false;
    }
    if (initialized) {
        return;
    }
    initialized = true;
    type = D3DPT_LINELIST;
    vertices_max = 0x100; // support for up to 256 line segments, should be enough
    vertices = nullptr;

    const HRESULT hr = device->CreateVertexBuffer(
        sizeof(D3DVertex) * vertices_max, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    if (FAILED(hr)) {
        printf("Error setting up CustomRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
}

void CustomRenderer::Terminate()
{
    VBuffer::Terminate();
    for (const auto l : lines) {
        delete l;
    }
    lines.clear();
}

void CustomRenderer::CustomPolygon::Initialize(IDirect3DDevice9* device)
{
    if (filled && points.size() < max_points_filled) {
        if (points.size() < 3) {
            return; // can't draw a triangle with less than 3 vertices
        }
        type = D3DPT_TRIANGLELIST;

        const auto poly = std::vector{{points}};
        point_indices.clear();
        point_indices = mapbox::earcut<unsigned>(poly);

        const auto vertex_count = point_indices.size();
        D3DVertex* _vertices = nullptr;

        if (buffer) {
            buffer->Release();
        }
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        for (auto i = 0u; i < point_indices.size(); i++) {
            _vertices[i].x = points.at(point_indices.at(i)).x;
            _vertices[i].y = points.at(point_indices.at(i)).y;
            _vertices[i].z = 0.f;
            _vertices[i].color = color;
        }

        buffer->Unlock();
    }
    else {
        if (points.size() < 2) {
            return;
        }
        type = D3DPT_LINESTRIP;

        const auto vertex_count = points.size() + 1;
        D3DVertex* _vertices = nullptr;

        if (buffer) {
            buffer->Release();
        }
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        for (auto i = 0u; i < points.size(); i++) {
            _vertices[i].x = points.at(i).x;
            _vertices[i].y = points.at(i).y;
            _vertices[i].z = 0.f;
            _vertices[i].color = color;
        }

        buffer->Unlock();
    }
    initialized = true;
}

void CustomRenderer::CustomPolygon::Render(IDirect3DDevice9* device)
{
    if (!initialized) {
        Initialize(device);
    }
    if (filled && points.size() < 3 || !filled && points.size() < 2) {
        return;
    }

    if (visible && (map == GW::Constants::MapID::None || map == GW::Map::GetMapID())) {
        const auto primitive_count = filled ? point_indices.size() / 3 : points.size() - 1;
        device->SetFVF(D3DFVF_CUSTOMVERTEX);
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, primitive_count);
    }
}

void CustomRenderer::CustomMarker::Initialize(IDirect3DDevice9* device)
{
    const auto colour = (color & IM_COL32_A_MASK) == 0 ? CustomRenderer::color : color;
    if (shape == Shape::FullCircle) {
        type = D3DPT_TRIANGLEFAN;
        count = 48;
        const unsigned vertex_count = count + 2;
        D3DVertex* _vertices = nullptr;

        if (buffer) {
            buffer->Release();
        }
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        _vertices[0].x = 0.0f;
        _vertices[0].y = 0.0f;
        _vertices[0].z = 0.0f;
        _vertices[0].color = Colors::Sub(colour, Colors::ARGB(50, 0, 0, 0));
        for (auto i = 1u; i < vertex_count; i++) {
            constexpr auto pi = DirectX::XM_PI;
            const float angle = (i - 1) * (2 * pi / static_cast<float>(count));
            _vertices[i].x = std::cos(angle);
            _vertices[i].y = std::sin(angle);
            _vertices[i].z = 0.0f;
            _vertices[i].color = colour;
        }

        buffer->Unlock();
    }
    else {
        type = D3DPT_LINESTRIP;
        count = 48;
        const auto vertex_count = count + 1;
        D3DVertex* _vertices = nullptr;

        if (buffer) {
            buffer->Release();
        }
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        for (auto i = 0u; i < count; i++) {
            constexpr auto pi = DirectX::XM_PI;
            const float angle = i * (2 * pi / (count + 1));
            _vertices[i].x = std::cos(angle);
            _vertices[i].y = std::sin(angle);
            _vertices[i].z = 0.0f;
            _vertices[i].color = colour;
        }
        _vertices[count] = _vertices[0];

        buffer->Unlock();
    }
    initialized = true;
}

void CustomRenderer::CustomMarker::Render(IDirect3DDevice9* device)
{
    if (!initialized) {
        Initialize(device);
    }

    if (!visible || (map != GW::Constants::MapID::None && map != GW::Map::GetMapID())) {
        return;
    }

    const auto translate = DirectX::XMMatrixTranslation(pos.x, pos.y, 0.0f);
    const auto scale = DirectX::XMMatrixScaling(size, size, 1.0f);
    const auto world = scale * translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));

    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
    device->DrawPrimitive(type, 0, count);
}

void CustomRenderer::LineCircle::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_LINESTRIP;
    count = 48; // poly count
    const auto vertex_count = count + 1;
    D3DVertex* _vertices = nullptr;

    if (buffer) {
        buffer->Release();
    }
    device->CreateVertexBuffer(
        sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

    for (size_t i = 0; i < count; i++) {
        const float angle = i * (DirectX::XM_2PI / (count + 1));
        _vertices[i].x = std::cos(angle);
        _vertices[i].y = std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = color; // 0xFF666677;
    }
    _vertices[count] = _vertices[0];

    buffer->Unlock();
}

void CustomRenderer::Render(IDirect3DDevice9* device)
{
    Initialize(device);
    if (!initialized) {
        return;
    }

    if (markers_changed) {
        GameWorldRenderer::TriggerSyncAllMarkers();
        marker_file_dirty = true;
        markers_changed = false;
        Invalidate();
        return;
    }

    DrawCustomMarkers(device);

    vertices_count = 0;
    if (const HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, reinterpret_cast<void**>(&vertices), D3DLOCK_DISCARD); FAILED(res)) {
        printf("CustomRenderer Lock() error: HRESULT: 0x%lX\n", res);
    }

    DrawCustomLines(device);

    const auto xmi = DirectX::XMMatrixIdentity();
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&xmi));

    buffer->Unlock();
    if (vertices_count != 0) {
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, vertices_count / 2);
        vertices_count = 0;
    }
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

    if (GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags; flags.valid()) {
        for (const auto& flag : flags) {
            const auto translate = DirectX::XMMatrixTranslation(flag.flag.x, flag.flag.y, 0.0f);
            const auto scale = DirectX::XMMatrixScaling(200.0f, 200.0f, 1.0f);
            const auto world = scale * translate;
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
            linecircle.Render(device);
        }
    }
    const GW::Vec3f allflag = GW::GetGameContext()->world->all_flag;
    const auto translate = DirectX::XMMatrixTranslation(allflag.x, allflag.y, 0.0f);
    const auto scale = DirectX::XMMatrixScaling(300.0f, 300.0f, 1.0f);
    const auto world = scale * translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
    linecircle.Render(device);
}

void CustomRenderer::DrawCustomLines(const IDirect3DDevice9*)
{
    const auto doa_outpost = GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable && GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish;

    for (const auto line : lines) {
        // Draw everywhere besides the DoA outpost. Only draw the lines with draw_everywhere in DoA
        if (line->visible && line->draw_on_minimap && (line->map == GW::Constants::MapID::None || line->map == GW::Map::GetMapID()) &&
            (!doa_outpost || line->draw_everywhere)) {
            EnqueueVertex(line->p1.x, line->p1.y, line->color);
            EnqueueVertex(line->p2.x, line->p2.y, line->color);
        }
    }
}

void CustomRenderer::EnqueueVertex(const float x, const float y, const Color _color)
{
    if (vertices_count == vertices_max) {
        return;
    }
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].color = _color;
    ++vertices;
    ++vertices_count;
}
