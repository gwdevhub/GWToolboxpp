#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {
    enum class FriendType : uint32_t {
        Unknow = 0,
        Friend = 1,
        Ignore = 2,
        Player = 3,
        Trade  = 4,
    };

    enum class FriendStatus : uint32_t {
        Offline = 0,
        Online  = 1,
        DND     = 2,
        Away    = 3,
        Unknown    = 4
    };

    struct Friend {
        /* +h0000 */ FriendType type;
        /* +h0004 */ FriendStatus status;
        /* +h0008 */ uint8_t uuid[16];
        /* +h0018 */ wchar_t alias[20];
        /* +h002C */ wchar_t charname[20];
        /* +h0040 */ uint32_t friend_id;
        /* +h0044 */ uint32_t zone_id;
    };

    typedef Array<Friend *> FriendsListArray;

    struct FriendList {
        /* +h0000 */ FriendsListArray friends;
        /* +h0010 */ uint8_t  h0010[20];
        /* +h0024 */ uint32_t number_of_friend;
        /* +h0028 */ uint32_t number_of_ignore;
        /* +h002C */ uint32_t number_of_partner;
        /* +h0030 */ uint32_t number_of_trade;
        /* +h0034 */ uint8_t  h0034[108];
        /* +h00A0 */ FriendStatus player_status;
    };
}
