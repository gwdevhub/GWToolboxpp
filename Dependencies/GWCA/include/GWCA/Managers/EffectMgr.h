#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {

    struct Buff;
    struct Effect;
    struct AgentEffects;

    typedef Array<Buff> BuffArray;
    typedef Array<Effect> EffectArray;
    typedef Array<AgentEffects> AgentEffectsArray;

    namespace Constants {
        enum class SkillID : uint32_t;
    }

    struct Module;
    extern Module EffectModule;

    namespace Effects {
        // Returns current level of intoxication, 0-5 scale.
        // If > 0 then skills that benefit from drunk will work.
        // Important: requires SetupPostProcessingEffectHook() above.
        GWCA_API uint32_t GetAlcoholLevel();

        // Have fun with this ;))))))))))
        GWCA_API void GetDrunkAf(float intensity, uint32_t tint);

        // Get full array of effects and buffs for whole party.
        GWCA_API AgentEffectsArray* GetPartyEffectsArray();

        // Get full array of effects and buffs for agent.
        GWCA_API AgentEffects* GetAgentEffectsArray(uint32_t agent_id);

        // Get full array of effects and buffs for player
        GWCA_API AgentEffects* GetPlayerEffectsArray();

        // Get array of effects on the agent.
        GWCA_API EffectArray* GetAgentEffects(uint32_t agent_id);

        // Get array of buffs on the player.
        GWCA_API BuffArray* GetAgentBuffs(uint32_t agent_id);

        // Get array of effects on the player.
        GWCA_API EffectArray* GetPlayerEffects();

        // Get array of buffs on the player.
        GWCA_API BuffArray* GetPlayerBuffs();

        // Drop buffid buff.
        GWCA_API bool DropBuff(uint32_t buff_id);

        // Gets effect struct of effect on player with SkillID, returns Effect::Nil() if no match.
        GWCA_API Effect * GetPlayerEffectBySkillId(Constants::SkillID skill_id);

        // Gets Buff struct of Buff on player with SkillID, returns Buff::Nil() if no match.
        GWCA_API Buff *GetPlayerBuffBySkillId(Constants::SkillID skill_id);
    };
}
