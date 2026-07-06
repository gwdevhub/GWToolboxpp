#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <Color.h>

class SettingsDoc;
class ToolboxIni;

// Shared tracking of hostile ground AoE effects (Meteor Shower, Maelstrom, traps, ...), fed by
// StoC packets. Single source of truth for the minimap EffectRenderer and the in-world
// DangerRingsModule; tracking runs regardless of whether either consumer is visible.
namespace AoeEffects {
    struct EffectSettings {
        Color color = 0xFFFF0000;
        std::string name;
        uint32_t effect_id = 0;
        float range = 0.f;
        uint32_t stoc_header = 0;
        uint32_t duration = 10000;
    };

    struct ActiveEffect {
        uint64_t uid = 0;
        uint32_t effect_id = 0;
        GW::Vec2f pos;
        uint32_t zplane = 0;
        clock_t start = 0;
        uint32_t duration = 0;
        float range = 0.f;
        Color color = 0;      // per-skill colour (minimap)
        bool from_enemy = true; // cast by an enemy (vs one of our own) - in-world danger rings colour by this
    };

    // Allegiance colours (ARGB), shared by the in-world danger rings and the minimap circles: enemy AoE
    // is drawn in color_enemy, our own in color_ally. Alpha 0 hides that side (ally is hidden by default).
    extern Color color_enemy;
    extern Color color_ally;

    // Idempotent; registers the packet callbacks on first call.
    void Initialize();
    void Terminate();

    void LoadDefaults();
    std::unordered_map<uint32_t, EffectSettings*>& GetEffectSettings();

    // Copy the active (non-expired) effects into out; expired entries are pruned.
    void GetActiveEffects(std::vector<ActiveEffect>& out);
    void Clear();

    // The enemy/ally colour pickers, shared by every settings page that exposes these colours.
    // Returns true if a colour was changed this frame.
    bool DrawColorSettings();

    // Persist the two allegiance colours (called from the minimap's settings section).
    void LoadColorSettings(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section);
    void SaveColorSettings(SettingsDoc& doc, const char* section);
}
