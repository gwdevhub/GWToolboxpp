#include "GameWorldRenderer.h"
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <Widgets/Minimap/Minimap.h>

namespace {
    int lerp_steps_per_line = 10;
    GW::Vec2f lerp(const GW::Vec2f& A, const GW::Vec2f& B, float t) { return A * t + B * (1.f - t); }
    constexpr float ALTITUDE_UNKNOWN = std::numeric_limits<float>::max();

    std::vector<GW::Vec2f> circular_points_from_marker(float pos_x, float pos_y, float size)
    {
        std::vector<GW::Vec2f> points;
        static constexpr float pi = DirectX::XM_PI;
        static constexpr std::size_t num_points_per_circle = 48;
        float slice = 2.0f * pi / static_cast<float>(num_points_per_circle);
        for (std::size_t i = 0; i < num_points_per_circle; i++) {
            float angle = slice * i;
            points.push_back(GW::Vec2f{pos_x + size * std::cos(angle), pos_y + size * std::sin(angle)});
        }
        points.push_back(points.at(0)); // to complete the LINELIST
        return points;
    }
} // namespace

GenericPolyRenderable::GenericPolyRenderable(IDirect3DDevice9* device, GW::Constants::MapID map_id, const std::vector<GW::Vec2f>& points, unsigned int col, bool filled)
    : vb(nullptr)
    , map_id(map_id)
    , col(col)
    , altitudes_computed(false)
    , filled(filled)
    , points(points)
{
    if (filled && points.size() >= 3) {
        // (filling doesn't make sense if there is not at least enough points for one triangle)
        std::vector<GW::Vec2f> lerp_points;
        for (std::size_t i = 0; i < points.size(); i++) {
            const GW::Vec2f& pt = points.at(i);
            if (!lerp_points.empty() && lerp_steps_per_line > 0) {
                for (int j = 1; j < lerp_steps_per_line; j++) {
                    float div = static_cast<float>(j) / static_cast<float>(lerp_steps_per_line);
                    GW::Vec2f split = lerp(points[i], points[i - 1], div);
                    lerp_points.push_back(split);
                }
            }
            lerp_points.push_back(pt);
        }
        const auto poly = std::vector<std::vector<GW::Vec2f>>{{lerp_points}};
        std::vector<unsigned> indices = mapbox::earcut<unsigned>(poly);
        for (std::size_t i = 0; i < indices.size(); i++) {
            const auto& pt = lerp_points.at(indices.at(i));
            vertices.push_back(D3DVertex{pt.x, pt.y, ALTITUDE_UNKNOWN, col});
        }
    }
    else {
        for (std::size_t i = 0; i < points.size(); i++) {
            const GW::Vec2f& pt = points.at(i);
            if (!vertices.empty() && lerp_steps_per_line > 0) {
                for (int j = 1; j < lerp_steps_per_line; j++) {
                    float div = static_cast<float>(j) / static_cast<float>(lerp_steps_per_line);
                    GW::Vec2f split = lerp(points[i], points[i - 1], div);
                    vertices.push_back(D3DVertex{split.x, split.y, ALTITUDE_UNKNOWN, col});
                }
            }
            vertices.push_back(D3DVertex{pt.x, pt.y, ALTITUDE_UNKNOWN, col});
        }
    }

    device->CreateVertexBuffer(vertices.size() * sizeof(D3DVertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &vb, NULL);
}

GenericPolyRenderable::~GenericPolyRenderable()
{
    if (vb != nullptr) {
        vb->Release();
    }
}

bool GenericPolyRenderable::is_in_range(const GW::GamePos& pos, const float dist_sq) const
{
    // future optimisation: use a bounding box in case of polygons with many points.
    const GW::Vec2f as_vec2 = {pos.x, pos.y};
    for (const auto& pt : points) {
        if (GW::GetSquareDistance(as_vec2, pt) <= dist_sq) {
            return true;
        }
    }
    return false;
}

void GenericPolyRenderable::draw(IDirect3DDevice9* device)
{
    // draw this specific renderable
    if (device->SetStreamSource(0, vb, 0, sizeof(D3DVertex)) != D3D_OK) {
        // a safe failure mode
        return;
    }
    // update altitudes if not done already
    if (!altitudes_computed) {
        // altitudes (Z value) for each vertex can't be known until we are in the correct map,
        // so these are dynamically computed, one-time.
        float altitude = ALTITUDE_UNKNOWN;
        for (std::size_t i = 0; i < vertices.size(); i++) {
            GW::Map::QueryAltitude({vertices[i].x, vertices[i].y, 0}, 10.0f, altitude);
            vertices[i].z = altitude - 1.0f; // slightly above terrain
        }
        altitudes_computed = true;
        void* mem_loc = nullptr;
        // map the vertex buffer memory and write vertices to it.
        if (vb->Lock(0, vertices.size() * sizeof(D3DVertex), (void**)&mem_loc, D3DLOCK_DISCARD) != S_OK) {
            // this should avoid an invalid memcpy, if locking fails for some reason
            GWCA_ERR("unable to lock GenericPolyRenderable vertex buffer");
        }
        else {
            memcpy(mem_loc, vertices.data(), vertices.size() * sizeof(D3DVertex));
            vb->Unlock();
        }
    }
    // copy the vertex buffer to the back buffer
    if (filled) {
        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices.size() / 3);
    }
    else {
        device->DrawPrimitive(D3DPT_LINESTRIP, 0, vertices.size() - 1);
    }
}

