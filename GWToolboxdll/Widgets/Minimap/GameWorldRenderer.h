#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <ToolboxIni.h>
#include <Widgets/Minimap/D3DVertex.h>

class GenericPolyRenderable {
public:
    GenericPolyRenderable(IDirect3DDevice9* device, GW::Constants::MapID map_id, const std::vector<GW::Vec2f>& points, unsigned int col, bool filled);
    ~GenericPolyRenderable();
    void Draw(IDirect3DDevice9* device);
    bool IsInRange(const GW::GamePos& pos, float dist_sq) const;

    GW::Constants::MapID map_id{};

private:
    IDirect3DVertexBuffer9* vb = nullptr;
    unsigned int col = 0;
    std::vector<GW::Vec2f> points;
    std::vector<D3DVertex> vertices;
    bool altitudes_computed = true;
    bool filled = false;
};

class GameWorldRenderer {
public:
    void Render(IDirect3DDevice9* device);
    static void LoadSettings(const ToolboxIni* ini, const char* section);
    static void SaveSettings(ToolboxIni* ini, const char* section);
    static void DrawSettings();
    void Terminate();
    static void TriggerSyncAllMarkers();

private:
    std::vector<std::unique_ptr<GenericPolyRenderable>> renderables;
    void SyncLines(IDirect3DDevice9* device);
    void SyncPolys(IDirect3DDevice9* device);
    void SyncMarkers(IDirect3DDevice9* device);
    void SyncAllMarkers(IDirect3DDevice9* device);
    static void SetD3DTransform(IDirect3DDevice9* device);
};
