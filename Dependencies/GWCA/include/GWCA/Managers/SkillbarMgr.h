#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

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

        // Get the skill slot in the player bar of the player.
        // Returns -1 if the skill is not there
        GWCA_API int GetSkillSlot(Constants::SkillID skill_id);

        // Use Skill in slot (Slot) on (Agent), optionally call that you are using said skill.
        GWCA_API bool UseSkill(uint32_t slot, uint32_t target = 0);

        // Send raw packet to use skill with ID (SkillID).
        // Same as above except the skillbar client struct will not be registered as casting.
        GWCA_API bool UseSkillByID(uint32_t skill_id, uint32_t target = 0);

        // Get skill structure of said id, houses pretty much everything you would want to know about the skill.
        GWCA_API Skill* GetSkillConstantData(Constants::SkillID skill_id);

        // Name/Description/Profession etc for an attribute by id
        GWCA_API AttributeInfo* GetAttributeConstantData(Constants::Attribute attribute_id);

        // Get array of skillbars, [0] = player [1-7] = heroes.
        GWCA_API SkillbarArray* GetSkillbarArray();
        GWCA_API Skillbar* GetPlayerSkillbar();
        GWCA_API Skillbar* GetSkillbar(const uint32_t agent_id);
        GWCA_API Skillbar* GetHeroSkillbar(uint32_t hero_index);
        GWCA_API Skill* GetHoveredSkill();
        GWCA_API bool GetSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template);

        // Whether this skill is unlocked at account level, not necessarily learnt by the current character
        GWCA_API bool GetIsSkillUnlocked(Constants::SkillID skill_id);
        // Whether the current character has learnt this skill
        GWCA_API bool GetIsSkillLearnt(Constants::SkillID skill_id);

        GWCA_API bool ChangeSecondProfession(uint32_t agent_id, Constants::Profession profession);
        GWCA_API bool DecodeSkillTemplate(SkillTemplate& skill_template, const char *temp);
        GWCA_API bool EncodeSkillTemplate(const SkillTemplate& skill_template, char* result, size_t result_len);

        
        GWCA_API bool LoadSkillTemplate(uint32_t agent_id, const char *temp);
        GWCA_API bool LoadSkillTemplate(uint32_t agent_id, SkillTemplate& skill_template);

        GWCA_API bool LoadSkillTemplate(SkillTemplate& skill_template);
        GWCA_API bool LoadSkillTemplate(const char* temp);
        GWCA_API bool GetSkillTemplate(SkillTemplate& skill_template);
    }
}
