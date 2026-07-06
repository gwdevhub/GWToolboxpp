#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <Color.h>

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
        Color color = 0;
    };

    // Idempotent; registers the packet callbacks on first call.
    void Initialize();
    void Terminate();

    void LoadDefaults();
    std::unordered_map<uint32_t, EffectSettings*>& GetEffectSettings();

    // Copy the active (non-expired) effects into out; expired entries are pruned.
    void GetActiveEffects(std::vector<ActiveEffect>& out);
    void Clear();

    // The per-skill colour pickers + restore-defaults button, shared by every settings page that
    // exposes these colours. Persistence stays with the minimap section (EffectRenderer).
    void DrawColorSettings();
}
