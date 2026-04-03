#pragma once

#include <D3DContainers.h>

class SymbolsRenderer : public D3DVertexBuffer {
    friend class QuestModule;

public:
    SymbolsRenderer() = default;

    void Render(IDirect3DDevice9* device, float zoom = 1.f);
    void Render(IDirect3DDevice9* device) override { Render(device, 1.f); };

    void DrawSettings();
    void LoadSettings(const ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;

private:
    void Initialize(IDirect3DDevice9* device) override;
    Color color_quest = 0;
    Color color_quest_line = 0;
    Color color_other_quests = 0;
    Color color_north = 0;
    Color color_modifier = 0;

    const DWORD star_ntriangles = 16;
    DWORD star_offset = 0;

    const DWORD arrow_ntriangles = 2;
    DWORD arrow_offset = 0;

    const DWORD north_ntriangles = 2;
    DWORD north_offset = 0;

    bool initialized = false;
};
