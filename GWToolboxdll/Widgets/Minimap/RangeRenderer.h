#pragma once

#include <Color.h>
#include <D3DContainers.h>

#include <vector>

class SettingsDoc;
class ToolboxModule;

class RangeRenderer : public D3DVertexBuffer {
    static constexpr size_t circle_points = 192;
    static constexpr size_t circle_triangles = circle_points - 2;

public:
    struct RangeCircle {
        std::string label;
        float radius = 500.f;
        float line_thickness = 1.f;
        Colors::SettingColor color = 0xFF888888;
        bool visible = true;
        bool on_target = false; // false = player origin, true = current target origin
    };

    void Render(IDirect3DDevice9* device) override { Render(device, gwinches_per_pixel); }
    void Render(IDirect3DDevice9* device, float _gwinches_per_pixel);
    void SetDrawCenter(const bool b) { draw_center_ = b; }

    void DrawSettings();
    void RegisterSettings(ToolboxModule* module);
    void LoadSettings(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section);
    void SaveSettings(SettingsDoc& doc, const char* section) const;
    void LoadDefaults();

private:
    void Initialize(IDirect3DDevice9* device) override;
    void LoadDefaultCircles();

    bool HaveHos();

    bool checkforhos_ = true;
    bool havehos_ = false;

    bool draw_center_ = false;

    // This gets set at runtime when Render() is called
    float gwinches_per_pixel = 1.f;

    // User-configurable range circles
    std::vector<RangeCircle> circles_;

    // Special-case circles with conditional rendering logic (not user-configurable)
    Color color_range_hos = 0xFF881188;
    Color color_range_chain_aggro = 0x00994444;
    Color color_range_res_aggro = 0x64D6D6D6;
    Color color_range_shadowstep_aggro = Colors::ARGB(200, 128, 0, 128);
};
