#pragma once

#include <GWCA/GameEntities/Party.h>

#include <ToolboxWidget.h>

#include "Defines.h"

#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/EffectRenderer.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Widgets/Minimap/PingsLinesRenderer.h>
#include <Widgets/Minimap/PmapRenderer.h>
#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/SymbolsRenderer.h>

class Minimap final : public ToolboxWidget {


    Minimap()
    {
        is_resizable = false;
    }

    ~Minimap() override = default;

public:
    Minimap(const Minimap&) = delete;

    static Minimap& Instance()
    {
        static Minimap instance;
        return instance;
    }



    const int ms_before_back = 1000; // time before we snap back to player
    const float acceleration = 0.5f;
    const float max_speed = 15.0f; // game units per frame

    [[nodiscard]] const char* Name() const override { return "Minimap"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    [[nodiscard]] float Scale() const;

    void DrawHelp() override;
    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;

    void Draw(IDirect3DDevice9* device) override;
    static void RenderSetupProjection(IDirect3DDevice9* device);

    bool FlagHeros(LPARAM lParam);
    bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) const;
    bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
    static void CHAT_CMD_FUNC(OnFlagHeroCmd);
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    [[nodiscard]] float GetMapRotation() const;
    [[nodiscard]] static DirectX::XMFLOAT2 GetGwinchScale();
    [[nodiscard]] GW::Vec2f ShadowstepLocation() const;

    // 0 is 'all' flag, 1 to 7 is each hero
    static bool FlagHero(uint32_t idx);

    RangeRenderer range_renderer;
    PmapRenderer pmap_renderer;
    AgentRenderer agent_renderer;
    PingsLinesRenderer pingslines_renderer;
    SymbolsRenderer symbols_renderer;
    CustomRenderer custom_renderer;
    EffectRenderer effect_renderer;

    static bool ShouldMarkersDrawOnMap();
    static bool ShouldDrawAllQuests();
    static void Render(IDirect3DDevice9* device);

    [[nodiscard]] static bool IsActive();

private:

    [[nodiscard]] bool IsInside(int x, int y) const;
    // returns true if the map is visible, valid, not loading, etc

    static void SelectTarget(GW::Vec2f pos);
    static size_t GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags = nullptr);

    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage /*msgid*/, void* /*wParam*/, void*);
};
