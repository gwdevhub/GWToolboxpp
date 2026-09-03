#include <Rendering.h>

#include <array>
#include <vector>
#include <mutex>
#include <numbers>
#include <chrono>

#include "Windows.h"
#include <d3d9types.h>
#include <d3d9.h>
#include <DirectXMath.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Camera.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <Widgets/Minimap/Shaders/game_world_renderer_vs.h>
#include <Widgets/Minimap/Shaders/game_world_renderer_dotted_ps.h>
#include <mapbox/earcut.hpp>

namespace mapbox::util {
    template <>
    struct nth<0, GW::GamePos> {
        static auto get(const GW::GamePos& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::GamePos> {
        static auto get(const GW::GamePos& t) { return t.y; }
    };

    template <>
    struct nth<0, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.y; }
    };

    template <>
    struct nth<0, GW::Vec3f> {
        static auto get(const GW::Vec3f& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::Vec3f> {
        static auto get(const GW::Vec3f& t) { return t.y; }
    };
} // namespace mapbox::util

namespace {
    constexpr float render_max_distance = 5000.f;
    constexpr float fog_factor = 0.5f;
    unsigned lerp_steps_per_line = 10;

    std::mutex renderables_mutex{};
    IDirect3DVertexShader9* vshader = nullptr;
    IDirect3DPixelShader9* pshader = nullptr;
    IDirect3DVertexDeclaration9* vertex_declaration = nullptr;

    struct D3DVertex {
        float x;
        float y;
        float z;
        D3DCOLOR color;
    };
    constexpr DWORD D3DFVF_CUSTOMVERTEX = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    class GenericPolyRenderable {
    public:
        GenericPolyRenderable(const std::vector<GW::GamePos>& points, unsigned int col, bool filled, std::optional<std::chrono::steady_clock::time_point> despawnTime, const void* owner = nullptr) noexcept 
            : map_id(GW::Map::GetMapID()), col(col), points(points), despawnTime{despawnTime}, filled(filled), owner(owner)
        {}
        ~GenericPolyRenderable() noexcept
        {
            if (vb != nullptr) {
                vb->Release();
                vb = nullptr;
            }
        }

        // copy not allowed
        GenericPolyRenderable(const GenericPolyRenderable&) = delete;
        GenericPolyRenderable& operator=(const GenericPolyRenderable& other) = delete;

        GenericPolyRenderable& operator=(GenericPolyRenderable&& other) noexcept
        {
            if (vb && vb != other.vb) 
            {
                vb->Release();
            }
            vb = other.vb; // Move the buffer!
            other.vb = nullptr;

            map_id = other.map_id;
            col = other.col;
            points = std::move(other.points);
            vertices = std::move(other.vertices);
            vertices_zplanes = std::move(other.vertices_zplanes);
            despawnTime = std::move(other.despawnTime);
            filled = other.filled;
            from_player_pos = other.from_player_pos;
            use_dotted_effect = other.use_dotted_effect;
            vertices_processed = other.vertices_processed;
            owner = other.owner;

            return *this;
        }
        GenericPolyRenderable(GenericPolyRenderable&& other) noexcept { *this = std::move(other); }

        void Draw(IDirect3DDevice9* device);
        GW::Constants::MapID map_id{};
        unsigned int col = 0u;
        std::vector<GW::GamePos> points{};
        std::vector<D3DVertex> vertices{};
        std::vector<uint32_t> vertices_zplanes{};
        std::optional<std::chrono::steady_clock::time_point> despawnTime{};
        bool filled = false;
        bool from_player_pos = false;
        bool use_dotted_effect = false;
        unsigned int vertices_processed = 0u;
        IDirect3DVertexBuffer9* vb = nullptr;
        const void* owner = nullptr;
    };

    constexpr GW::Vec2f lerp(const GW::Vec2f& a, const GW::Vec2f& b, const float t)
    {
        return a * t + b * (1.f - t);
    }

    constexpr auto ALTITUDE_UNKNOWN = std::numeric_limits<float>::max();

