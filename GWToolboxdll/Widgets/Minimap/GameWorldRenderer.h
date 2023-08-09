#pragma once
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <ToolboxIni.h>
#include <Widgets/Minimap/CustomRenderer.h> // for Vec2f earcut
#include <Widgets/Minimap/D3DVertex.h>
#include <d3d9.h>
#include <memory>
#include <vector>

class GenericPolyRenderable {
private:
    IDirect3DVertexBuffer9* vb;
    unsigned int col;
    std::vector<GW::Vec2f> points;
    std::vector<D3DVertex> vertices;
    bool altitudes_computed;
    bool filled;

public:
    GenericPolyRenderable(IDirect3DDevice9* device, const GW::Constants::MapID map_id, const std::vector<GW::Vec2f>& points, unsigned int col, bool filled);
    ~GenericPolyRenderable();
    void draw(IDirect3DDevice9* device);
    bool is_in_range(const GW::GamePos& pos, const float dist_sq) const;

    const GW::Constants::MapID map_id;
};

class GameWorldRenderer {
public:
    void Render(IDirect3DDevice9* device);
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;
    void DrawSettings();
    void Terminate();
    void TriggerSyncAllMarkers();

private:
    std::vector<std::unique_ptr<GenericPolyRenderable>> renderables;
    void sync_lines(IDirect3DDevice9* device);
    void sync_polys(IDirect3DDevice9* device);
    void sync_markers(IDirect3DDevice9* device);
    void sync_all_markers(IDirect3DDevice9* device);
    bool need_sync_markers = true;
    void set_3d_transforms(IDirect3DDevice9* device);

    // settings
    int render_max_dist_enum = 1;
    float render_max_dist_sq = 0.0f;
};