void GameWorldRenderer::set_3d_transforms(IDirect3DDevice9* device)
{
    // set up directX standard MVP matrices according to those used to render the game world
    // values here are largely according to GWCA's example code. my knowledge is admittedly lacking.

#define USED_FOR_PROJ_MATRIX 0
#define USED_FOR_MODEL_MATRIX 1

    auto R0 = *GW::Render::GetTransform((GW::Render::Transform)USED_FOR_PROJ_MATRIX);
    // clang-format off
    DirectX::XMFLOAT4X4 mat_proj(
        R0._11 / R0._33, 0.0f, 0.0f, 0.0f,
        0.0f, R0._22 / R0._33, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0001f, 1.0f,
        0.0f, 0.0f, -10.001f, 0.0f
    );
    // clang-format on
    device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&mat_proj);

    auto R1 = *GW::Render::GetTransform((GW::Render::Transform)USED_FOR_MODEL_MATRIX);
    // clang-format off
    DirectX::XMFLOAT4X4 mat_world(  // it's transpose of game's 4x3 matrix
        R1._11, R1._21, R1._31, 0.0f,
        R1._12, R1._22, R1._32, 0.0f,
        R1._13, R1._23, R1._33, 0.0f,
        R1._14, R1._24, R1._34, 1.0f
    );
    // clang-format on
    device->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)&mat_world);

    // view matrix is just identity
    DirectX::XMFLOAT4X4 mat_view;
    DirectX::XMStoreFloat4x4(&mat_view, DirectX::XMMatrixIdentity());
    device->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)&mat_view);
}

void GameWorldRenderer::Render(IDirect3DDevice9* device)
{
    if (need_sync_markers) {
        // marker synchronisation is done when needed on the render thread, as it requires access
        // to the directX device for creating vertex buffers.
        sync_all_markers(device);
    }
    if (renderables.empty()) {
        // as both a performance optimisation, and a safety feature, if there are no renderables,
        // i.e. nothing ticked "Draw On Terrain", then no extra directX stuff will happen here.
        // that means that it'll not hit performance much, and also, if there is some bug with
        // the rendering, TB can continue to function.
        return;
    }

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        // perhaps not actually needed, but it's here to be safe.
        return;
    }

    // backup original immediate state and transforms:
    DWORD old_D3DRS_COLORWRITEENABLE;
    DWORD old_D3DRS_LIGHTING;
    DWORD old_D3DRS_SCISSORTESTENABLE;
    DWORD old_D3DRS_STENCILENABLE;
    D3DMATRIX old_transform_projection;
    D3DMATRIX old_transform_world;
    D3DMATRIX old_transform_view;
    device->GetRenderState(D3DRS_COLORWRITEENABLE, &old_D3DRS_COLORWRITEENABLE);
    device->GetRenderState(D3DRS_LIGHTING, &old_D3DRS_LIGHTING);
    device->GetRenderState(D3DRS_SCISSORTESTENABLE, &old_D3DRS_SCISSORTESTENABLE);
    device->GetRenderState(D3DRS_STENCILENABLE, &old_D3DRS_STENCILENABLE);
    // backup original MVP transforms:
    device->GetTransform(D3DTS_WORLD, &old_transform_world);
    device->GetTransform(D3DTS_VIEW, &old_transform_view);
    device->GetTransform(D3DTS_PROJECTION, &old_transform_projection);

    // ensure we can write to all color bits
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
    // ensure there is no lighting effect applied
    device->SetRenderState(D3DRS_LIGHTING, false);
    // no scissor test / stencil (used by minimap)
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
    device->SetRenderState(D3DRS_STENCILENABLE, false);

    set_3d_transforms(device);

    const GW::Camera* cam = GW::CameraMgr::GetCamera();
    if (cam != nullptr) {
        // unsure if can ever be nullptr here, but failure mode is at least clean.
        GW::Vec2f look_at_2 = {cam->look_at_target.x, cam->look_at_target.y};
        GW::Constants::MapID map_id = GW::Map::GetMapID();
        for (const auto& renderable : renderables) {
            // future consideration: should we really render markers on terrian that have MapID=None?
            if (renderable->map_id == GW::Constants::MapID::None || renderable->map_id == map_id) {
                if (renderable->is_in_range(look_at_2, render_max_dist_sq)) {
                    renderable->draw(device);
                }
            }
        }
    }

    // restore immediate state:
    device->SetRenderState(D3DRS_COLORWRITEENABLE, old_D3DRS_COLORWRITEENABLE);
    device->SetRenderState(D3DRS_LIGHTING, old_D3DRS_LIGHTING);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, old_D3DRS_SCISSORTESTENABLE);
    device->SetRenderState(D3DRS_STENCILENABLE, old_D3DRS_STENCILENABLE);
    // restore MVP transforms:
    device->SetTransform(D3DTS_WORLD, &old_transform_world);
    device->SetTransform(D3DTS_VIEW, &old_transform_view);
    device->SetTransform(D3DTS_PROJECTION, &old_transform_projection);
}
void GameWorldRenderer::LoadSettings(ToolboxIni* ini, const char* section)
{
    // load the rendering settings from disk
    render_max_dist_enum = ini->GetLongValue(section, VAR_NAME(render_max_dist_enum), render_max_dist_enum);
    lerp_steps_per_line = ini->GetLongValue(section, VAR_NAME(lerp_steps_per_line), lerp_steps_per_line);
}
void GameWorldRenderer::SaveSettings(ToolboxIni* ini, const char* section) const
{
    // save the rendering settings to disk
    ini->SetLongValue(section, VAR_NAME(render_max_dist_enum), render_max_dist_enum);
    ini->SetLongValue(section, VAR_NAME(lerp_steps_per_line), lerp_steps_per_line);
}

