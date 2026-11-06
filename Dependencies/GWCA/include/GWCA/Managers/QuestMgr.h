#pragma once
#include <GWCA/Runtime.h>

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

        GW::Constants::QuestID GetActiveQuestId();

        bool SetActiveQuestId(Constants::QuestID quest_id);

        Quest* GetActiveQuest();

        bool SetActiveQuest(Quest* quest);

        bool AbandonQuest(Quest* quest);

        bool AbandonQuestId(Constants::QuestID quest_id);

        QuestLog* GetQuestLog();

        Quest* GetQuest(GW::Constants::QuestID);

        // Find and populate a wchar_t* buffer with the encoded name of the category the quest belongs to in the quest log. Returns false on failure.
        bool GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len);

        bool RequestQuestInfo(const Quest* quest, bool update_markers=false);
        
        bool RequestQuestInfoId(Constants::QuestID quest_id, bool update_markers=false);

    };
}
// C Interop API

namespace GW::QuestMgr {
    extern "C" GWCA_API GW::Constants::QuestID __cdecl GW_QuestMgr_GetActiveQuestId() noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_SetActiveQuestId(Constants::QuestID quest_id) noexcept;
    extern "C" GWCA_API Quest* __cdecl GW_QuestMgr_GetActiveQuest() noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_SetActiveQuest(Quest* quest) noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_AbandonQuest(Quest* quest) noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_AbandonQuestId(Constants::QuestID quest_id) noexcept;
    extern "C" GWCA_API QuestLog* __cdecl GW_QuestMgr_GetQuestLog() noexcept;
    extern "C" GWCA_API Quest* __cdecl GW_QuestMgr_GetQuest(GW::Constants::QuestID gwca_arg0) noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len) noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_RequestQuestInfo(const Quest* quest, bool update_markers) noexcept;
    extern "C" GWCA_API bool __cdecl GW_QuestMgr_RequestQuestInfoId(Constants::QuestID quest_id, bool update_markers) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::QuestMgr {
    inline GW::Constants::QuestID GetActiveQuestId()
    {
        return GWCA_CALL_C(GW_QuestMgr_GetActiveQuestId);
    }

    inline bool SetActiveQuestId(Constants::QuestID quest_id)
    {
        return GWCA_CALL_C(GW_QuestMgr_SetActiveQuestId, quest_id);
    }

    inline Quest* GetActiveQuest()
    {
        return GWCA_CALL_C(GW_QuestMgr_GetActiveQuest);
    }

    inline bool SetActiveQuest(Quest* quest)
    {
        return GWCA_CALL_C(GW_QuestMgr_SetActiveQuest, quest);
    }

    inline bool AbandonQuest(Quest* quest)
    {
        return GWCA_CALL_C(GW_QuestMgr_AbandonQuest, quest);
    }

    inline bool AbandonQuestId(Constants::QuestID quest_id)
    {
        return GWCA_CALL_C(GW_QuestMgr_AbandonQuestId, quest_id);
    }

    inline QuestLog* GetQuestLog()
    {
        return GWCA_CALL_C(GW_QuestMgr_GetQuestLog);
    }

    inline Quest* GetQuest(GW::Constants::QuestID gwca_arg0)
    {
        return GWCA_CALL_C(GW_QuestMgr_GetQuest, gwca_arg0);
    }

    inline bool GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len)
    {
        return GWCA_CALL_C(GW_QuestMgr_GetQuestEntryGroupName, quest_id, out, out_len);
    }

    inline bool RequestQuestInfo(const Quest* quest, bool update_markers)
    {
        return GWCA_CALL_C(GW_QuestMgr_RequestQuestInfo, quest, update_markers);
    }

    inline bool RequestQuestInfoId(Constants::QuestID quest_id, bool update_markers)
    {
        return GWCA_CALL_C(GW_QuestMgr_RequestQuestInfoId, quest_id, update_markers);
    }
}
#endif