    std::vector<GW::GamePos> ellipsoidal_points_from_marker(const GW::GamePos& marker, const GW::Vec2f ADirection, float a, float b)
    {
        std::vector<GW::GamePos> points{};
        constexpr float pi = (float)std::numbers::pi;
        constexpr size_t num_points_per_circle = 48;
        constexpr auto slice = 2.0f * pi / static_cast<float>(num_points_per_circle);

        const auto BDirection = GW::Vec2f{-ADirection.y, ADirection.x};
        const auto scaledA = a * ADirection;
        const auto scaledB = b * BDirection;

        for (auto i = 0u; i < num_points_per_circle; i++) {
            const auto angle = slice * static_cast<float>(i);
            auto point = GW::Vec2f{marker};
            point += std::cos(angle) * scaledA + std::sin(angle) * scaledB;
            points.push_back({point.x, point.y, marker.zplane});
        }
        points.push_back(points.at(0)); // to complete the line list
        return points;
    }

    // update altitudes if not done already, then add to the device buffer
    bool AddPolyToDevice(GenericPolyRenderable& poly, IDirect3DDevice9* device)
    {
        if (poly.vb) return true; // Already created the vertex buffer for this poly, which means altitudes have been done!
        auto& vertices = poly.vertices;
        if (poly.vertices_processed == vertices.size()) return true;
        
        // in order to properly query altitudes, we have to use the pathing map
        // to determine the number of Z planes in the current map.
        const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map || pathing_map->size() == 0) return false;

        const auto z_plane0 = poly.vertices_zplanes[0];

        auto queryPos = GW::GamePos{vertices[0].x, vertices[0].y, z_plane0};
        auto altitude = GW::Map::QueryAltitude(&queryPos, 5.f);
        
        const auto altitude0 = altitude;
        ++poly.vertices_processed;
        vertices[0].z = altitude;

        const auto z_planeZ = poly.vertices_zplanes[vertices.size() - 1];
        queryPos = GW::GamePos{vertices[vertices.size() - 1].x, vertices[vertices.size() - 1].y, z_planeZ};
        const auto altitudeZ = GW::Map::QueryAltitude(&queryPos, 5.f);
        vertices[vertices.size() - 1].z = altitudeZ;

        const auto altitude_diff = altitudeZ - altitude0;

        for (size_t i = poly.vertices_processed; i < vertices.size() - 1; i++, poly.vertices_processed++) {
            // until we have a better solution, all Z planes will be queried per vertex
            // to avoid a significant delay in the render thread, query one plane per frame
            // until all have been queried. this might result in some renderables shifting
            // not appearing for a while on first map load, but IMO is better than stalling.
            // It seems to take, even in most extreme cases, less time than it takes for agents
            // to appear.

            // @Cleanup: zplane needs setting properly here!
            const auto z_plane = poly.vertices_zplanes[i];
            queryPos = GW::GamePos{vertices[i].x, vertices[i].y, z_plane};
            altitude = GW::Map::QueryAltitude(&queryPos, 5.f);

            if (altitude < vertices[i].z) {
                // recall that the Up camera component is inverted
                vertices[i].z = altitude;
            }

            const auto guessed_altitude = altitude0 + (altitude_diff * static_cast<float>(i) / static_cast<float>(vertices.size() - 1));
            if (std::abs(altitude - guessed_altitude) > 20.f) {
                auto min_diff = std::abs(altitude - guessed_altitude);
                for (unsigned zplane = pathing_map->size() - 1; zplane >= 1; --zplane) {
                    queryPos = GW::GamePos{vertices[i].x, vertices[i].y, zplane};
                    altitude = GW::Map::QueryAltitude(&queryPos, 5.f);
                    const auto cur_diff = std::abs(altitude - guessed_altitude);
                    if (cur_diff < min_diff && altitude < vertices[i].z) {
                        min_diff = cur_diff;
                        vertices[i].z = altitude;
                    }
                }
            }
        }
        ++poly.vertices_processed;

        // commit the completed vertices to vram
        auto res = device->CreateVertexBuffer(vertices.size() * sizeof(D3DVertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &poly.vb, nullptr);
        if (res != S_OK) {
            poly.vb = nullptr;
            return false;
        }

        // map the vertex buffer memory and write vertices to it.
        void* mem_loc = nullptr;
        res = poly.vb->Lock(0, vertices.size() * sizeof(D3DVertex), &mem_loc, D3DLOCK_DISCARD);
        if (res != S_OK || !mem_loc) {
            poly.vb->Release();
            poly.vb = nullptr;
            return false;
        }

        memcpy(mem_loc, vertices.data(), vertices.size() * sizeof(D3DVertex));
        poly.vb->Unlock();
        return true;
    }

