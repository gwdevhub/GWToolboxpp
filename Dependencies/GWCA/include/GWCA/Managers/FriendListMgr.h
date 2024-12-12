#pragma once

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

        GWCA_API FriendList *GetFriendList();

        GWCA_API Friend *GetFriend(const wchar_t *alias, const wchar_t *charname, FriendType type = (FriendType)1);
        GWCA_API Friend *GetFriend(uint32_t index);
        GWCA_API Friend *GetFriend(const uint8_t *uuid);

        GWCA_API uint32_t GetNumberOfFriends(FriendType = (FriendType)1);
        GWCA_API uint32_t GetNumberOfIgnores();
        GWCA_API uint32_t GetNumberOfPartners();
        GWCA_API uint32_t GetNumberOfTraders();

        GWCA_API FriendStatus GetMyStatus();

        GWCA_API bool SetFriendListStatus(FriendStatus status);

        typedef HookCallback<const Friend*, const Friend*> FriendStatusCallback;
        GWCA_API void RegisterFriendStatusCallback(
            HookEntry *entry,
            const FriendStatusCallback& callback);

        GWCA_API void RemoveFriendStatusCallback(
            HookEntry *entry);

        GWCA_API bool AddFriend(const wchar_t *name, const wchar_t *alias = nullptr);
        GWCA_API bool AddIgnore(const wchar_t *name, const wchar_t *alias = nullptr);
        GWCA_API bool RemoveFriend(Friend *_friend);
    };
}
