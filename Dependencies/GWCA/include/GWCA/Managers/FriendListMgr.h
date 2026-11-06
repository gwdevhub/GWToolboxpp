#pragma once
#include <GWCA/Runtime.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>
namespace GW {
    struct Friend;
    struct FriendList;
    enum class FriendStatus : uint32_t;
    enum class FriendType : uint32_t;
    struct Module;
    extern Module FriendListModule;
    namespace FriendListMgr {
        FriendList* GetFriendList();
        Friend* GetFriendByName(const wchar_t* alias, const wchar_t* charname, FriendType type = (FriendType)1);
        Friend* GetFriendByIndex(uint32_t index);
        Friend* GetFriendByUuid(const uint8_t* uuid);
        inline Friend* GetFriend(const wchar_t* alias, const wchar_t* charname, FriendType type = (FriendType)1) { return GetFriendByName(alias, charname, type); }
        inline Friend* GetFriend(uint32_t index) { return GetFriendByIndex(index); }
        inline Friend* GetFriend(const uint8_t* uuid) { return GetFriendByUuid(uuid); }
        uint32_t GetNumberOfFriends(FriendType = (FriendType)1);
        uint32_t GetNumberOfIgnores();
        uint32_t GetNumberOfPartners();
        uint32_t GetNumberOfTraders();
        FriendStatus GetMyStatus();
        bool SetFriendListStatus(FriendStatus status);
        using NativeFriendStatusCallback = void (__cdecl *)(HookStatus* status, const Friend* old_state, const Friend* new_state, void* context);
        bool RegisterFriendStatusCallback(
            HookEntry* entry,
            NativeFriendStatusCallback callback, void* context = nullptr);
        void RemoveFriendStatusCallbackNative(
            HookEntry* entry);
#if !defined(GWCA_STDLIB)
        inline void RemoveFriendStatusCallback(HookEntry* entry) { RemoveFriendStatusCallbackNative(entry); }
#endif
        bool AddFriend(const wchar_t* name, const wchar_t* alias = nullptr);
        bool AddIgnore(const wchar_t* name, const wchar_t* alias = nullptr);
        // Fire-and-forget, like AddFriend: true means the request was queued (it
        // may need the Friends window to finish opening across a few game ticks
        // first), not that the row is gone by the time this returns.
        bool RemoveFriend(Friend* _friend);
        bool ChangeFriendType(Friend* _friend, FriendType type);
    };
}

// C Interop API

namespace GW::FriendListMgr {
    extern "C" GWCA_API FriendList* __cdecl GW_FriendListMgr_GetFriendList() noexcept;
    extern "C" GWCA_API Friend* __cdecl GW_FriendListMgr_GetFriendByName(const wchar_t* alias, const wchar_t* charname, FriendType type) noexcept;
    extern "C" GWCA_API Friend* __cdecl GW_FriendListMgr_GetFriendByIndex(uint32_t index) noexcept;
    extern "C" GWCA_API Friend* __cdecl GW_FriendListMgr_GetFriendByUuid(const uint8_t* uuid) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_FriendListMgr_GetNumberOfFriends(FriendType gwca_arg0) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_FriendListMgr_GetNumberOfIgnores() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_FriendListMgr_GetNumberOfPartners() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_FriendListMgr_GetNumberOfTraders() noexcept;
    extern "C" GWCA_API FriendStatus __cdecl GW_FriendListMgr_GetMyStatus() noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_SetFriendListStatus(FriendStatus status) noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_RegisterFriendStatusCallback(HookEntry* entry, NativeFriendStatusCallback callback, void* context) noexcept;
    extern "C" GWCA_API void __cdecl GW_FriendListMgr_RemoveFriendStatusCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_AddFriend(const wchar_t* name, const wchar_t* alias) noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_AddIgnore(const wchar_t* name, const wchar_t* alias) noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_RemoveFriend(Friend* _friend) noexcept;
    extern "C" GWCA_API bool __cdecl GW_FriendListMgr_ChangeFriendType(Friend* _friend, FriendType type) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::FriendListMgr {
    inline FriendList* GetFriendList()
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetFriendList);
    }

    inline Friend* GetFriendByName(const wchar_t* alias, const wchar_t* charname, FriendType type)
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetFriendByName, alias, charname, type);
    }

    inline Friend* GetFriendByIndex(uint32_t index)
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetFriendByIndex, index);
    }

    inline Friend* GetFriendByUuid(const uint8_t* uuid)
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetFriendByUuid, uuid);
    }

    inline uint32_t GetNumberOfFriends(FriendType gwca_arg0)
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetNumberOfFriends, gwca_arg0);
    }

    inline uint32_t GetNumberOfIgnores()
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetNumberOfIgnores);
    }

    inline uint32_t GetNumberOfPartners()
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetNumberOfPartners);
    }

    inline uint32_t GetNumberOfTraders()
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetNumberOfTraders);
    }

    inline FriendStatus GetMyStatus()
    {
        return GWCA_CALL_C(GW_FriendListMgr_GetMyStatus);
    }

    inline bool SetFriendListStatus(FriendStatus status)
    {
        return GWCA_CALL_C(GW_FriendListMgr_SetFriendListStatus, status);
    }

    inline bool RegisterFriendStatusCallback(HookEntry* entry, NativeFriendStatusCallback callback, void* context)
    {
        return GWCA_CALL_C(GW_FriendListMgr_RegisterFriendStatusCallback, entry, callback, context);
    }

    inline void RemoveFriendStatusCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_FriendListMgr_RemoveFriendStatusCallbackNative, entry);
    }

    inline bool AddFriend(const wchar_t* name, const wchar_t* alias)
    {
        return GWCA_CALL_C(GW_FriendListMgr_AddFriend, name, alias);
    }

    inline bool AddIgnore(const wchar_t* name, const wchar_t* alias)
    {
        return GWCA_CALL_C(GW_FriendListMgr_AddIgnore, name, alias);
    }

    inline bool RemoveFriend(Friend* _friend)
    {
        return GWCA_CALL_C(GW_FriendListMgr_RemoveFriend, _friend);
    }

    inline bool ChangeFriendType(Friend* _friend, FriendType type)
    {
        return GWCA_CALL_C(GW_FriendListMgr_ChangeFriendType, _friend, type);
    }
}
#endif