    void GenericPolyRenderable::Draw(IDirect3DDevice9* device)
    {
        if (vertices.empty()) {
            if (filled && points.size() >= 3) {
                // (filling doesn't make sense if there is not at least enough points for one triangle)
                std::vector<GW::GamePos> lerp_points{};
                for (size_t i = 0; i < points.size(); i++) {
                    if (!lerp_points.empty() && lerp_steps_per_line > 0) {
                        for (auto j = 1u; j < lerp_steps_per_line; j++) {
                            const float div = static_cast<float>(j) / static_cast<float>(lerp_steps_per_line);
                            auto split = lerp(points[i], points[i - 1], div);
                            lerp_points.emplace_back(split.x, split.y, points[i].zplane);
                        }
                    }
                    lerp_points.push_back(points[i]);
                }
                const std::vector<unsigned> indices = mapbox::earcut<unsigned>(std::vector{{lerp_points}});
                for (size_t i = 0; i < indices.size(); i++) {
                    const auto& pt = lerp_points[indices[i]];
                    vertices.emplace_back(pt.x, pt.y, ALTITUDE_UNKNOWN, col);
                    vertices_zplanes.push_back(pt.zplane);
                }
            }
            else {
                for (size_t i = 0; i < points.size(); i++) {
                    const auto& pt = points[i];
                    if (!vertices.empty() && lerp_steps_per_line > 0) {
                        for (auto j = 1u; j < lerp_steps_per_line; j++) {
                            const auto div = static_cast<float>(j) / static_cast<float>(lerp_steps_per_line);
                            const auto split = lerp(points[i], points[i - 1], div);
                            vertices.emplace_back(split.x, split.y, ALTITUDE_UNKNOWN, col);
                            const auto zplanes = std::vector{points[i].zplane, points[i - 1].zplane, points[0].zplane, points[points.size() - 1].zplane};
                            const auto zplane = std::ranges::max_element(zplanes);
                            vertices_zplanes.push_back(*zplane);
                        }
                    }
                    vertices.emplace_back(pt.x, pt.y, ALTITUDE_UNKNOWN, col);
                    vertices_zplanes.push_back(pt.zplane);
                }
            }
        }
        if (vertices.size() < (filled ? 3u : 2u)) {
            return;
        }

        if (!AddPolyToDevice(*this, device)) {
            return;
        }

        if (from_player_pos && vertices.size() > 1) {
            if (const auto player = GW::Agents::GetControlledCharacter()) {
                size_t vertices_write_cnt = 0;
                for (vertices_write_cnt = 0; vertices_write_cnt < vertices.size() - 1; ++vertices_write_cnt) {
                    auto& vertex = vertices[vertices_write_cnt];
                    if (vertices_write_cnt == 0 || GW::GetSquareDistance(player->pos, {vertex.x, vertex.y}) < GW::Constants::SqrRange::Earshot) {
                        vertex.x = player->pos.x;
                        vertex.y = player->pos.y;
                        vertex.z = player->name_tag_z;
                        continue;
                    }
                    break;
                }

                void* mem_loc = nullptr;
                auto res = vb->Lock(0, vertices_write_cnt * sizeof(D3DVertex), &mem_loc, D3DLOCK_DISCARD);
                if (res == S_OK) {
                    memcpy(mem_loc, vertices.data(), vertices_write_cnt * sizeof(D3DVertex));
                    vb->Unlock();
                }
            }
        }

        // draw this specific renderable
        if (device->SetStreamSource(0, vb, 0, sizeof(D3DVertex)) != D3D_OK) {
            // a safe failure mode
            return;
        }

        // Set the use_dotted_effect value as a pixel shader constant
        const BOOL dotted_effect_constant[1] = {static_cast<BOOL>(use_dotted_effect)};
        if (device->SetPixelShaderConstantB(0, dotted_effect_constant, 1) != D3D_OK) {
            return;
        }

        // copy the vertex buffer to the back buffer
        filled ? device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices.size() / 3) : device->DrawPrimitive(D3DPT_LINESTRIP, 0, vertices.size() - 1);
    }

