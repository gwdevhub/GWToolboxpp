#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <ToolboxIni.h>
#include <Widgets/Minimap/D3DVertex.h>
#include <stacktrace>

class GameWorldRenderer {
public:
    class GenericPolyRenderable {
    public:
        GenericPolyRenderable(GW::Constants::MapID map_id, const std::vector<GW::GamePos>& points, unsigned int col, bool filled) noexcept;
        ~GenericPolyRenderable() noexcept;

        // copy not allowed
        GenericPolyRenderable(const GenericPolyRenderable& other) = delete;

        GenericPolyRenderable(GenericPolyRenderable&& other) noexcept
            : vb(other.vb), map_id(other.map_id)
            , col(other.col)
            , filled(other.filled)
            , vertices_processed(other.vertices_processed)
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

            return *this;
        }

        void Draw(IDirect3DDevice9* device);
        GW::Constants::MapID map_id{};
        unsigned int col = 0u;
        std::vector<GW::GamePos> points{};
        std::vector<D3DVertex> vertices{};
        std::vector<uint32_t> vertices_zplanes{};
        bool filled = false;
        unsigned int vertices_processed = 0u;
        IDirect3DVertexBuffer9* vb = nullptr;


    };

    static void Render(IDirect3DDevice9* device);
    static void LoadSettings(const ToolboxIni* ini, const char* section);
    static void SaveSettings(ToolboxIni* ini, const char* section);
    static void DrawSettings();
    static void Terminate();
    static void TriggerSyncAllMarkers();

    using RenderableVectors = std::vector<GenericPolyRenderable>;

private:
    static RenderableVectors SyncLines();
    static RenderableVectors SyncPolys();
    static RenderableVectors SyncMarkers();
    static void SyncAllMarkers();
    static bool ConfigureProgrammablePipeline(IDirect3DDevice9* device);
    static bool SetD3DTransform(IDirect3DDevice9* device);
};
