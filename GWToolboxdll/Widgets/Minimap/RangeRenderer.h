#pragma once

#include <Color.h>
#include <Widgets/Minimap/VBuffer.h>

class RangeRenderer : public VBuffer {
    static constexpr size_t num_circles = 8;
    static constexpr size_t circle_points = 192;
    static constexpr size_t circle_triangles = circle_points - 2;

public:
    void Render(IDirect3DDevice9* device) override;
    void SetDrawCenter(bool b) { draw_center_ = b; }

    void DrawSettings();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;
    void LoadDefaults();
    // Returns number of vertices used.
    size_t CreateCircle(D3DVertex *vertices, float radius, DWORD color) const;

private:
    void Initialize(IDirect3DDevice9* device) override;

    bool HaveHos();

    bool checkforhos_ = true;
    bool havehos_ = false;

    bool draw_center_ = false;

    float line_thickness = 1.f;

    Color color_range_chain_aggro = 0;
    Color color_range_res_aggro = 0;

    Color color_range_hos = 0;
    Color color_range_aggro = 0;
    Color color_range_cast = 0;
    Color color_range_spirit = 0;
    Color color_range_compass = 0;
    Color color_range_shadowstep_aggro = 0;
};