    bool need_configure_pipeline = true;
    bool ConfigureProgrammablePipeline(IDirect3DDevice9* device)
    {
        constexpr D3DVERTEXELEMENT9 decl[] = {{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0}, D3DDECL_END()};
        if (!vertex_declaration && device->CreateVertexDeclaration(decl, &vertex_declaration) != D3D_OK) {
            return false;
        }

        if (!vshader
            && device->CreateVertexShader(reinterpret_cast<const DWORD*>(&game_world_renderer_vs), &vshader)
                != D3D_OK) {
            return false;
        }
        if (!pshader
            && device->CreatePixelShader(
                   reinterpret_cast<const DWORD*>(&game_world_renderer_dotted_ps), &pshader)
                != D3D_OK) {
            return false;
        }
        need_configure_pipeline = false;
        return true;
    }

    bool SetD3DTransform(IDirect3DDevice9* device)
    {
        // set up directX standard view/proj matrices according to those used to render the game world
        if (device == nullptr) {
            return false;
        }

        constexpr auto vertex_shader_view_matrix_offset = 0u;
        constexpr auto vertex_shader_proj_matrix_offset = 4u;

        // compute view matrix:
        DirectX::XMFLOAT4X4A mat_view{};
        const auto cam = GW::CameraMgr::GetCamera();
        const auto viewport_height = GW::Render::GetViewportHeight();
        if (!cam || !viewport_height) {
            return false;
        }
        const DirectX::XMFLOAT3 eye_pos = {cam->position.x, cam->position.y, cam->position.z};
        const DirectX::XMFLOAT3 player_pos = {cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z};
        constexpr DirectX::XMFLOAT3 up = {0.0f, 0.0f, -1.0f};
        XMStoreFloat4x4A(&mat_view, XMMatrixTranspose(DirectX::XMMatrixLookAtLH(XMLoadFloat3(&eye_pos), XMLoadFloat3(&player_pos), XMLoadFloat3(&up))));
        if (device->SetVertexShaderConstantF(vertex_shader_view_matrix_offset, reinterpret_cast<const float*>(&mat_view), 4) != D3D_OK) {
            return false;
        }

        // compute projection matrix:
        DirectX::XMFLOAT4X4A mat_proj{};
        const auto fov = GW::Render::GetFieldOfView();
        const auto aspect_ratio = static_cast<float>(GW::Render::GetViewportWidth())
            / static_cast<float>(viewport_height);

        XMStoreFloat4x4A(&mat_proj, XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, 0.1f, 100000.0f)));
        if (device->SetVertexShaderConstantF(vertex_shader_proj_matrix_offset, reinterpret_cast<const float*>(&mat_proj), 4) != D3D_OK) {
            return false;
        }

        return true;
    }
    std::vector<GenericPolyRenderable> renderables;
    std::optional<GenericPolyRenderable> singletonPolyline;
    int last_draw_frame = -1;
} // namespace

namespace RenderingUtils {
    void clearDrawingList(const void* owner)
    {
        std::scoped_lock lock{renderables_mutex};
        if (owner) {
            std::erase_if(renderables, [owner](const auto& renderable) {
                return renderable.owner == owner;
            });
        }
        else {
            renderables.clear();
            singletonPolyline = {};
        }
    }

    void addCircleToDraw(GW::GamePos center, float radius, unsigned int color, bool filled, std::optional<int> msToShow, const void* owner)
    {
        addEllipseToDraw(center, {1.f, 1.f}, radius, radius, color, filled, msToShow, owner);
    }

    void addEllipseToDraw(GW::GamePos center, GW::Vec2f ADirection, float a, float b, unsigned int color, bool filled, std::optional<int> msToShow, const void* owner)
    {
        auto circlePositions = ellipsoidal_points_from_marker(center, ADirection, a, b);
        std::scoped_lock lock{renderables_mutex};

        if (msToShow) {
            auto despawnTime = std::chrono::steady_clock::now() + std::chrono::milliseconds{*msToShow};
            renderables.push_back(GenericPolyRenderable{circlePositions, color, filled, despawnTime, owner});
        }
        else {
            renderables.push_back(GenericPolyRenderable{circlePositions, color, filled, std::nullopt, owner});
        }
    }

