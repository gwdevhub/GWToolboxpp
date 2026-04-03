#pragma once

#include <Color.h>
#include <D3DContainers.h>

struct MinimapRenderContext;

class PmapRenderer : public D3DVertexBuffer {
public:
    // Triangle 1: (XTL, YT) (XTR, YT), (XBL, YB)
    // Triangle 2: (XBL, YB), (XTR, YT), (XBR, YB)
    void Render(IDirect3DDevice9*) override
    {
        ASSERT(false && "PmapRenderer::Render without MinimapRenderContext called!");
    };
    void Render(IDirect3DDevice9* device, const MinimapRenderContext&);

    void DrawSettings();

protected:
    void Initialize(IDirect3DDevice9* device) override;

private:
    size_t trapez_count_ = 0;
    size_t tri_count_ = 0; // of just 1 batch (map)
    size_t total_tri_count_ = 0; // including shadow
    size_t vert_count_ = 0;
    size_t total_vert_count_ = 0; // including shadow
};
