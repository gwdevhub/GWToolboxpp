#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Managers/AgentMgr.h>

namespace GW {

    namespace Constants {
        enum class SkillID : uint32_t;
        enum class Attribute : uint32_t;
        enum class Profession: uint32_t;
    }

    struct Skill;
    struct AttributeInfo;
    struct Skillbar;

    typedef Array<Skillbar> SkillbarArray;

    struct Module;
    extern Module SkillbarModule;

    namespace SkillbarMgr {
        struct Attribute {
            Constants::Attribute attribute = (Constants::Attribute)0xFF;
            uint32_t points{};
        };
        struct SkillTemplate {
            Constants::Profession primary{};
            Constants::Profession secondary{};
            uint32_t attributes_count;
            Constants::Attribute attribute_ids[12];
            uint32_t attribute_values[12];
            Constants::SkillID    skills[8]{};
        };
        static_assert(sizeof(SkillTemplate) == 140);

        // Handlers run in altitude order: <= 0 before the hooked function (may set status->blocked), > 0 after it.
        using NativeDecodeTemplateHeaderCallback = void (__cdecl *)(HookStatus*, SkillTemplate* out_template, void* reader, uint8_t* result, void* context);
        bool RegisterDecodeTemplateHeaderCallback(HookEntry* entry, NativeDecodeTemplateHeaderCallback callback, void* context = nullptr, int32_t altitude = -0x8000);

        using NativeGetAccountTemplateDataCallback = void (__cdecl *)(HookStatus*, uint32_t template_id, SkillTemplate* out_data, int32_t* out_error, int32_t* result, void* context);
        bool RegisterGetAccountTemplateDataCallback(HookEntry* entry, NativeGetAccountTemplateDataCallback callback, void* context = nullptr, int32_t altitude = -0x8000);

        using NativeUpdateTemplateDisplayCallback = void (__cdecl *)(HookStatus*, uint32_t* frame_data, void* context);
        bool RegisterUpdateTemplateDisplayCallback(HookEntry* entry, NativeUpdateTemplateDisplayCallback callback, void* context = nullptr, int32_t altitude = -0x8000);

        using NativeDecodeTemplateStringCallback = void (__cdecl *)(HookStatus*, const wchar_t* text, SkillTemplate* out_template, int32_t* out_error, uint8_t* result, void* context);
        bool RegisterDecodeTemplateStringCallback(HookEntry* entry, NativeDecodeTemplateStringCallback callback, void* context = nullptr, int32_t altitude = -0x8000);

        using NativeLoadSkillTemplateCallback = void (__cdecl *)(HookStatus*, uint32_t agent_id, SkillTemplate* skill_template, void* context);
        bool RegisterLoadSkillTemplateCallback(HookEntry* entry, NativeLoadSkillTemplateCallback callback, void* context = nullptr, int32_t altitude = -0x8000);

        void RemoveTemplateCallbackNative(HookEntry* entry);
#if !defined(GWCA_STDLIB)
        inline void RemoveTemplateCallback(HookEntry* entry) { RemoveTemplateCallbackNative(entry); }
#endif

        // Skill slot in the player's bar, or -1 if the skill is not there.
        int GetSkillSlot(Constants::SkillID skill_id);

        // Use Skill in slot (Slot) on (Agent), optionally call that you are using said skill.
        bool UseSkill(uint32_t slot, uint32_t target = 0);

        // Raw packet to use skill ID; unlike above, the skillbar client struct is not registered as casting.
        bool UseSkillByID(uint32_t skill_id, uint32_t target = 0);

        // Get skill structure of said id, houses pretty much everything you would want to know about the skill.
        Skill* GetSkillConstantData(Constants::SkillID skill_id);

        // Number of entries in the client's skill constant data array; can exceed Constants::SkillID::Count when GW adds skills.
        uint32_t GetSkillCount();

        // Name/Description/Profession etc for an attribute by id
        AttributeInfo* GetAttributeConstantData(Constants::Attribute attribute_id);

        // Get array of skillbars, [0] = player [1-7] = heroes.
        SkillbarArray* GetSkillbarArray();
        Skillbar* GetPlayerSkillbar();
        Skillbar* GetSkillbar(const uint32_t agent_id);
        Skillbar* GetHeroSkillbar(uint32_t hero_index);
        Skill* GetHoveredSkill();
        bool GetSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template);

        // Whether this skill is unlocked at account level, not necessarily learnt by the current character
        bool GetIsSkillUnlocked(Constants::SkillID skill_id);
        // Whether the current character has learnt this skill
        bool GetIsSkillLearnt(Constants::SkillID skill_id);

        bool ChangeSecondProfession(uint32_t agent_id, Constants::Profession profession);
        bool DecodeSkillTemplate(SkillTemplate& skill_template, const char *temp);
        bool EncodeSkillTemplate(const SkillTemplate& skill_template, char* result, uint32_t result_len);

        bool LoadSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template);

        inline bool LoadSkillTemplate(SkillTemplate& value) { return LoadSkillTemplate(Agents::GetControlledCharacterId(), value); }
        inline bool LoadSkillTemplate(uint32_t agent_id, const char* text)
        {
            SkillTemplate value{};
            return DecodeSkillTemplate(value, text) && LoadSkillTemplate(agent_id, value);
        }
        inline bool LoadSkillTemplate(const char* text) { return LoadSkillTemplate(Agents::GetControlledCharacterId(), text); }
        inline bool GetSkillTemplate(SkillTemplate& value) { return GetSkillTemplate(Agents::GetControlledCharacterId(), value); }
    }
}
// C Interop API

