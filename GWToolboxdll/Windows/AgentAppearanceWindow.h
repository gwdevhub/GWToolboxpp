#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <ToolboxWindow.h>
#include <Color.h>
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

class AgentAppearanceWindow : public ToolboxWindow {
    friend class AgentRenderer;
    AgentAppearanceWindow() = default;
    ~AgentAppearanceWindow() override = default;

public:
    static AgentAppearanceWindow& Instance()
    {
        static AgentAppearanceWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Agent Appearance"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PALETTE; }
    void Show();
    void Draw(IDirect3DDevice9* device) override;

    static bool AppearanceRulesLoaded();

    static void DrawSettings();
    static void DrawRuleEditor();
    static void RegisterSettings(ToolboxModule* module);
    static void RegisterMinimapSettings(ToolboxModule* module);
    static void LoadCustomAgents(SettingsDoc& doc, ToolboxIni* legacy);
    static void SaveCustomAgents(SettingsDoc& doc);
    static bool ApplyNameTagColor(const GW::Agent* agent, Color& color);
    static void InvalidateAppearance(uint32_t agent_id);
    static void ReleaseAppearanceHooks();

    static void LoadDefaultColors();
    static void LoadDefaultSizes();
    static void ResetAppearanceSettings();
    static void LoadLegacyAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy);

    static Color GetProfessionColor(GW::Constants::Profession profession);

    enum Shape_e { Shape_None = -1, Tear, Circle, Quad, BigCircle, Star };

    inline static bool show_hidden_npcs = false;
    inline static bool show_quest_npcs_on_minimap = false;
    inline static bool show_props_on_minimap = false;
    inline static bool enemies_colors_by_profession = true;
    inline static bool only_color_bosses = true;
    inline static float agent_border_thickness = 0.f;
    inline static float target_border_thickness = 50.f;

    inline static uint32_t auto_target_id = 0;

private:
    bool pending_focus = false;

    enum CombatState { InCombat, NotInCombat, EitherCombat };
    enum WeaponState { HasWeapon, NoWeapon, EitherWeapon };
    enum DeadState { Dead, Alive, EitherDeadState };
    enum QuestState { QuestGiver, NotQuestGiver, EitherQuestState };
    enum AgentType { Any = 1, Item, Gadget, NPC, Player };
    enum TargetState { EitherTarget, Targeted, NotTargeted, Marked };
    enum PlayerRelation { AnyRelation, Self, Other, Friend, Guild, MyParty, InParty };
    enum GadgetState { AnyGadget, ClosedChest, OpenedChest, OtherGadget };

public:
    class CustomAgent {
        static unsigned int cur_ui_id;

    public:
        struct AgentTypeOption {
            const char* label;
            AgentType type;
            PlayerRelation relation;
            GadgetState gadget;
        };

        inline static constexpr AgentTypeOption agent_type_options[] = {
            {"Any", Any, AnyRelation, AnyGadget},
            {"Item", Item, AnyRelation, AnyGadget},
            {"Gadget", Gadget, AnyRelation, AnyGadget},
            {"Gadget (Locked chest, closed)", Gadget, AnyRelation, ClosedChest},
            {"Gadget (Locked chest, opened)", Gadget, AnyRelation, OpenedChest},
            {"Gadget (Other)", Gadget, AnyRelation, OtherGadget},
            {"NPC", NPC, AnyRelation, AnyGadget},
            {"Player", Player, AnyRelation, AnyGadget},
            {"Player (Self)", Player, Self, AnyGadget},
            {"Player (Other player)", Player, Other, AnyGadget},
            {"Player (Friend)", Player, Friend, AnyGadget},
            {"Player (Guild member)", Player, Guild, AnyGadget},
            {"Player (My party)", Player, MyParty, AnyGadget},
            {"Player (In a party)", Player, InParty, AnyGadget}
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
            uint32_t dead_states = UINT32_MAX;
            uint32_t quest_states = UINT32_MAX;
            Colors::SettingColor color = 0;
            Colors::SettingColor color_text = 0;
            int shape = Shape_None;
            float size = 0.0f;
            int agent_type = 0;
            DWORD identifier = 0;
            std::string match_name;
            int target_state = EitherTarget;
            int player_relation = AnyRelation;
            bool outpost_only = false;
            int gadget_state = AnyGadget;
            int profession = 0;
            int boss_state = 0;
            uint32_t boss_states = UINT32_MAX;
            Colors::SettingColor border_color = 0;
            std::optional<bool> override_color;
            std::optional<bool> override_text_color;
            std::optional<bool> override_border_color;
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
        bool DrawSettings();
        [[nodiscard]] const char* AgentTypeName() const;
        [[nodiscard]] std::string DefaultLabel() const;
        [[nodiscard]] std::string Label() const;
        [[nodiscard]] Settings ToSettings() const;
        void ApplyLegacyFlags(const LegacyFlags& flags);

        const unsigned int ui_id = 0;
        size_t index = 0;
        bool edit_open = false;
        bool focus_editor = false;

        bool active = true;
        char name[128]{};
        char group[64]{};
        DWORD modelId = 0;
        DWORD mapId = 0;
        CombatState combat_state = CombatState::EitherCombat;
        WeaponState weapon_state = WeaponState::EitherWeapon;
        int allegiance = -1;
        uint32_t dead_states = 0;
        uint32_t quest_states = 0;

        Color color = 0;
        Color color_text = 0;
        bool override_color = false;
        bool override_text_color = false;
        bool override_border_color = false;
        Shape_e shape = Shape_None;
        float size = 0.0f;
        AgentType agent_type = NPC;
        DWORD identifier = 0;
        char match_name[128]{};
        TargetState target_state = EitherTarget;
        PlayerRelation player_relation = AnyRelation;
        bool outpost_only = false;
        GadgetState gadget_state = AnyGadget;
        GW::Constants::Profession profession = GW::Constants::Profession::None;
        uint32_t boss_states = 0;
        Color border_color = 0;
    };

