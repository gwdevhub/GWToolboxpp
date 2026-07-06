#pragma once

#include <ToolboxModule.h>

// While a skill is hovered (skillbar, skills window, templates...), drapes its relevant range
// rings onto the terrain via the shared GameWorldCompositor: cast/use range around the player,
// AoE radius around the current target (or the player), earshot for shouts/chants, and the
// constant-effect radius (spirit range) where present.
class SkillRangeRingsModule : public ToolboxModule {
    SkillRangeRingsModule() = default;
    ~SkillRangeRingsModule() override = default;

public:
    static SkillRangeRingsModule& Instance()
    {
        static SkillRangeRingsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Skill Range Rings"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Shows a hovered skill's area of effect (AoE radius, earshot, spirit range) as a ring on the ground in the game world.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CIRCLE_NOTCH; }

    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    // Force rings as if hovering the given skill id (0 = back to real hover). Test harness hook.
    static void SetDebugSkill(uint32_t skill_id);

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawInWorld(IDirect3DDevice9* device);
};
