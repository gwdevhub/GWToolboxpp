#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameContainers/GamePos.h>

#include <D3DContainers.h>


namespace GW {
    struct Agent;
    struct AgentLiving;
    struct MapProp;

    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

using Color = uint32_t;

class ToolboxModule;

class AgentRenderer : public D3DVertexBuffer {
    friend class Minimap;
    static constexpr int num_triangles = 32;

public:
    AgentRenderer();

    void Terminate() override;
    static AgentRenderer& Instance();

    void Render(IDirect3DDevice9* device) override;

    void DrawSettings();
    void RegisterSettings(ToolboxModule* module);
    void LoadCustomAgents();
    void SaveCustomAgents() const;

    void LoadDefaultColors();
    void LoadDefaultSizes();

    bool show_hidden_npcs = false;
    bool show_quest_npcs_on_minimap = false;
    bool enemies_colors_by_profession = true;
    bool only_color_bosses = true;
    float agent_border_thickness = 0.f;
    float target_border_thickness = 50.f;

    uint32_t auto_target_id = 0;

    DWORD last_check = 0;

private:
    static AgentRenderer* instance;

    static constexpr size_t shape_size = 5;

    enum Shape_e { Tear, Circle, Quad, BigCircle, Star };

    enum Color_Modifier {
        None,
        // rgb 0,0,0
        Dark,
        // user defined
        Light,
        // user defined
        CircleCenter // alpha -50
    };

    enum CombatState { InCombat, NotInCombat, EitherCombat };
    enum WeaponState { HasWeapon, NoWeapon, EitherWeapon };

    class CustomAgent {
        static unsigned int cur_ui_id;

    public:
        enum class Operation {
            None,
            MoveUp,
            MoveDown,
            Delete,
            ModelIdChange
        };

        // glaze-serialized mirror of the persisted fields (AgentColors.json)
        struct Settings {
            bool active = true;
            std::string name;
            DWORD modelId = 0;
            DWORD mapId = 0;
            int combat_state = EitherCombat;
            int weapon_state = EitherWeapon;
            Colors::SettingColor color = 0xFFF00000;
            Colors::SettingColor color_text = 0xFFF00000;
            int shape = Tear;
            float size = 0.0f;
            bool color_active = true;
            bool color_text_active = false;
            bool shape_active = true;
            bool size_active = false;
        };

        CustomAgent(const ToolboxIni* ini, const char* section);
        explicit CustomAgent(const Settings& settings);
        CustomAgent(DWORD model_id, Color _color, const char* _name);

        bool DrawHeader();
        bool DrawSettings(Operation& op);
        [[nodiscard]] Settings ToSettings() const;

        // utility
        const unsigned int ui_id = 0; // to ensure UI consistency
        size_t index = 0;             // index in the array. Used for faster sorting.

        // define the agent
        bool active = true;
        char name[128]{};
        DWORD modelId = 0;
        DWORD mapId = 0; // 0 for 'any map'
        CombatState combat_state = CombatState::EitherCombat;
        WeaponState weapon_state = WeaponState::EitherWeapon;

        // attributes to change
        Color color = 0xFFF00000;
        Color color_text = 0xFFF00000;
        Shape_e shape = Tear;
        float size = 0.0f;
        bool color_active = true;
        bool color_text_active = false;
        bool shape_active = true;
        bool size_active = false;
    };

    struct Shape_Vertex : GW::Vec2f {
        Shape_Vertex(const float x, const float y, const Color_Modifier mod)
            : Vec2f(x, y), modifier(mod) { }

        Color_Modifier modifier;
    };

    struct Shape_t {
        std::vector<Shape_Vertex> vertices{};
        void AddVertex(float x, float y, Color_Modifier mod);
    };

    Shape_t shapes[shape_size];

    void Initialize(IDirect3DDevice9* device) override;


    Color GetColor(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;
    float GetSize(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;
    Shape_e GetShape(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;

    struct RenderPosition {
        float rotation_cos;
        float rotation_sin;
        GW::Vec2f position;
    };

    void Enqueue(const GW::Agent* agent, const CustomAgent* ca = nullptr);
    void Enqueue(Shape_e shape, const GW::Agent* agent, float size, Color color);
    void Enqueue(Shape_e shape, const GW::MapProp* agent, float size, Color color);
    void Enqueue(Shape_e shape, const RenderPosition& pos, float size, Color color, Color modifier = 0);

    std::vector<const CustomAgent*>* GetCustomAgentsToDraw(const GW::Agent* agent);


    Color color_agent_modifier = 0x001E1E1E;
    Color color_agent_damaged_modifier = 0x00505050;
    Color color_eoe = 0x3200FF00;
    Color color_qz = 0x320000FF;
    Color color_winnowing = 0x3200FFFF;
    Color color_frozen_soil = 0x00FEFFFF;
    Color color_target = 0xFFFFFF00;
    Color color_player = 0xFFFF8000;
    Color color_player_dead = 0x64FF8000;
    Color color_signpost = 0xFF0000C8;
    Color color_locked_chest = 0xFF0000C8;
    Color color_locked_chest_open = 0xFF0000C8;
    Color color_item = 0xFF0000F0;
    Color color_hostile = 0xFFF00000;
    Color color_hostile_dead = 0xFF320000;
    Color color_neutral = 0xFF0000DC;
    Color color_ally = 0xFF00B300;
    Color color_ally_npc = 0xFF99FF99;
    Color color_ally_npc_quest = 0xFF99FF99;
    Color color_ally_spirit = 0xFF608000;
    Color color_ally_minion = 0xFF008060;
    Color color_ally_dead = 0x64006400;
    Color color_marked_target = 0xFFFFFC00;

    Color profession_colors[11] = {
        0xFF666666,
        0xFFEEAA33,
        0xFF55AA00,
        0xFF4444BB,
        0xFF00AA55,
        0xFF8800AA,
        0xFFBB3333,
        0xFFAA0088,
        0xFF00AAAA,
        0xFF996600,
        0xFF7777CC
    };

    std::vector<CustomAgent*> custom_agents{};
    std::unordered_map<DWORD, std::vector<const CustomAgent*>> custom_agents_map{};
    void BuildCustomAgentsMap();
    //const CustomAgent* FindValidCustomAgent(DWORD modelid) const;

    float size_default = 100.f;
    float size_player = 100.f;
    float size_signpost = 50.f;
    float size_locked_chest = 50.f;
    float size_locked_chest_open = 50.f;
    float size_item = 25.f;
    float size_boss = 125.f;
    float size_minion = 50.f;
    float size_marked_target = 100.f;
    float size_hostile = 100.f;
    float size_neutral = 100.f;
    float size_ally = 100.f;
    float size_ally_npc = 100.f;
    float size_ally_npc_quest = 100.f;
    float size_ally_spirit = 100.f;
    bool marked_target_inherit_custom_agents = false;
    Shape_e default_shape = Tear;
    Shape_e shape_player = Tear;
    Shape_e shape_players = Tear;

    bool agentcolors_changed = false;
    bool custom_agents_loaded = false; // guards SaveCustomAgents against clobbering a file that was never read

    GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
};
