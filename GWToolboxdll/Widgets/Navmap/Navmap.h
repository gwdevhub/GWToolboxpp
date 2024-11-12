#pragma once

//#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/UIMgr.h>

#include <ToolboxWidget.h>

//#include <Widgets/Navmap/NavAgentRenderer.h>
//#include <Widgets/Minimap/CustomRenderer.h>
//#include <Widgets/Navmap/NavEffectRenderer.h>
#include <Widgets/Navmap/NavGameWorldRenderer.h>
//#include <Widgets/Navmap/NavPingsLinesRenderer.h>
#include <Widgets/Navmap/NavPmapRenderer.h>
#include <Widgets/Navmap/NavMarkerRenderer.h>

class Navmap final : public ToolboxWidget {
    struct Vec2i {
        Vec2i(const int _x, const int _y)
            : x(_x)
            , y(_y) { }

        Vec2i() = default;
        int x = 0;
        int y = 0;
    };

    Navmap()
    {
        is_resizable = false;
    }

    ~Navmap() override = default;

public:
    Navmap(const Navmap&) = delete;

    static Navmap& Instance()
    {
        static Navmap instance;
        return instance;
    }

    enum class NavmapModifierBehaviour : int {
        Disabled,
        Move
    };

    const int ms_before_back = 1000; // time before we snap back to player
    const float acceleration = 0.5f;
    const float max_speed = 15.0f; // game units per frame

    [[nodiscard]] const char* Name() const override { return "Navmap"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_ROUTE; }

    [[nodiscard]] float Scale() const { return scale; }

    void DrawHelp() override;
    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* device) override;
    void RenderSetupProjection(IDirect3DDevice9* device) const;

    //bool FlagHeros(LPARAM lParam);
    bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) const;
    bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
    //static void OnFlagHeroCmd(const wchar_t* message, int argc, const LPWSTR* argv);
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    [[nodiscard]] float GetMapRotation() const;
    [[nodiscard]] static DirectX::XMFLOAT2 GetGwinchScale();
    //[[nodiscard]] GW::Vec2f ShadowstepLocation() const;

    // 0 is 'all' flag, 1 to 7 is each hero
    //static bool FlagHero(uint32_t idx);

    NavMarkerRenderer marker_renderer;
    NavPmapRenderer pmap_renderer;
    //NavAgentRenderer agent_renderer;
    //NavPingsLinesRenderer pingslines_renderer;
    //NavSymbolsRenderer symbols_renderer;
    //CustomRenderer custom_renderer;
    //NavEffectRenderer effect_renderer;
    NavGameWorldRenderer game_world_renderer;

    //static bool ShouldMarkersDrawOnMap();
    static void Render(IDirect3DDevice9* device);

    void DrawSizeAndPositionSettings() override;

    void Update(float delta) override;

private:
    [[nodiscard]] bool IsInside(int x, int y) const;
    // returns true if the map is visible, valid, not loading, etc
    [[nodiscard]] bool IsActive() const;

    [[nodiscard]] GW::Vec2f InterfaceToWorldPoint(Vec2i pos) const;
    [[nodiscard]] GW::Vec2f InterfaceToWorldVector(Vec2i pos) const;
    //static void SelectTarget(GW::Vec2f pos);
    [[nodiscard]] bool IsKeyDown(NavmapModifierBehaviour mmb) const;

    bool mousedown = false;
    bool camera_currently_reversed = false;

    //float testx = 0.0f, testy = 0.0f;

    float center_y_offset = 120.0f;

    Vec2i location;
    Vec2i size;
    //bool snap_to_compass = false;

    //GW::Vec2f shadowstep_location = {0.f, 0.f};
    RECT clipping = {0};

    Vec2i drag_start;
    GW::Vec2f translation;
    float scale = 0.f;

    // vars for navmap movement
    clock_t last_moved = 0;

    bool loading = false; // only consider some cases but still good

    bool mouse_clickthrough_in_explorable = false;
    bool mouse_clickthrough_in_outpost = false;
    //bool flip_on_reverse = false;
    bool rotate_navmap = true;
    bool smooth_rotation = true;
    bool circular_map = false;
    NavmapModifierBehaviour key_none_behavior = NavmapModifierBehaviour::Disabled;
    NavmapModifierBehaviour key_ctrl_behavior = NavmapModifierBehaviour::Disabled;
    NavmapModifierBehaviour key_shift_behavior = NavmapModifierBehaviour::Disabled;
    NavmapModifierBehaviour key_alt_behavior = NavmapModifierBehaviour::Disabled;
    bool is_observing = false;

    //bool hero_flag_controls_show = false;
    //bool hero_flag_window_attach = false;
    //Color hero_flag_window_background = 0;
    //std::vector<GW::AgentID> player_heroes{};

    //static size_t GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags = nullptr);

    //GW::HookEntry AgentPinged_Entry;
    //GW::HookEntry CamRev_Entry1;
    //GW::HookEntry CamRev_Entry2;
    GW::HookEntry CompassEvent_Entry;
    //GW::HookEntry GenericValueTarget_Entry;
    //GW::HookEntry SkillActivate_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage /*msgid*/, void* /*wParam*/, void*);
};
