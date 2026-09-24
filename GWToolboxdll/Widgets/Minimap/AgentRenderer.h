#pragma once

#include <array>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <D3DContainers.h>
#include <Utils/TextUtils.h>
#include <Widgets/Minimap/CustomRenderer.h>


namespace GW {
    struct Agent;
    struct AgentLiving;
    struct MapProp;

    namespace UI {
        enum class UIMessage : uint32_t;
    }
    namespace Constants {
        enum class Allegiance : uint8_t;
    }
}

using Color = uint32_t;

class ToolboxModule;
class SettingsDoc;

class AgentRenderer : public D3DVertexBuffer {
    friend class Minimap;
    static constexpr int num_triangles = 32;

public:
    AgentRenderer();

    void Terminate() override;
    static AgentRenderer& Instance();
    static bool AppearanceRulesLoaded();

    void Render(IDirect3DDevice9* device) override;

    void DrawSettings();
    void RegisterSettings(ToolboxModule* module);
    void RegisterMinimapSettings(ToolboxModule* module);
    void LoadCustomAgents(SettingsDoc& doc, ToolboxIni* legacy);
    void SaveCustomAgents(SettingsDoc& doc) const;
    bool ApplyNameTagColor(const GW::Agent* agent, Color& color);
    void InvalidateAppearance(uint32_t agent_id);
    void ReleaseAppearanceHooks();

    void LoadDefaultColors();
    void LoadDefaultSizes();
    void ResetAppearanceSettings();
    void LoadLegacyAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy);

    Color GetProfessionColor(GW::Constants::Profession profession) const;

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

    enum Shape_e { Shape_None = -1, Tear, Circle, Quad, BigCircle, Star };

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
    enum DeadState { Dead, Alive, EitherDeadState };
    enum QuestState { QuestGiver, NotQuestGiver, EitherQuestState };
    enum AgentType { Any = 1, Item, Gadget, NPC, Player };
    enum TargetState { EitherTarget, Targeted, NotTargeted, Marked };
    enum PlayerRelation { AnyRelation, Self, Other, Friend, Guild, MyParty, InParty };
    enum GadgetState { AnyGadget, ClosedChest, OpenedChest, OtherGadget };

    class CustomAgent {
        static unsigned int cur_ui_id;

    public:
        enum class Operation {
            None,
            MoveUp,
            MoveDown,
            Delete
        };

        struct Settings {
            bool active = true;
            std::string name;
            std::string group;
            DWORD modelId = 0;
            DWORD mapId = 0;
            int combat_state = EitherCombat;
            int weapon_state = EitherWeapon;
            int allegiance = -1;
            int dead_state = EitherDeadState;
            int quest_state = EitherQuestState;
            Colors::SettingColor color = 0;
            Colors::SettingColor color_text = 0;
            int shape = Shape_None;
            float size = 0.0f;
            int agent_type = 0;
            DWORD identifier = 0;
            bool identifier_active = false;
            std::string match_name;
            int target_state = EitherTarget;
            int player_relation = AnyRelation;
            bool outpost_only = false;
            int gadget_state = AnyGadget;
            int profession = 0;
            int boss_state = 0;
            Colors::SettingColor border_color = 0;
        };

        struct LegacyFlags {
            bool is_default = false;
            bool color_active = true;
            bool color_text_active = false;
            bool shape_active = true;
            bool size_active = false;
            bool border_color_active = false;
        };

        CustomAgent(const ToolboxIni* ini, const char* section);
        explicit CustomAgent(const Settings& settings);
        CustomAgent(DWORD model_id, Color _color, const char* _name);

        bool DrawHeader();
        bool DrawSettings(Operation& op);
        [[nodiscard]] Settings ToSettings() const;
        void ApplyLegacyFlags(const LegacyFlags& flags);

        // utility
        const unsigned int ui_id = 0; // to ensure UI consistency
        size_t index = 0;             // index in the array. Used for faster sorting.

        // define the agent
        bool active = true;
        char name[128]{};
        char group[64]{};
        DWORD modelId = 0;
        DWORD mapId = 0; // 0 for 'any map'
        CombatState combat_state = CombatState::EitherCombat;
        WeaponState weapon_state = WeaponState::EitherWeapon;
        int allegiance = -1;
        DeadState dead_state = DeadState::EitherDeadState;
        QuestState quest_state = QuestState::EitherQuestState;

        // attributes to change
        Color color = 0;
        Color color_text = 0;
        Shape_e shape = Shape_None;
        float size = 0.0f;
        AgentType agent_type = NPC;
        DWORD identifier = 0;
        bool identifier_active = false;
        char match_name[128]{};
        TargetState target_state = EitherTarget;
        PlayerRelation player_relation = AnyRelation;
        bool outpost_only = false;
        GadgetState gadget_state = AnyGadget;
        GW::Constants::Profession profession = GW::Constants::Profession::None;
        int boss_state = 0;
        Color border_color = 0;
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

    struct CachedPolygon {
        const CustomRenderer::CustomPolygon* polygon = nullptr;
        float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;
    };
    struct CachedMarker {
        const CustomRenderer::CustomMarker* marker = nullptr;
        float radius_squared = 0.f;
    };
    void RefreshRelevantPolys();
    std::vector<CachedPolygon> relevant_polygons;
    std::vector<CachedMarker> relevant_markers;


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
    void RefreshMatches(const GW::Agent* agent);
    static void OnNameDecoded(void* context, const wchar_t* decoded);
    struct MatchCache {
        const GW::Agent* agent = nullptr;
        uint32_t generation = 0;
        uint32_t identifier = 0;
        uint32_t map_id = 0;
        uint32_t flags = 0;
        int allegiance = -1;
        GW::Constants::Profession profession = GW::Constants::Profession::None;
        AgentType type = Any;
        bool valid = false;
        uint32_t relation_flags = 0;
        bool name_requested = false;
        std::wstring name;
        std::vector<const CustomAgent*> matches;
    };
    std::unordered_map<uint32_t, MatchCache> match_cache;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> pending_names;
    uint32_t next_name_token = 0;
    bool rules_changed = true;


    Color color_agent_modifier = 0x001E1E1E;
    Color color_agent_damaged_modifier = 0x00505050;
    Color color_eoe = 0x3200FF00;
    Color color_qz = 0x320000FF;
    Color color_winnowing = 0x3200FFFF;
    Color color_frozen_soil = 0x00FEFFFF;
    Color color_symbiosis = 0x00FF00FF;
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

    static constexpr std::array<Color, 11> DefaultProfessionColors()
    {
        return {0xFF666666, 0xFFEEAA33, 0xFF55AA00, 0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                0xFFBB3333, 0xFFAA0088, 0xFF00AAAA, 0xFF996600, 0xFF7777CC};
    }
    std::array<Color, 11> profession_colors = DefaultProfessionColors();

    std::vector<CustomAgent*> custom_agents{};
    std::unordered_map<const CustomAgent*, TextUtils::SearchPattern<wchar_t>> compiled_name_patterns;
    bool check_friends = false;
    bool check_guild = false;
    bool check_party = false;
    void RebuildRuleMatchers();
    void SeedDefaultCustomAgents();
    void SeedAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy);
    bool custom_agent_defaults_seeded = false;
    bool appearance_defaults_seeded = false;

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
    Shape_e default_shape = Tear;
    Shape_e shape_player = Tear;
    Shape_e shape_players = Tear;

    bool custom_agents_loaded = false; // guards SaveCustomAgents against clobbering a file that was never read

    GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
};
