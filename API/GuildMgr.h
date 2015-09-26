#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

namespace GWAPI {

	class GuildMgr {
		GWAPIMgr* const parent_;
		friend class GWAPIMgr;
		GuildMgr(GWAPIMgr* obj) : parent_(obj) {}

	public:

		// Array of guilds, holds basically everything about a guild. Can get structs of all players in outpost ;)
		GW::GuildArray GetGuildArray();
		
		// Index in guild array of player guild.
		DWORD GetPlayerGuildIndex();

		// Announcement in guild at the moment.
		wchar_t* GetPlayerGuildAnnouncement();

		// Name of player who last edited the announcement.
		wchar_t* GetPlayerGuildAnnouncer();
	};
}