#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <D3DContainers.h>

namespace GW::Constants {
    enum class MapID : uint32_t;
}

using Color = uint32_t;

class ToolboxModule;

class CustomRenderer : public D3DVertexBuffer {
    friend class AgentRenderer;
    friend class GameWorldRenderer;

    enum class Shape {
        LineCircle,
        FullCircle
    };

    // glaze-serialized mirror of the persisted marker data (Markers.json); field set matches the legacy ini
    struct MarkersFile {
        struct Line {
            std::string name = "line";
            float x1 = 0.f;
            float y1 = 0.f;
            float x2 = 0.f;
            float y2 = 0.f;
            Colors::SettingColor color = 0xFFFFFFFF;
            uint32_t map = 0;
            bool visible = true;
            bool draw_on_terrain = false;
        };

        struct Marker {
            std::string name = "marker";
            float x = 0.f;
            float y = 0.f;
            float size = 0.f;
            int shape = 0;
            uint32_t map = 0;
            bool visible = true;
            bool draw_on_terrain = false;
            Colors::SettingColor color = 0x00FFFFFF;
            Colors::SettingColor color_sub = 0x00FFFFFF;
        };

        struct Point {
            float x = 0.f;
            float y = 0.f;
        };

        struct Polygon {
            std::string name = "polygon";
            std::vector<Point> points{};
            Colors::SettingColor color = 0xA0FFFFFF;
            Colors::SettingColor color_sub = 0x00FFFFFF;
            uint32_t map = 0;
            bool visible = true;
            bool draw_on_terrain = false;
            bool filled = false;
        };

        std::vector<Line> lines{};
        std::vector<Marker> markers{};
        std::vector<Polygon> polygons{};
    };

    struct CustomMarker final {
        CustomMarker(float x, float y, float s, Shape sh, GW::Constants::MapID m, const char* _name);
        explicit CustomMarker(const char* name);
        ~CustomMarker() { Terminate(); }

        GW::GamePos pos;
        float size = 1.f;
        Shape shape = Shape::LineCircle;
        GW::Constants::MapID map = (GW::Constants::MapID)0;
        bool visible = true;
        bool draw_on_terrain = false;
        char name[128]{};
        Color color{0x00FFFFFF};
        Color color_sub{0x00FFFFFF};

        void Invalidate();
        void Terminate();
        void Render(IDirect3DDevice9* device);
        [[nodiscard]] bool IsFilled() const { return shape == Shape::FullCircle; }

    private:
        void SyncGeometry();

        D3DFillCircle fill_circle{{}, 1.f};
        D3DLineCircle line_circle;
    };

struct CustomPolygon final : D3DVertexBuffer {
        CustomPolygon(GW::Constants::MapID m, const char* n);
        explicit CustomPolygon(const char* name);
        ~CustomPolygon() { Terminate(); }
        std::vector<GW::GamePos> points{};
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
    };

public:
    struct CustomLine {
        CustomLine(float x1, float y1, float x2, float y2, GW::Constants::MapID m, const char* n = nullptr, bool draw_everywhere = false);
        CustomLine(GW::GamePos p1, GW::GamePos p2, GW::Constants::MapID m, const char* n = nullptr, bool draw_everywhere = false);

        explicit CustomLine(const char* n)
            : CustomLine(0, 0, 0, 0, static_cast<GW::Constants::MapID>(0), n) { }
        GW::GamePos p1{};
        GW::GamePos p2{};
        GW::Constants::MapID map{};
        Color color{0xFFFFFFFF};
        bool visible = true;
        bool draw_on_terrain = false;
        bool draw_on_minimap = true;
        bool draw_on_mission_map = true;
        bool draw_everywhere = false;
        bool created_by_toolbox = false;
        bool from_player_pos = false;
        bool dotted = true; // toolbox terrain lines dash by default; set false for a solid line
        // p1/p2 hold world-map coords, not game coords; only the world map renders these
        // (cross-map route tails, whose game positions belong to other maps).
        bool world_coords = false;
        char name[128]{};
    };

    void Render(IDirect3DDevice9* device) override;
    void Invalidate() override;
    void Terminate() override;
    void DrawSettings();
    void DrawLineSettings();
    void DrawMarkerSettings();
    void DrawPolygonSettings();
    void RegisterSettings(ToolboxModule* module);
    void LoadMarkers();
    void SaveMarkers();
    CustomLine* AddCustomLine(const GW::GamePos& from, const GW::GamePos& to, const char* _name = nullptr, bool draw_everywhere = false);
    bool RemoveCustomLine(CustomLine* line);
    // Bulk removal in a single O(N) pass; removing lines one-by-one is O(N^2) (linear find + vector erase each).
    void RemoveCustomLines(const std::vector<CustomLine*>& lines_to_remove);

    [[nodiscard]] const std::vector<CustomLine*>& GetLines() const { return lines; }
    [[nodiscard]] const std::vector<CustomPolygon>& GetPolys() const { return polygons; }
    [[nodiscard]] const std::vector<CustomMarker>& GetMarkers() const { return markers; }

    void Render(IDirect3DDevice9* device, float gwinches_per_pixel);

private:
    void Initialize(IDirect3DDevice9* device) override;

    void DrawCustomMarkers(IDirect3DDevice9* device);
    void DrawCustomLines(const IDirect3DDevice9* device);
    void SetTooltipMapID(const GW::Constants::MapID& map_id);

    struct MapTooltip {
        GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0);
        std::wstring map_name_ws;
        char tooltip_str[128]{};
    } map_id_tooltip;

    // Triangle-strip circle buffer for hero flag indicators
    struct HeroCircles : D3DVertexBuffer {
        static constexpr size_t circle_points = 192;
        static constexpr size_t circle_triangles = circle_points - 2;
        DWORD color = 0xFF666677;
        float thickness = 1.f;
        float gwinches_per_pixel = 1.f;
        void Update(DWORD c, float t, float gpp);
        void RenderAt(IDirect3DDevice9* device, float x, float y, bool is_allflag);
    private:
        void Initialize(IDirect3DDevice9* device) override;
    };
    HeroCircles hero_circles_{};
    Color color_hero_flags_{0xFF666677};
    float hero_flag_line_thickness_ = 1.f;
    float gwinches_per_pixel_ = 1.f;

    inline static Color color{0xFFFFFFFF};

    int show_polygon_details = -1;
    bool markers_changed = false;
    bool marker_file_dirty = true;
    bool markers_loaded = false; // guards SaveMarkers against clobbering a file that was never read
    std::vector<CustomLine*> lines{};
    std::vector<CustomMarker> markers{};
    std::vector<CustomPolygon> polygons{};
};
