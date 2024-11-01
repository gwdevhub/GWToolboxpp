#pragma once

#include <Color.h>
#include <Widgets/Minimap/VBuffer.h>
#include "Navigation/Navigation.h"

struct NavmapUIvertex
{
    float x, y, z;
};

constexpr auto D3DFVF_NAVMAP_UI_VERTEX = D3DFVF_XYZ;

class NavMarkerRenderer : public VBuffer {
public:
    void Render(IDirect3DDevice9* device) override;

    void DrawSettings();
    void LoadSettings(const ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;

protected:
    void Initialize(IDirect3DDevice9* device) override;

private:
    bool ConfigureProgrammablePipeline(IDirect3DDevice9* device);
    bool SetD3DTransform(IDirect3DDevice9* device, const NavmeshBuildSettings& _nbs);

    Color color_portal = 0, color_teleporter = 0, color_primary_objective = 0, color_secondary_objective = 0;

    int segments, vertexCount;
    float angleIncrement;

    IDirect3DVertexShader9* vshader = nullptr;
    IDirect3DPixelShader9* pshader = nullptr;
    IDirect3DVertexDeclaration9* vertex_declaration = nullptr;

    bool need_configure_pipeline = true;

    NavmeshBuildSettings nbs;
};
