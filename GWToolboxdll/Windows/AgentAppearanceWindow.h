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
    inline static bool show_props_on_minimap = false;
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
            std::optional<float> scale;
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
            bool stop_processing_rules = false;
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
        bool stop_processing_rules = false;
        Shape_e shape = Shape_None;
        float scale = 0.0f;
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
        Color* border_color_out = nullptr, float* border_thickness_out = nullptr, Color* text_color_out = nullptr, float* size_out = nullptr,
        std::vector<const AppearanceRule*>* matched_rules_out = nullptr);
    static Shape_e GetShape(const GW::Agent* agent);
    static Color GetColor(const GW::Agent* agent);
    static float GetSize(const GW::Agent* agent);
    static bool IsMarked(uint32_t agent_id);
    static void ResetAppearanceCache();
    static void RebuildRuleMatchers();

private:
    static void EditRule(CustomAgent* rule);

    static float GetBaseSize();
    static float ScaleFromAbsolute(float size);
    static Color LegacyRuleColor(const char* key, Color preset);
    static float LegacyRuleScale(const char* key, float preset);
    static Shape_e LegacyRuleShape(const char* key, Shape_e preset);


    static void RefreshMatches(const GW::Agent* agent);
    static void EnsureRuleMatchers();
    static void OnNameDecoded(void* context, const wchar_t* decoded);
    enum RuleCheck : uint32_t {
        CheckIdentifier = 1u,
        CheckProfession = 2u,
        CheckGadget = 4u,
        CheckSelf = 8u,
        CheckFriend = 16u,
        CheckGuild = 32u,
        CheckMyParty = 64u,
        CheckAnyParty = 128u,
        CheckName = 256u
    };
    struct CompiledRule {
        const AppearanceRule* rule = nullptr;
        uint32_t required_flags = 0;
        uint32_t rejected_flags = 0;
        uint32_t required_relations = 0;
        uint32_t rejected_relations = 0;
        uint32_t identifier = 0;
        const TextUtils::SearchPattern<wchar_t>* name_pattern = nullptr;
    };
    struct RuleBucket {
        std::vector<CompiledRule> rules;
        uint32_t checks = 0;
    };
    static std::array<RuleBucket, 6> rule_buckets;
    inline static std::array<const AppearanceRule*, 11> profession_rules{};
    inline static uint32_t matcher_map_id = UINT32_MAX;
    inline static uint32_t matcher_instance_type = UINT32_MAX;
    inline static bool matcher_context_valid = false;
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


    inline static Color color_agent_modifier = 0x001E1E1E;
    inline static Color color_agent_damaged_modifier = 0x00505050;
    inline static Color color_default = 0xFFFFFFFF;
    inline static std::unordered_map<std::string, Color> legacy_rule_colors;
    inline static std::unordered_map<std::string, float> legacy_rule_sizes;
    inline static std::unordered_map<std::string, float> legacy_rule_scales;
    inline static std::unordered_map<std::string, Shape_e> legacy_rule_shapes;
    inline static bool legacy_enemy_profession_colors = true;
    inline static bool legacy_boss_colors_only = true;

    static constexpr std::array<Color, 11> DefaultProfessionColors()
    {
        return {0xFF666666, 0xFFEEAA33, 0xFF55AA00, 0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                0xFFBB3333, 0xFFAA0088, 0xFF00AAAA, 0xFF996600, 0xFF7777CC};
    }

    inline static std::vector<CustomAgent*> custom_agents{};
    inline static std::unordered_map<const CustomAgent*, TextUtils::SearchPattern<wchar_t>> compiled_name_patterns;
    static void SeedDefaultCustomAgents();
    static void SeedAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy);
    static void SeedSpiritDefaults(const SettingsDoc& doc);
    inline static bool custom_agent_defaults_seeded = false;
    inline static bool appearance_defaults_seeded = false;
    inline static bool spirit_defaults_seeded = false;

    inline static float size_default = 100.f;
    inline static Shape_e default_shape = Tear;

    inline static bool custom_agents_loaded = false;

    inline static GW::HookEntry UIMsg_Entry;
    static void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
};

using AppearanceRule = AgentAppearanceWindow::AppearanceRule;
