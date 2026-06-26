#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <D3DContainers.h>
#include <ToolboxModule.h>

// In-game (game-world) rendering: draws custom markers, lines and polygons into the 3D
// world, occluded by terrain and drawn under the in-game UI. Its own settings module,
// with its own settings section / JSON file (separate from the Minimap).
class GameWorldRenderer : public ToolboxModule {
    GameWorldRenderer() = default;
    ~GameWorldRenderer() override = default;

public:
    static GameWorldRenderer& Instance()
    {
        static GameWorldRenderer instance;
        return instance;
    }

    class GenericPolyRenderable {
    public:
        GenericPolyRenderable(GW::Constants::MapID map_id, const std::vector<GW::GamePos>& points, unsigned int col, bool filled) noexcept;
        ~GenericPolyRenderable() noexcept;

        // copy not allowed
        GenericPolyRenderable(const GenericPolyRenderable& other) = delete;

        GenericPolyRenderable(GenericPolyRenderable&& other) noexcept
            : map_id(other.map_id), col(other.col)
            , filled(other.filled)
            , from_player_pos(other.from_player_pos)
            , use_dotted_effect(other.use_dotted_effect)
            , vertices_processed(other.vertices_processed)
            , vb(other.vb)
        {
            other.vb = nullptr;
            points = std::move(other.points);
            other.points.clear();
            vertices = std::move(other.vertices);
            other.vertices.clear();
        }

        // copy not allowed
        GenericPolyRenderable& operator=(const GenericPolyRenderable& other) = delete;

        GenericPolyRenderable& operator=(GenericPolyRenderable&& other) noexcept
        {
            if (vb && vb != other.vb) {
                vb->Release();
            }
            vb = other.vb; // Move the buffer!
            other.vb = nullptr;
            points = std::move(other.points);
            vertices = std::move(other.vertices);

            map_id = other.map_id;
            col = other.col;
            filled = other.filled;
            vertices_processed = other.vertices_processed;
            from_player_pos = other.from_player_pos;
            use_dotted_effect = other.use_dotted_effect;

            return *this;
        }

        void Draw(IDirect3DDevice9* device);
        GW::Constants::MapID map_id{};
        unsigned int col = 0u;
        std::vector<GW::GamePos> points{};
        std::vector<D3DVertex> vertices{};
        bool filled = false;
        bool from_player_pos = false;
        bool use_dotted_effect = false;
        unsigned int vertices_processed = 0u;
        IDirect3DVertexBuffer9* vb = nullptr;
    };

    using RenderableVectors = std::vector<GenericPolyRenderable>;

    // ToolboxModule
    [[nodiscard]] const char* Name() const override { return "In-game rendering"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Draws custom markers, lines and polygons into the 3D game world - occluded by terrain and drawn under the in-game UI.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CUBES; }
    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    // Rendering entry points. State is file-static (single instance), so these stay static
    // and are called directly from the render hooks and from the marker data sources.
    static void Render(IDirect3DDevice9* device);
    static void TriggerSyncAllMarkers();

    // Batched in-world line overlay: one incrementally terrain-draped vertex buffer drawn in a single call.
    // Used by the navmesh debug overlay instead of thousands of per-line CustomLines (which made each map-load
    // rebuild O(N^2) through the CustomLine sync/drape/remove path). Pass the full edge set once per map.
    struct BatchedLine {
        GW::GamePos a, b;
        unsigned int color;
    };
    // Near subset (range-culled): terrain-draped and drawn in-world as one batched VB.
    static void SetNavmeshLines(GW::Constants::MapID map_id, std::vector<BatchedLine> lines);
    // gw between terrain-altitude samples when draping overlay edges (smaller = hugs the floor closer, more verts).
    static void SetNavmeshSampleSpacing(float gw);
    // Re-drape the current edge set with the latest sample spacing (no re-cull). Call after SetNavmeshSampleSpacing.
    static void RedrapeNavmesh();
    // Full mesh for the 2D top-down M-key world map: not draped, redrawn flat by WorldMapWidget (game coords -> map).
    static void SetNavmeshWorldMapLines(GW::Constants::MapID map_id, std::vector<BatchedLine> lines);
    static void ClearNavmeshLines();
    static const std::vector<BatchedLine>& GetNavmeshWorldMapLines();
    static GW::Constants::MapID GetNavmeshWorldMapMapId();
    // Max in-world render distance (game units) — the shared "Maximum render distance" for terrain overlays.
    static float GetRenderMaxDistance();

private:
    static void RegisterSettings(ToolboxModule* module);
    static void OnSettingsLoaded();
    static void DrawSettings();
    static RenderableVectors SyncLines();
    static RenderableVectors SyncPolys();
    static RenderableVectors SyncMarkers();
    static void SyncAllMarkers();
    // Draws the markers into the world (compositor callback / on-top fallback).
    static void DrawInWorld(IDirect3DDevice9* device);
    // Register/unregister our under-UI draw with the shared compositor based on render_under_ui.
    static void UpdateCompositorRegistration();
};
