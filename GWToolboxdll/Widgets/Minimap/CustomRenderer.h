#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <Widgets/Minimap/VBuffer.h>

namespace GW {
    namespace Constants {
        enum class MapID;
    }
}
typedef uint32_t Color;
namespace mapbox // enable mapbox::earcut to work with GW::Vec2f as Point
{
    namespace util
    {
        template <>
        struct nth<0, GW::Vec2f>
        {
            static auto get(const GW::Vec2f& t) { return t.x; };
        };
        template <>
        struct nth<1, GW::Vec2f>
        {
            static auto get(const GW::Vec2f& t) { return t.y; };
        };
    }
}

class CustomRenderer : public VBuffer
{
    friend class AgentRenderer;
    struct CustomLine
    {
        CustomLine(float x1, float y1, float x2, float y2, GW::Constants::MapID m, const char* n);
        CustomLine(const char* n)
            : CustomLine(0, 0, 0, 0, static_cast<GW::Constants::MapID>(0), n){};
        GW::Vec2f p1;
        GW::Vec2f p2;
        GW::Constants::MapID map;
        Color color{ 0xFFFFFFFF };
        bool visible;
        char name[128]{};
    };
    enum class Shape
    {
        LineCircle,
        FullCircle
    };
    struct CustomMarker final : VBuffer
    {
        CustomMarker(float x, float y, float s, Shape sh, GW::Constants::MapID m, const char* n);
        explicit CustomMarker(const char* n);
        GW::Vec2f pos;
        float size;
        Shape shape;
        GW::Constants::MapID map;
        bool visible;
        char name[128]{};
        Color color{0x00FFFFFF};
        Color color_sub{0x00FFFFFF};
        void Render(IDirect3DDevice9* device) override;

    private:
        void Initialize(IDirect3DDevice9* device) override;
    };

    struct CustomPolygon final : VBuffer
    {
        CustomPolygon(GW::Constants::MapID m, const char* n);
        CustomPolygon(const char* n);

        std::vector<GW::Vec2f> points{};
        GW::Constants::MapID map;
        bool visible = true;
        Shape shape;
        bool filled = false;
        char name[128]{};
        Color color{0xA0FFFFFF};
        Color color_sub{0x00FFFFFF};
        constexpr static auto max_points = 21;
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
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;
    void LoadMarkers();
    void SaveMarkers() const;

private:
    void Initialize(IDirect3DDevice9* device) override;
    void DrawCustomMarkers(IDirect3DDevice9* device);
    void DrawCustomLines(IDirect3DDevice9* device);
    void EnqueueVertex(float x, float y, Color color);
    void SetTooltipMapID(const GW::Constants::MapID& map_id);
    struct MapTooltip {
        GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0);
        std::wstring map_name_ws;
        char tooltip_str[128]{};
    } map_id_tooltip;

    class LineCircle : public VBuffer
    {
        void Initialize(IDirect3DDevice9* device) override;
    } linecircle;

    inline static Color color{0xFF00FFFF};

    D3DVertex* vertices = nullptr;
    unsigned int vertices_count = 0;
    unsigned int vertices_max = 0;

    int show_polygon_details = -1;
    bool markers_changed = false;
    std::vector<CustomLine> lines;
    std::vector<CustomMarker> markers;
    std::vector<CustomPolygon> polygons{};

    ToolboxIni* inifile = nullptr;

    bool initialized = false;
};
