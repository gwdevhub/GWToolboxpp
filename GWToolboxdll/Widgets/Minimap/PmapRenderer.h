#pragma once

#include <Color.h>
#include <Widgets/Minimap/VBuffer.h>

class PmapRenderer : public VBuffer {
public:
    // Triangle 1: (XTL, YT) (XTR, YT), (XBL, YB)
    // Triangle 2: (XBL, YB), (XTR, YT), (XBR, YB)
    void Render(IDirect3DDevice9* device) override;

    void DrawSettings();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;
    Color GetBackgroundColor() { return color_mapbackground; }


protected:
    void Initialize(IDirect3DDevice9* device) override;
private:
    Color color_map = 0;
    Color color_mapshadow = 0;
    Color color_mapbackground = 0;

    size_t trapez_count_ = 0;
    size_t tri_count_ = 0; // of just 1 batch (map)
    size_t total_tri_count_ = 0; // including shadow
    size_t vert_count_ = 0;
    size_t total_vert_count_ = 0; // including shadow
};
