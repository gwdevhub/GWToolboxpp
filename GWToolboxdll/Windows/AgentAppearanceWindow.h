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
    static void LoadCustomAgents(const SettingsDoc& doc);
    static void SaveCustomAgents(SettingsDoc& doc);
    static bool ApplyNameTagColor(const GW::Agent* agent, Color& color);
    static void InvalidateAppearance(uint32_t agent_id);
    static void ReleaseAppearanceHooks();

    static void ResetAppearanceSettings();

    static Color GetProfessionColor(GW::Constants::Profession profession);

    enum Shape_e { Shape_None = -1, Tear, Circle, Quad, BigCircle, Star };

    static constexpr size_t LayerCount = 5;

    struct MarkerAppearance {
        Shape_e shape = Shape_None;
        Color color = 0;
        Color border_color = 0;
        float border_thickness = 0.f;
        float size = 0.f;
    };

    struct AgentAppearance {
        std::array<MarkerAppearance, LayerCount> layers{};
        uint8_t active_layers = 0;
        std::optional<Color> name_tag_color;
    };

    inline static bool show_hidden_npcs = false;
    inline static bool show_props_on_minimap = false;
    inline static uint32_t auto_target_id = 0;

    enum CombatState { InCombat, NotInCombat, EitherCombat };
    enum WeaponState { HasWeapon, NoWeapon, EitherWeapon };
    enum DeadState { Dead, Alive, EitherDeadState };
    enum QuestState { QuestGiver, NotQuestGiver, EitherQuestState };
    enum AgentType { Any = 1, Item, Gadget, NPC, Player };
    enum TargetState { EitherTarget, Targeted, NotTargeted, Marked };
    enum PlayerRelation { AnyRelation, Self, Other, Friend, Guild, MyParty, InParty };
    enum GadgetState { AnyGadget, ClosedChest, OpenedChest, OtherGadget };

    struct RuleSettings {
        bool active = true;
        std::string name;
        std::string group;
        std::vector<uint32_t> map_ids;
        CombatState combat_state = EitherCombat;
        WeaponState weapon_state = EitherWeapon;
        int allegiance = -1;
        uint32_t dead_states = 0;
        uint32_t quest_states = 0;
        Colors::SettingColor color = 0;
        Colors::SettingColor color_text = 0;
        Shape_e shape = Shape_None;
        float scale = 0.f;
        AgentType agent_type = NPC;
        uint32_t identifier = 0;
        std::string match_name;
        TargetState target_state = EitherTarget;
        PlayerRelation player_relation = AnyRelation;
        bool outpost_only = false;
        GadgetState gadget_state = AnyGadget;
        GW::Constants::Profession profession = GW::Constants::Profession::None;
        uint32_t boss_states = 0;
        Colors::SettingColor border_color = 0;
        float border_thickness = 0.f;
        bool override_color = false;
        bool override_text_color = false;
        bool override_border_color = false;
        bool stop_processing_rules = false;
        int layer = 0;
    };

    class CustomAgent : public RuleSettings {
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

        CustomAgent();
        explicit CustomAgent(const RuleSettings& settings);

        bool DrawHeader();
        bool DrawSettings();
        [[nodiscard]] const char* AgentTypeName() const;
        [[nodiscard]] std::string DefaultLabel() const;
        [[nodiscard]] std::string Label() const;

        const unsigned int ui_id = 0;
        size_t index = 0;
        bool edit_open = false;
        bool focus_editor = false;
    };

    using AppearanceRule = CustomAgent;

    static const std::vector<const AppearanceRule*>* GetAppearanceRules(const GW::Agent* agent);
    static void GetAgentAppearance(const GW::Agent* agent, AgentAppearance& appearance);
    static bool IsMarked(uint32_t agent_id);
    static void ResetAppearanceCache();
    static void RebuildRuleMatchers();
    static float GetBaseSize();
    static Shape_e GetDefaultShape();
    static Color GetDefaultColor();
    static Color GetAgentModifierColor();
};

using AppearanceRule = AgentAppearanceWindow::AppearanceRule;
