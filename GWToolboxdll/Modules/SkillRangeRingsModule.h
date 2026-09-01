#pragma once

#include <ToolboxModule.h>

class SkillRangeRingsModule : public ToolboxModule {
    SkillRangeRingsModule() = default;
    ~SkillRangeRingsModule() override = default;

public:
    static SkillRangeRingsModule& Instance()
    {
        static SkillRangeRingsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "技能范围"; }
    [[nodiscard]] const char* Description() const override
    {
        return "显示悬停技能的影响力范围 (AoE半径, 耳语范围, 精灵范围) 作为游戏世界中的圆环。";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CIRCLE_NOTCH; }

    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    // Force rings as if hovering the given skill id (0 = back to real hover). Test harness hook.
    static void SetDebugSkill(uint32_t skill_id);
    // Describes the rings the module would draw for a skill id into `buf` ("156:aoe@target,...");
    // returns the ring count (harness `skillinfo` verification).
    static size_t DebugSpecs(uint32_t skill_id, char* buf, size_t len);

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawInWorld(IDirect3DDevice9* device);
};
