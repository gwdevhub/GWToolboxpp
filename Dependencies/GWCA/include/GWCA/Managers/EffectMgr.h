#pragma once
#include <GWCA/Runtime.h>

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
        // Current intoxication level, 0-5; above 0 makes drunk-benefit skills work. Requires SetupPostProcessingEffectHook().
        uint32_t GetAlcoholLevel();

        // Have fun with this ;))))))))))
        void GetDrunkAf(float intensity, uint32_t tint);

        // Get full array of effects and buffs for whole party.
        AgentEffectsArray* GetPartyEffectsArray();

        // Get full array of effects and buffs for agent.
        AgentEffects* GetAgentEffectsArray(uint32_t agent_id);

        // Get full array of effects and buffs for player
        AgentEffects* GetPlayerEffectsArray();

        // Get array of effects on the agent.
        EffectArray* GetAgentEffects(uint32_t agent_id);

        // Get array of buffs on the player.
        BuffArray* GetAgentBuffs(uint32_t agent_id);

        // Get array of effects on the player.
        EffectArray* GetPlayerEffects();

        // Get array of buffs on the player.
        BuffArray* GetPlayerBuffs();

        // Drop buffid buff.
        bool DropBuff(uint32_t buff_id);

        // Gets effect struct of effect on player with SkillID, returns Effect::Nil() if no match.
        Effect * GetPlayerEffectBySkillId(Constants::SkillID skill_id);

        // Gets Buff struct of Buff on player with SkillID, returns Buff::Nil() if no match.
        Buff *GetPlayerBuffBySkillId(Constants::SkillID skill_id);
    };
}
// C Interop API

namespace GW::Effects {
    extern "C" GWCA_API uint32_t __cdecl GW_Effects_GetAlcoholLevel() noexcept;
    extern "C" GWCA_API void __cdecl GW_Effects_GetDrunkAf(float intensity, uint32_t tint) noexcept;
    extern "C" GWCA_API AgentEffectsArray* __cdecl GW_Effects_GetPartyEffectsArray() noexcept;
    extern "C" GWCA_API AgentEffects* __cdecl GW_Effects_GetAgentEffectsArray(uint32_t agent_id) noexcept;
    extern "C" GWCA_API AgentEffects* __cdecl GW_Effects_GetPlayerEffectsArray() noexcept;
    extern "C" GWCA_API EffectArray* __cdecl GW_Effects_GetAgentEffects(uint32_t agent_id) noexcept;
    extern "C" GWCA_API BuffArray* __cdecl GW_Effects_GetAgentBuffs(uint32_t agent_id) noexcept;
    extern "C" GWCA_API EffectArray* __cdecl GW_Effects_GetPlayerEffects() noexcept;
    extern "C" GWCA_API BuffArray* __cdecl GW_Effects_GetPlayerBuffs() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Effects_DropBuff(uint32_t buff_id) noexcept;
    extern "C" GWCA_API Effect* __cdecl GW_Effects_GetPlayerEffectBySkillId(Constants::SkillID skill_id) noexcept;
    extern "C" GWCA_API Buff* __cdecl GW_Effects_GetPlayerBuffBySkillId(Constants::SkillID skill_id) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Effects {
    inline uint32_t GetAlcoholLevel()
    {
        return GWCA_CALL_C(GW_Effects_GetAlcoholLevel);
    }

    inline void GetDrunkAf(float intensity, uint32_t tint)
    {
        return GWCA_CALL_C(GW_Effects_GetDrunkAf, intensity, tint);
    }

    inline AgentEffectsArray* GetPartyEffectsArray()
    {
        return GWCA_CALL_C(GW_Effects_GetPartyEffectsArray);
    }

    inline AgentEffects* GetAgentEffectsArray(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Effects_GetAgentEffectsArray, agent_id);
    }

    inline AgentEffects* GetPlayerEffectsArray()
    {
        return GWCA_CALL_C(GW_Effects_GetPlayerEffectsArray);
    }

    inline EffectArray* GetAgentEffects(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Effects_GetAgentEffects, agent_id);
    }

    inline BuffArray* GetAgentBuffs(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Effects_GetAgentBuffs, agent_id);
    }

    inline EffectArray* GetPlayerEffects()
    {
        return GWCA_CALL_C(GW_Effects_GetPlayerEffects);
    }

    inline BuffArray* GetPlayerBuffs()
    {
        return GWCA_CALL_C(GW_Effects_GetPlayerBuffs);
    }

    inline bool DropBuff(uint32_t buff_id)
    {
        return GWCA_CALL_C(GW_Effects_DropBuff, buff_id);
    }

    inline Effect* GetPlayerEffectBySkillId(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_Effects_GetPlayerEffectBySkillId, skill_id);
    }

    inline Buff* GetPlayerBuffBySkillId(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_Effects_GetPlayerBuffBySkillId, skill_id);
    }
}
#endif
