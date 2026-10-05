#pragma once

#include <D3DContainers.h>
#include <Windows/AgentAppearanceWindow.h>

class AgentRenderer : public D3DVertexBuffer {
public:
    AgentRenderer();
    void Terminate() override;
    void Render(IDirect3DDevice9* device) override;

private:
    using Shape_e = AgentAppearanceWindow::Shape_e;
    static constexpr auto Shape_None = AgentAppearanceWindow::Shape_None;
    static constexpr auto Tear = AgentAppearanceWindow::Tear;
    static constexpr auto Circle = AgentAppearanceWindow::Circle;
    static constexpr auto Quad = AgentAppearanceWindow::Quad;
    static constexpr auto BigCircle = AgentAppearanceWindow::BigCircle;
    static constexpr auto Star = AgentAppearanceWindow::Star;
    static constexpr int num_triangles = 32;
    static constexpr size_t shape_size = 5;

    enum Color_Modifier { None, Dark, Light, CircleCenter };

    struct Shape_Vertex : GW::Vec2f {
        Shape_Vertex(float x, float y, Color_Modifier mod)
            : Vec2f(x, y), modifier(mod) {}
        Color_Modifier modifier;
    };

    struct Shape_t {
        std::vector<Shape_Vertex> vertices;
        void AddVertex(float x, float y, Color_Modifier mod);
    };

    struct RenderPosition {
        float rotation_cos;
        float rotation_sin;
        GW::Vec2f position;
    };

    Shape_t shapes[shape_size];
    DWORD last_check = 0;
    bool target_drawn = false;

    void Initialize(IDirect3DDevice9* device) override;
    void Enqueue(const GW::Agent* agent);
    void Enqueue(Shape_e shape, const GW::Agent* agent, float size, Color color, Color border_color = 0, float border_thickness = 0.f);
    void Enqueue(Shape_e shape, const GW::MapProp* agent, float size, Color color);
    void Enqueue(Shape_e shape, const RenderPosition& pos, float size, Color color, Color modifier = 0);
};
