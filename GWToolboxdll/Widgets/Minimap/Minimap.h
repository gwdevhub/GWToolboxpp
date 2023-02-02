#pragma once

#include <GWCA/GameEntities/Party.h>

#include <ToolboxWidget.h>

#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/EffectRenderer.h>
#include <Widgets/Minimap/PingsLinesRenderer.h>
#include <Widgets/Minimap/PmapRenderer.h>
#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/SymbolsRenderer.h>

class Minimap final : public ToolboxWidget
{
    struct Vec2i
    {
        Vec2i(int _x, int _y)
            : x(_x)
            , y(_y)
        {
        }
        Vec2i() = default;
        int x = 0;
        int y = 0;
    };
    Minimap()
    {
        is_resizable = false;
    };
    ~Minimap() = default;

public:
    Minimap(const Minimap&) = delete;

    static Minimap &Instance()
    {
        static Minimap instance;
        return instance;
    }

    enum class MinimapModifierBehaviour : int
    {
        Disabled,
        Draw,
        Target,
        Move,
        Walk
    };

    const int ms_before_back = 1000; // time before we snap back to player
    const float acceleration = 0.5f;
    const float max_speed = 15.0f; // game units per frame

    const char* Name() const override { return "Minimap"; }
    const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    float Scale() const { return scale; }

    void DrawHelp() override;
    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9 *device) override;
    void RenderSetupProjection(IDirect3DDevice9 *device) const;

    bool FlagHeros(LPARAM lParam);
    bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
    static void OnFlagHeroCmd(const wchar_t *message, int argc, LPWSTR *argv);
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(ToolboxIni *ini) override;
    void SaveSettings(ToolboxIni *ini) override;
    void DrawSettingInternal() override;

    float GetMapRotation() const;
    DirectX::XMFLOAT2 GetGwinchScale() const;
    GW::Vec2f ShadowstepLocation() const;

    // 0 is 'all' flag, 1 to 7 is each hero
    bool FlagHero(uint32_t idx);

    RangeRenderer range_renderer;
    PmapRenderer pmap_renderer;
    AgentRenderer agent_renderer;
    PingsLinesRenderer pingslines_renderer;
    SymbolsRenderer symbols_renderer;
    CustomRenderer custom_renderer;
    EffectRenderer effect_renderer;

    static void Render(IDirect3DDevice9* device);

private:

    bool IsInside(int x, int y) const;
    // returns true if the map is visible, valid, not loading, etc
    bool IsActive() const;

    GW::Vec2f InterfaceToWorldPoint(Vec2i pos) const;
    GW::Vec2f InterfaceToWorldVector(Vec2i pos) const;
    void SelectTarget(GW::Vec2f pos) const;
    bool IsKeyDown(MinimapModifierBehaviour mmb) const;

    bool mousedown = false;
    bool camera_currently_reversed = false;

    Vec2i location;
    Vec2i size;
    bool snap_to_compass = false;

    GW::Vec2f shadowstep_location = {0.f, 0.f};
    RECT clipping = { 0 };

    Vec2i drag_start;
    GW::Vec2f translation;
    float scale = 0.f;

    // vars for minimap movement
    clock_t last_moved = 0;

    bool loading = false; // only consider some cases but still good

    bool mouse_clickthrough_in_explorable = false;
    bool mouse_clickthrough_in_outpost = false;
    bool flip_on_reverse = false;
    bool rotate_minimap = true;
    bool smooth_rotation = true;
    bool circular_map = true;
    MinimapModifierBehaviour key_none_behavior = MinimapModifierBehaviour::Draw;
    MinimapModifierBehaviour key_ctrl_behavior = MinimapModifierBehaviour::Target;
    MinimapModifierBehaviour key_shift_behavior = MinimapModifierBehaviour::Move;
    MinimapModifierBehaviour key_alt_behavior = MinimapModifierBehaviour::Walk;
    bool is_observing = false;

    bool hero_flag_controls_show = false;
    bool hero_flag_window_attach = true;
    Color hero_flag_window_background = 0;
    std::vector<GW::AgentID> player_heroes{};

    size_t GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags = nullptr);

    GW::HookEntry AgentPinged_Entry;
    GW::HookEntry CompassEvent_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry SkillActivate_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
};