    using AppearanceRule = CustomAgent;

    static void GetAgentAppearanceRules(std::vector<AppearanceRule*>& out);
    static const std::vector<const AppearanceRule*>* GetAppearanceRules(const GW::Agent* agent);
    static bool GetAgentAppearance(const GW::Agent* agent, Shape_e* shape_out = nullptr, Color* color_out = nullptr,
        Color* border_color_out = nullptr, float* border_thickness_out = nullptr, Color* text_color_out = nullptr, float* size_out = nullptr);
    static Shape_e GetShape(const GW::Agent* agent);
    static Color GetColor(const GW::Agent* agent);
    static float GetSize(const GW::Agent* agent);
    static bool IsMarked(uint32_t agent_id);
    static void ResetAppearanceCache();

private:
    static void EditRule(CustomAgent* rule);

    static Color GetDefaultColor(const GW::Agent* agent, const CustomAgent* ca = nullptr);
    static float GetDefaultSize(const GW::Agent* agent);
    static Shape_e GetDefaultShape(const GW::Agent* agent);

    struct CachedPolygon {
        const CustomRenderer::CustomPolygon* polygon = nullptr;
        float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;
    };
    struct CachedMarker {
        const CustomRenderer::CustomMarker* marker = nullptr;
        float radius_squared = 0.f;
    };
    static void RefreshRelevantPolys();
    inline static std::vector<CachedPolygon> relevant_polygons;
    inline static std::vector<CachedMarker> relevant_markers;


    static void RefreshMatches(const GW::Agent* agent);
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
    inline static std::unordered_map<uint32_t, MatchCache> match_cache;
    inline static std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> pending_names;
    inline static uint32_t next_name_token = 0;
    inline static bool rules_changed = true;


    inline static Color color_agent_modifier = 0x001E1E1E;
    inline static Color color_agent_damaged_modifier = 0x00505050;
    inline static Color color_eoe = 0x3200FF00;
    inline static Color color_qz = 0x320000FF;
    inline static Color color_winnowing = 0x3200FFFF;
    inline static Color color_frozen_soil = 0x00FEFFFF;
    inline static Color color_symbiosis = 0x00FF00FF;
    inline static Color color_target = 0xFFFFFF00;
    inline static Color color_player = 0xFFFF8000;
    inline static Color color_player_dead = 0x64FF8000;
    inline static Color color_signpost = 0xFF0000C8;
    inline static Color color_locked_chest = 0xFF0000C8;
    inline static Color color_locked_chest_open = 0xFF0000C8;
    inline static Color color_item = 0xFF0000F0;
    inline static Color color_hostile = 0xFFF00000;
    inline static Color color_hostile_dead = 0xFF320000;
    inline static Color color_neutral = 0xFF0000DC;
    inline static Color color_ally = 0xFF00B300;
    inline static Color color_ally_npc = 0xFF99FF99;
    inline static Color color_ally_npc_quest = 0xFF99FF99;
    inline static Color color_ally_spirit = 0xFF608000;
    inline static Color color_ally_minion = 0xFF008060;
    inline static Color color_ally_dead = 0x64006400;
    inline static Color color_marked_target = 0xFFFFFC00;

    static constexpr std::array<Color, 11> DefaultProfessionColors()
    {
        return {0xFF666666, 0xFFEEAA33, 0xFF55AA00, 0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                0xFFBB3333, 0xFFAA0088, 0xFF00AAAA, 0xFF996600, 0xFF7777CC};
    }
    inline static std::array<Color, 11> profession_colors = DefaultProfessionColors();

    inline static std::vector<CustomAgent*> custom_agents{};
    inline static std::unordered_map<const CustomAgent*, TextUtils::SearchPattern<wchar_t>> compiled_name_patterns;
    inline static bool check_friends = false;
    inline static bool check_guild = false;
    inline static bool check_party = false;
    static void RebuildRuleMatchers();
    static void SeedDefaultCustomAgents();
    static void SeedAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy);
    inline static bool custom_agent_defaults_seeded = false;
    inline static bool appearance_defaults_seeded = false;

    inline static float size_default = 100.f;
    inline static float size_player = 100.f;
    inline static float size_signpost = 50.f;
    inline static float size_locked_chest = 50.f;
    inline static float size_locked_chest_open = 50.f;
    inline static float size_item = 25.f;
    inline static float size_boss = 125.f;
    inline static float size_minion = 50.f;
    inline static float size_marked_target = 100.f;
    inline static float size_hostile = 100.f;
    inline static float size_neutral = 100.f;
    inline static float size_ally = 100.f;
    inline static float size_ally_npc = 100.f;
    inline static float size_ally_npc_quest = 100.f;
    inline static float size_ally_spirit = 100.f;
    inline static Shape_e default_shape = Tear;
    inline static Shape_e shape_player = Tear;
    inline static Shape_e shape_players = Tear;

    inline static bool custom_agents_loaded = false;

    inline static GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
};

using AppearanceRule = AgentAppearanceWindow::AppearanceRule;
