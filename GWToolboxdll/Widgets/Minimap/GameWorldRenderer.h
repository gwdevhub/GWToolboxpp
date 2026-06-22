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