namespace GW::SkillbarMgr {
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_RegisterDecodeTemplateHeaderCallback(HookEntry* entry, NativeDecodeTemplateHeaderCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_RegisterGetAccountTemplateDataCallback(HookEntry* entry, NativeGetAccountTemplateDataCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_RegisterUpdateTemplateDisplayCallback(HookEntry* entry, NativeUpdateTemplateDisplayCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_RegisterDecodeTemplateStringCallback(HookEntry* entry, NativeDecodeTemplateStringCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_RegisterLoadSkillTemplateCallback(HookEntry* entry, NativeLoadSkillTemplateCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API void __cdecl GW_SkillbarMgr_RemoveTemplateCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API int __cdecl GW_SkillbarMgr_GetSkillSlot(Constants::SkillID skill_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_UseSkill(uint32_t slot, uint32_t target) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_UseSkillByID(uint32_t skill_id, uint32_t target) noexcept;
    extern "C" GWCA_API Skill* __cdecl GW_SkillbarMgr_GetSkillConstantData(Constants::SkillID skill_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_SkillbarMgr_GetSkillCount() noexcept;
    extern "C" GWCA_API AttributeInfo* __cdecl GW_SkillbarMgr_GetAttributeConstantData(Constants::Attribute attribute_id) noexcept;
    extern "C" GWCA_API SkillbarArray* __cdecl GW_SkillbarMgr_GetSkillbarArray() noexcept;
    extern "C" GWCA_API Skillbar* __cdecl GW_SkillbarMgr_GetPlayerSkillbar() noexcept;
    extern "C" GWCA_API Skillbar* __cdecl GW_SkillbarMgr_GetSkillbar(const uint32_t agent_id) noexcept;
    extern "C" GWCA_API Skillbar* __cdecl GW_SkillbarMgr_GetHeroSkillbar(uint32_t hero_index) noexcept;
    extern "C" GWCA_API Skill* __cdecl GW_SkillbarMgr_GetHoveredSkill() noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_GetSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_GetIsSkillUnlocked(Constants::SkillID skill_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_GetIsSkillLearnt(Constants::SkillID skill_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_ChangeSecondProfession(uint32_t agent_id, Constants::Profession profession) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_DecodeSkillTemplate(SkillTemplate& skill_template, const char* temp) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_EncodeSkillTemplate(const SkillTemplate& skill_template, char* result, uint32_t result_len) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SkillbarMgr_LoadSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::SkillbarMgr {
    inline bool RegisterDecodeTemplateHeaderCallback(HookEntry* entry, NativeDecodeTemplateHeaderCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RegisterDecodeTemplateHeaderCallback, entry, callback, context, altitude);
    }

    inline bool RegisterGetAccountTemplateDataCallback(HookEntry* entry, NativeGetAccountTemplateDataCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RegisterGetAccountTemplateDataCallback, entry, callback, context, altitude);
    }

    inline bool RegisterUpdateTemplateDisplayCallback(HookEntry* entry, NativeUpdateTemplateDisplayCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RegisterUpdateTemplateDisplayCallback, entry, callback, context, altitude);
    }

    inline bool RegisterDecodeTemplateStringCallback(HookEntry* entry, NativeDecodeTemplateStringCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RegisterDecodeTemplateStringCallback, entry, callback, context, altitude);
    }

    inline bool RegisterLoadSkillTemplateCallback(HookEntry* entry, NativeLoadSkillTemplateCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RegisterLoadSkillTemplateCallback, entry, callback, context, altitude);
    }

    inline void RemoveTemplateCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_RemoveTemplateCallbackNative, entry);
    }

    inline int GetSkillSlot(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillSlot, skill_id);
    }

    inline bool UseSkill(uint32_t slot, uint32_t target)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_UseSkill, slot, target);
    }

    inline bool UseSkillByID(uint32_t skill_id, uint32_t target)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_UseSkillByID, skill_id, target);
    }

    inline Skill* GetSkillConstantData(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillConstantData, skill_id);
    }

    inline uint32_t GetSkillCount()
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillCount);
    }

    inline AttributeInfo* GetAttributeConstantData(Constants::Attribute attribute_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetAttributeConstantData, attribute_id);
    }

    inline SkillbarArray* GetSkillbarArray()
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillbarArray);
    }

    inline Skillbar* GetPlayerSkillbar()
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetPlayerSkillbar);
    }

    inline Skillbar* GetSkillbar(const uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillbar, agent_id);
    }

    inline Skillbar* GetHeroSkillbar(uint32_t hero_index)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetHeroSkillbar, hero_index);
    }

    inline Skill* GetHoveredSkill()
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetHoveredSkill);
    }

    inline bool GetSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetSkillTemplate, agent_id, skill_template);
    }

    inline bool GetIsSkillUnlocked(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetIsSkillUnlocked, skill_id);
    }

    inline bool GetIsSkillLearnt(Constants::SkillID skill_id)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_GetIsSkillLearnt, skill_id);
    }

    inline bool ChangeSecondProfession(uint32_t agent_id, Constants::Profession profession)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_ChangeSecondProfession, agent_id, profession);
    }

    inline bool DecodeSkillTemplate(SkillTemplate& skill_template, const char* temp)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_DecodeSkillTemplate, skill_template, temp);
    }

    inline bool EncodeSkillTemplate(const SkillTemplate& skill_template, char* result, uint32_t result_len)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_EncodeSkillTemplate, skill_template, result, result_len);
    }

    inline bool LoadSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template)
    {
        return GWCA_CALL_C(GW_SkillbarMgr_LoadSkillTemplate, agent_id, skill_template);
    }
}
#endif