void GameWorldRenderer::DrawSettings()
{
    // draw the settings using ImGui
    ImGui::Text("Note: custom markers are only rendered in-game if the option is enabled for a particular marker (check settings).");
    static constexpr const char* render_distance_choices[] = {"Earshot", "Spirit", "Compass"};
    need_sync_markers |= ImGui::Combo("Maximum render distance", &render_max_dist_enum, render_distance_choices, IM_ARRAYSIZE(render_distance_choices));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maximum distance to render custom markers on the in-game terrain.");
    need_sync_markers |= ImGui::DragInt("Interpolation granularity", &lerp_steps_per_line, 1.0f, 0, 100);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Number of points to interpolate. Affects smoothness of rendering.");
    }
}

void GameWorldRenderer::TriggerSyncAllMarkers()
{
    // a publically accessible function to trigger a re-sync of all custom markers
    need_sync_markers = true;
}

void GameWorldRenderer::Terminate()
{
    // free up any vertex buffers
    renderables.clear();
}

void GameWorldRenderer::sync_all_markers(IDirect3DDevice9* device)
{
    // synchronise _all_ markers with CustomRenderer
    switch (render_max_dist_enum) {
        case 0: // Earshot
            render_max_dist_sq = GW::Constants::Range::Earshot;
            break;
        case 1: // Spirit
            render_max_dist_sq = GW::Constants::Range::Spirit;
            break;
        case 2: // Compass
            render_max_dist_sq = GW::Constants::Range::Compass;
            break;
    }
    // as a performance optimisation, the distance comparison skips sqrt calculation,
    // so we pre-calculate the squared value ahead of time.
    render_max_dist_sq *= render_max_dist_sq;
    renderables.clear();
    sync_lines(device);
    sync_polys(device);
    sync_markers(device);
    need_sync_markers = false;
}

void GameWorldRenderer::sync_lines(IDirect3DDevice9* device)
{
    // sync lines with CustomRenderer
    const auto& lines = Minimap::Instance().custom_renderer.get_lines();
    // for each line, add as a renderable if appropriate
    for (const auto& line : lines) {
        if (!line.draw_on_terrain || !line.visible) {
            continue;
        }
        std::vector<GW::Vec2f> points = {line.p1, line.p2};
        renderables.push_back(std::make_unique<GenericPolyRenderable>(device, line.map, points, line.color, false));
    }
}

void GameWorldRenderer::sync_polys(IDirect3DDevice9* device)
{
    // sync polygons with CustomRenderer
    const auto& polys = Minimap::Instance().custom_renderer.get_polys();
    // for each poly, add as a renderable if appropriate
    for (const auto& poly : polys) {
        if (!poly.draw_on_terrain || !poly.visible) {
            continue;
        }
        renderables.push_back(std::make_unique<GenericPolyRenderable>(device, poly.map, poly.points, poly.color, poly.filled));
    }
}

void GameWorldRenderer::sync_markers(IDirect3DDevice9* device)
{
    // sync markers with CustomRenderer
    const auto& markers = Minimap::Instance().custom_renderer.get_markers();
    // for each marker, add as a renderable if appropriate
    for (const auto& marker : markers) {
        if (!marker.draw_on_terrain || !marker.visible) {
            continue;
        }
        std::vector<GW::Vec2f> points = circular_points_from_marker(marker.pos.x, marker.pos.y, marker.size);
        renderables.push_back(std::make_unique<GenericPolyRenderable>(device, marker.map, points, marker.color, false));
    }
}
