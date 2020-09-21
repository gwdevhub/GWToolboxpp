#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>

#include <Color.h>
#include <Widgets/Minimap/VBuffer.h>

namespace mapbox // enable mapbox::earcut to work with GW::Vec2f as Point
{
    namespace util
    {
        template <>
        struct nth<0, GW::Vec2f>
        {
            inline static auto get(const GW::Vec2f& t) { return t.x; };
        };
        template <>
        struct nth<1, GW::Vec2f>
        {
            inline static auto get(const GW::Vec2f& t) { return t.y; };
        };
    }
}

class CustomRenderer : public VBuffer
{
    friend class AgentRenderer;
    struct CustomLine
    {
        CustomLine(float x1, float y1, float x2, float y2, GW::Constants::MapID m, const char* n)
            : p1(x1, y1)
            , p2(x2, y2)
            , map(m)
            , visible(true)
        {
            if (n)
                GuiUtils::StrCopy(name, n, sizeof(name));
            else
                GuiUtils::StrCopy(name, "line", sizeof(name));
        };
        CustomLine(const char* n)
            : CustomLine(0, 0, 0, 0, GW::Constants::MapID::None, n){};
        GW::Vec2f p1;
        GW::Vec2f p2;
        GW::Constants::MapID map;
        bool visible;
        char name[128]{};
    };
    enum class Shape
    {
        LineCircle,
        FullCircle
    };
    struct CustomMarker
    {
        CustomMarker(float x, float y, float s, Shape sh, GW::Constants::MapID m, const char* n)
            : pos(x, y)
            , size(s)
            , shape(sh)
            , map(m)
            , visible(true)
        {
            if (n)
                GuiUtils::StrCopy(name, n, sizeof(name));
            else
                GuiUtils::StrCopy(name, "marker", sizeof(name));
        };
        CustomMarker(const char* n)
            : CustomMarker(0, 0, 100.0f, Shape::LineCircle, GW::Constants::MapID::None, n){};
        GW::Vec2f pos;
        float size;
        Shape shape;
        GW::Constants::MapID map;
        bool visible;
        char name[128]{};
        Color color{0xFFFFFFA0};
        Color color_sub{0x00FFFFFF};
    };

    struct CustomPolygon final : VBuffer
    {
        CustomPolygon(GW::Constants::MapID m, const char* n)
            : map(m)
        {
            if (n)
                GuiUtils::StrCopy(name, n, sizeof name);
            else
                GuiUtils::StrCopy(name, "marker", sizeof name);
        };
        CustomPolygon(const char* n)
            : CustomPolygon(GW::Constants::MapID::None, n){};

        std::vector<GW::Vec2f> points{};
        GW::Constants::MapID map;
        bool visible = true;
        bool filled = false;
        char name[128]{};
        Color color{0xA0FFFFFF};
        Color color_sub{0x00FFFFFF};
        const static auto max_points = 21;
        void Initialize(IDirect3DDevice9* device) override;
        void Render(IDirect3DDevice9* device) override;

    private:
        std::vector<unsigned> point_indices{};
    };

public:
    void Render(IDirect3DDevice9* device) override;
    void Invalidate() override;
    void DrawSettings();
    void LoadSettings(CSimpleIni* ini, const char* section);
    void SaveSettings(CSimpleIni* ini, const char* section) const;
    void LoadMarkers();
    void SaveMarkers() const;

private:
    void Initialize(IDirect3DDevice9* device) override;
    void DrawCustomMarkers(IDirect3DDevice9* device);
    void DrawCustomLines(IDirect3DDevice9* device);
    void EnqueueVertex(float x, float y, Color color);

    class FullCircle : public VBuffer
    {
        void Initialize(IDirect3DDevice9* device) override;
    } fullcircle;
    class LineCircle : public VBuffer
    {
        void Initialize(IDirect3DDevice9* device) override;
    } linecircle;

    inline static Color color{0xFF00FFFF};

    D3DVertex* vertices = nullptr;
    unsigned int vertices_count = 0;
    unsigned int vertices_max = 0;

    bool markers_changed = false;
    std::vector<CustomLine> lines;
    std::vector<CustomMarker> markers;
    std::vector<CustomPolygon> polygons{};

    CSimpleIni* inifile = nullptr;
};