    void addPolylineToDraw(std::vector<GW::GamePos>&& polyline, const unsigned int color, const void* owner)
    {
        std::scoped_lock lock{renderables_mutex};
        renderables.emplace_back(polyline, color, false, std::nullopt, owner);
    }

    void addSingletonPolyline(std::vector<GW::GamePos>&& polyline, unsigned int color) 
    {
        std::scoped_lock lock{renderables_mutex};
        singletonPolyline = GenericPolyRenderable{std::move(polyline), color, false, std::nullopt};
    }

    void clearSingletonPolyline() 
    {
        std::scoped_lock lock{renderables_mutex};
        if (!singletonPolyline.has_value()) return;
        singletonPolyline = {};
    }

    void draw(IDirect3DDevice9* device)
    {
        if (!device) return;
        const auto frame = ImGui::GetFrameCount();
        if (last_draw_frame == frame) return;
        last_draw_frame = frame;
        {
            std::scoped_lock lock{renderables_mutex};
            if (renderables.empty() && !singletonPolyline) return;
        }
        if (need_configure_pipeline) {
            if (!ConfigureProgrammablePipeline(device)) {
                return;
            }
        }

        if (vshader == nullptr || device->SetVertexShader(vshader) != D3D_OK) {
            return;
        }
        if (pshader == nullptr || device->SetPixelShader(pshader) != D3D_OK) {
            return;
        }
        if (device->SetVertexDeclaration(vertex_declaration) != D3D_OK) {
            return;
        }

        // backup original immediate state and transforms:
        DWORD old_D3DRS_SCISSORTESTENABLE = 0;
        DWORD old_D3DRS_STENCILENABLE = 0;
        device->GetRenderState(D3DRS_SCISSORTESTENABLE, &old_D3DRS_SCISSORTESTENABLE);
        device->GetRenderState(D3DRS_STENCILENABLE, &old_D3DRS_STENCILENABLE);

        // no scissor test / stencil (used by minimap)
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, 0u);
        device->SetRenderState(D3DRS_STENCILENABLE, 0u);

        const auto cam = GW::CameraMgr::GetCamera();
        if (SetD3DTransform(device) && cam) {
            // set Pixel Shader constants. they are always expressed as Float4 here:
            // first is the player's position

            constexpr auto pixel_shader_cur_pos_offset = 0u;
            constexpr auto pixel_shader_max_dist_offset = 1u;
            
            const float cur_pos_constant[4] = {cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z, 0.0f};
            const float max_dist_constant[4] = {render_max_distance, 0.0f, 0.0f, 0.0f};
            if (device->SetPixelShaderConstantF(pixel_shader_cur_pos_offset, cur_pos_constant, 1) == D3D_OK
                && device->SetPixelShaderConstantF(pixel_shader_max_dist_offset, max_dist_constant, 1) == D3D_OK) {
                const auto map_id = GW::Map::GetMapID();
                const auto now = std::chrono::steady_clock::now();
                std::scoped_lock lock{renderables_mutex};
                std::erase_if(renderables, [&now](const auto& r) {return r.despawnTime && now > *r.despawnTime;});
                for (auto& renderable : renderables) 
                {
                    if (renderable.map_id == map_id) renderable.Draw(device);
                }
                if (singletonPolyline) 
                {
                    singletonPolyline->Draw(device);
                }
            }
        }

        // restore immediate state:
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, old_D3DRS_SCISSORTESTENABLE);
        device->SetRenderState(D3DRS_STENCILENABLE, old_D3DRS_STENCILENABLE);
    }

    void shutdown()
    {
        std::scoped_lock lock{renderables_mutex};
        renderables.clear();
        singletonPolyline = {};
        if (pshader) {
            pshader->Release();
            pshader = nullptr;
        }
        if (vshader) {
            vshader->Release();
            vshader = nullptr;
        }
        if (vertex_declaration) {
            vertex_declaration->Release();
            vertex_declaration = nullptr;
        }
        need_configure_pipeline = true;
        last_draw_frame = -1;
    }
} // namespace RenderingUtils
