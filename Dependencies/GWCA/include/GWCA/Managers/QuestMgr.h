#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {

    struct Quest;
    typedef Array<Quest> QuestLog;

    namespace Constants {
        enum class QuestID : uint32_t;
        enum class Profession: uint32_t;
    }

    struct Module;
    extern Module QuestModule;

    namespace QuestMgr {

        GWCA_API GW::Constants::QuestID GetActiveQuestId();

        GWCA_API bool SetActiveQuestId(Constants::QuestID quest_id);

        GWCA_API Quest* GetActiveQuest();

        GWCA_API bool SetActiveQuest(Quest* quest);

        GWCA_API bool AbandonQuest(Quest* quest);

        GWCA_API bool AbandonQuestId(Constants::QuestID quest_id);

        GWCA_API QuestLog* GetQuestLog();

        GWCA_API Quest* GetQuest(GW::Constants::QuestID);

        // Find and populate a wchar_t* buffer with the encoded name of the category the quest belongs to in the quest log. Returns false on failure.
        GWCA_API bool GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len);

        GWCA_API bool RequestQuestInfo(const Quest* quest, bool update_markers=false);
        
        GWCA_API bool RequestQuestInfoId(Constants::QuestID quest_id, bool update_markers=false);

    };
}
