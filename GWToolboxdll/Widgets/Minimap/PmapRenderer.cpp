#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/MapMgr.h>

#include <D3DContainers.h>
#include <Widgets/Minimap/PmapRenderer.h>

#include <ImGuiAddons.h>

#include "Minimap.h"

namespace {
    void SetDeviceColor(IDirect3DDevice9* device, D3DCOLOR color)
    {
        device->SetRenderState(D3DRS_TEXTUREFACTOR, color);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    }

    void ResetDeviceColor(IDirect3DDevice9* device)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }

    void SetDeviceTranslation(IDirect3DDevice9* device, float x, float y, float z, D3DMATRIX* old_view = nullptr)
    {
        D3DMATRIX current;
        device->GetTransform(D3DTS_VIEW, &current);
        if (old_view) *old_view = current;

        const auto matrix = XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&current));
        const auto translated = matrix * DirectX::XMMatrixTranslation(x, y, z);
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&translated));
    }

    void ResetDeviceTranslation(IDirect3DDevice9* device, const D3DMATRIX& old_view)
    {
        device->SetTransform(D3DTS_VIEW, &old_view);
    }
}


void PmapRenderer::DrawSettings() {}

void PmapRenderer::Initialize(IDirect3DDevice9* device)
{
    clear();
    if (!GW::Map::GetIsMapLoaded()) {
        initialized = false;
        return;
    }

    GW::PathingMapArray* path_map = GW::Map::GetPathingMap();

    size_t trapez_count = 0;
    for (const GW::PathingMap& map : *path_map) {
        trapez_count += map.trapezoid_count;
    }
    if (!trapez_count) return;

#ifdef WIREFRAME_MODE
    type = D3DPT_LINELIST;
    vertices.reserve(trapez_count * 8);
    for (const GW::PathingMap& pmap : *path_map) {
        for (size_t j = 0; j < pmap.trapezoid_count; j++) {
            const GW::PathingTrapezoid& trap = pmap.trapezoids[j];
            vertices.push_back({trap.XTL, trap.YT, 0.f, 0});
            vertices.push_back({trap.XTR, trap.YT, 0.f, 0});
            vertices.push_back({trap.XTR, trap.YT, 0.f, 0});
            vertices.push_back({trap.XBR, trap.YB, 0.f, 0});
            vertices.push_back({trap.XBR, trap.YB, 0.f, 0});
            vertices.push_back({trap.XBL, trap.YB, 0.f, 0});
            vertices.push_back({trap.XBL, trap.YB, 0.f, 0});
            vertices.push_back({trap.XTL, trap.YT, 0.f, 0});
        }
    }
#else
    type = D3DPT_TRIANGLELIST;
    vertices.reserve(trapez_count * 6);
    for (const GW::PathingMap& pmap : *path_map) {
        for (size_t j = 0; j < pmap.trapezoid_count; j++) {
            const GW::PathingTrapezoid& trap = pmap.trapezoids[j];
            vertices.push_back({trap.XTL, trap.YT, 0.f, 0});
            vertices.push_back({trap.XTR, trap.YT, 0.f, 0});
            vertices.push_back({trap.XBL, trap.YB, 0.f, 0});
            vertices.push_back({trap.XBL, trap.YB, 0.f, 0});
            vertices.push_back({trap.XTR, trap.YT, 0.f, 0});
            vertices.push_back({trap.XBR, trap.YB, 0.f, 0});
        }
    }
#endif

    D3DVertexBuffer::Initialize(device);
}

void PmapRenderer::Render(IDirect3DDevice9* device, const MinimapRenderContext& ctx)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }

    if (ctx.shadow_color & IM_COL32_A_MASK) {
        D3DMATRIX oldview;
        SetDeviceTranslation(device, 0, -100.f, 0.f, &oldview);

        SetDeviceColor(device, ctx.shadow_color);
        D3DVertexBuffer::Render(device);
        ResetDeviceTranslation(device, oldview);
        ResetDeviceColor(device);
    }
    if (ctx.foreground_color & IM_COL32_A_MASK) {
        SetDeviceColor(device, ctx.foreground_color);
        D3DVertexBuffer::Render(device);
        ResetDeviceColor(device);
    }
}
