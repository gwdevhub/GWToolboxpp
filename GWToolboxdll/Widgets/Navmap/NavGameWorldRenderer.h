#pragma once

#include <ToolboxIni.h>
#include <GWCA/Managers/RenderMgr.h>

#include <vector>
#include <d3d9.h>

#include "Navigation/Navigation.h"

class NavGameWorldRenderer {
    friend class DebugDrawDX9;
public:
    void Render(IDirect3DDevice9* device, Navmesh * nm, const NavmeshBuildSettings& nbs);
    void LoadSettings(const ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section);
    void DrawSettings();

private:
    bool ConfigureProgrammablePipeline(IDirect3DDevice9* device);
    bool SetD3DTransform(IDirect3DDevice9* device, const NavmeshBuildSettings& nbs);

//public:

    bool need_configure_pipeline = true;
    
    IDirect3DVertexShader9* vshader = nullptr;
    IDirect3DPixelShader9* pshader = nullptr;
    IDirect3DVertexDeclaration9* vertex_declaration = nullptr;
    IDirect3DDevice9 * dx9_device = nullptr;

    bool render_navmesh = false;
};
