#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <Widgets/Minimap/VBuffer.h>

namespace GW::Constants {
    enum class MapID;
}

using Color = uint32_t;

namespace mapbox::util {
    template <>
    struct nth<0, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.y; }
    };
}

class CustomRenderer : public VBuffer {
    friend class AgentRenderer;

    struct CustomLine {
        CustomLine(float x1, float y1, float x2, float y2, GW::Constants::MapID m, const char* n);

        explicit CustomLine(const char* n)
            : CustomLine(0, 0, 0, 0, static_cast<GW::Constants::MapID>(0), n) { }
        GW::Vec2f p1{};
        GW::Vec2f p2{};
        GW::Constants::MapID map{};
        Color color{0xFFFFFFFF};
        bool visible = true;
        bool draw_on_terrain = false;
        char name[128]{};
    };

    enum class Shape {
        LineCircle,
        FullCircle
    };

    struct CustomMarker final : VBuffer {
        CustomMarker(float x, float y, float s, Shape sh, GW::Constants::MapID m, const char* _name);
        explicit CustomMarker(const char* name);
        GW::Vec2f pos;
        float size;
        Shape shape;
        GW::Constants::MapID map;
        bool visible = true;
        bool draw_on_terrain = false;
        char name[128]{};
        Color color{0x00FFFFFF};
        Color color_sub{0x00FFFFFF};
        void Render(IDirect3DDevice9* device) override;
        [[nodiscard]] bool IsFilled() const { return shape == Shape::FullCircle; }

    private:
        void Initialize(IDirect3DDevice9* device) override;
    };

    struct CustomPolygon final : VBuffer {
        CustomPolygon(GW::Constants::MapID m, const char* n);
        explicit CustomPolygon(const char* name);

        std::vector<GW::Vec2f> points{};
        GW::Constants::MapID map;
        bool visible = true;
        bool draw_on_terrain = false;
        Shape shape{};
        bool filled = false;
        char name[128]{};
        Color color{0xA0FFFFFF};
        Color color_sub{0x00FFFFFF};
        constexpr static auto max_points = 1800;
        constexpr static auto max_points_filled = 21;
        void Render(IDirect3DDevice9* device) override;

    private:
        void Initialize(IDirect3DDevice9* device) override;
        std::vector<unsigned> point_indices{};
    };

public:
    void Render(IDirect3DDevice9* device) override;
    void Invalidate() override;
    void DrawSettings();
    void DrawLineSettings();
    void DrawMarkerSettings();
    void DrawPolygonSettings();
    void LoadSettings(const ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section);
    void LoadMarkers();
    void SaveMarkers();

    [[nodiscard]] const std::vector<CustomLine>& GetLines() const { return lines; }
    [[nodiscard]] const std::vector<CustomPolygon>& GetPolys() const { return polygons; }
    [[nodiscard]] const std::vector<CustomMarker>& GetMarkers() const { return markers; }

private:
    void Initialize(IDirect3DDevice9* device) override;
    void DrawCustomMarkers(IDirect3DDevice9* device);
    void DrawCustomLines(const IDirect3DDevice9* device);
    void EnqueueVertex(float x, float y, Color color);
    void SetTooltipMapID(const GW::Constants::MapID& map_id);

    struct MapTooltip {
        GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0);
        std::wstring map_name_ws;
        char tooltip_str[128]{};
    } map_id_tooltip;

    class LineCircle : public VBuffer {
        void Initialize(IDirect3DDevice9* device) override;
    } linecircle;

    inline static Color color{0xFF00FFFF};

    D3DVertex* vertices = nullptr;
    unsigned int vertices_count = 0;
    unsigned int vertices_max = 0;

    int show_polygon_details = -1;
    bool markers_changed = false;
    bool marker_file_dirty = true;
    std::vector<CustomLine> lines{};
    std::vector<CustomMarker> markers{};
    std::vector<CustomPolygon> polygons{};
};
