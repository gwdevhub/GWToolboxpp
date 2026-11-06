#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>
#include <cstdarg>

namespace GW {
	struct Module;
	extern Module ChatModule;

	struct HookStatus;
	struct HookEntry;

	namespace Chat {
		typedef uint32_t Color;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200)
#endif
		struct ChatMessage {
			uint32_t channel;
			uint32_t unk1;
			FILETIME timestamp;
			wchar_t message[0];
		};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

		const size_t CHAT_LOG_LENGTH = 0x200;
		struct ChatBuffer {
			uint32_t next;
			uint32_t unk1;
			uint32_t unk2;
			ChatMessage* messages[CHAT_LOG_LENGTH];
		};
		namespace TextColor {
			constexpr uint32_t ColorItemCommon = 0xFFFFFFFF;
			constexpr uint32_t ColorItemEnhance = 0xFFA0F5F8;
			constexpr uint32_t ColorItemUncommon = 0xFFB38AEC;
			constexpr uint32_t ColorItemRare = 0xFFFFD24F;
			constexpr uint32_t ColorItemUnique = 0xFF00FF00;
			constexpr uint32_t ColorItemUniquePvp = 0xFFED1C24;
			constexpr uint32_t ColorItemDull = 0xFFA0A0A0;
			constexpr uint32_t ColorItemBasic = 0xFFFFFFFF;
			constexpr uint32_t ColorItemBonus = 0xFFA0F5F8;
			constexpr uint32_t ColorItemAssign = 0xFF6CC16D;
			constexpr uint32_t ColorItemCustom = 0xFFA0A0A0;
			constexpr uint32_t ColorItemRestrict = 0xFFF67D4D;
			constexpr uint32_t ColorItemSell = 0xFFFFFF00;
			constexpr uint32_t ColorLabel = 0xFFFFEAB8;
			constexpr uint32_t ColorQuest = 0xFF00FF00;
			constexpr uint32_t ColorSkillDull = 0xFFA0A0A0;
			constexpr uint32_t ColorWarning = 0xFFED0002;
		}

		enum Channel : int {
			CHANNEL_ALLIANCE = 0,
			CHANNEL_ALLIES = 1,
			CHANNEL_GWCA1 = 2,
			CHANNEL_ALL = 3,
			CHANNEL_GWCA2 = 4,
			CHANNEL_MODERATOR = 5,
			CHANNEL_EMOTE = 6,
			CHANNEL_WARNING = 7,
			CHANNEL_GWCA3 = 8,
			CHANNEL_GUILD = 9,
			CHANNEL_GLOBAL = 10,
			CHANNEL_GROUP = 11,
			CHANNEL_TRADE = 12,
			CHANNEL_ADVISORY = 13,
			CHANNEL_WHISPER = 14,
			CHANNEL_COUNT,
			CHANNEL_COMMAND,
			CHANNEL_UNKNOW = -1
		};


		Chat::ChatBuffer* GetChatLog();
		bool AddToChatLog(wchar_t* message, uint32_t channel);
		bool GetIsTyping();

		bool SendChatCurrentA(const char* msg);
		bool SendChatCurrentW(const wchar_t* msg);
		bool SendChatChannelA(char channel, const char* msg);
		bool SendChatChannelW(char channel, const wchar_t* msg);
		bool SendWhisper(const wchar_t* to, const wchar_t* msg);
        inline bool SendChat(const char* msg) { return SendChatCurrentA(msg); }
        inline bool SendChat(const wchar_t* msg) { return SendChatCurrentW(msg); }
        inline bool SendChat(char channel, const char* msg) { return SendChatChannelA(channel, msg); }
        inline bool SendChat(char channel, const wchar_t* msg) { return SendChatChannelW(channel, msg); }
        inline bool SendChat(const wchar_t* to, const wchar_t* msg) { return SendWhisper(to, msg); }

		void WriteChatV(Channel channel, const wchar_t* format, va_list args);
		inline void WriteChatF(Channel channel, const wchar_t* format, ...)
		{
			va_list args;
			va_start(args, format);
			WriteChatV(channel, format, args);
			va_end(args);
		}
		void WriteChat(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);
		void WriteChatEnc(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);

		typedef void(__cdecl* ChatCommandCallback)(GW::HookStatus*, const wchar_t* cmd, int argc, const LPWSTR* argv);
		void CreateCommand(GW::HookEntry* entry, const wchar_t* cmd, ChatCommandCallback callback);
		void DeleteCommand(GW::HookEntry* entry, const wchar_t* cmd = 0);

		void ToggleTimestamps(bool enable);
		void SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds = false);
		void SetTimestampsColor(Color color);

		Color SetSenderColor(Channel chan, Color col);
		Color SetMessageColor(Channel chan, Color col);
		void  GetChannelColors(Channel chan, Color* sender, Color* message);
		void  GetDefaultColors(Channel chan, Color* sender, Color* message);

		Channel GetChannel(char opcode);
		inline Channel GetChannel(wchar_t opcode) { return GetChannel(static_cast<char>(opcode)); }
		Channel GetCurrentChatChannel();
	};
}

// C Interop API

namespace GW::Chat {
    extern "C" GWCA_API Chat::ChatBuffer* __cdecl GW_Chat_GetChatLog() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_AddToChatLog(wchar_t* message, uint32_t channel) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_GetIsTyping() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_SendChatCurrentA(const char* msg) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_SendChatCurrentW(const wchar_t* msg) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_SendChatChannelA(char channel, const char* msg) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_SendChatChannelW(char channel, const wchar_t* msg) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Chat_SendWhisper(const wchar_t* to, const wchar_t* msg) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_WriteChatV(Channel channel, const wchar_t* format, va_list args) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_WriteChat(Channel channel, const wchar_t* message, const wchar_t* sender, bool transient) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_WriteChatEnc(Channel channel, const wchar_t* message, const wchar_t* sender, bool transient) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_CreateCommand(GW::HookEntry* entry, const wchar_t* cmd, ChatCommandCallback callback) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_DeleteCommand(GW::HookEntry* entry, const wchar_t* cmd) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_ToggleTimestamps(bool enable) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_SetTimestampsColor(Color color) noexcept;
    extern "C" GWCA_API Color __cdecl GW_Chat_SetSenderColor(Channel chan, Color col) noexcept;
    extern "C" GWCA_API Color __cdecl GW_Chat_SetMessageColor(Channel chan, Color col) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_GetChannelColors(Channel chan, Color* sender, Color* message) noexcept;
    extern "C" GWCA_API void __cdecl GW_Chat_GetDefaultColors(Channel chan, Color* sender, Color* message) noexcept;
    extern "C" GWCA_API Channel __cdecl GW_Chat_GetChannel(char opcode) noexcept;
    extern "C" GWCA_API Channel __cdecl GW_Chat_GetCurrentChatChannel() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Chat {
    inline Chat::ChatBuffer* GetChatLog()
    {
        return GWCA_CALL_C(GW_Chat_GetChatLog);
    }

    inline bool AddToChatLog(wchar_t* message, uint32_t channel)
    {
        return GWCA_CALL_C(GW_Chat_AddToChatLog, message, channel);
    }

    inline bool GetIsTyping()
    {
        return GWCA_CALL_C(GW_Chat_GetIsTyping);
    }

    inline bool SendChatCurrentA(const char* msg)
    {
        return GWCA_CALL_C(GW_Chat_SendChatCurrentA, msg);
    }

    inline bool SendChatCurrentW(const wchar_t* msg)
    {
        return GWCA_CALL_C(GW_Chat_SendChatCurrentW, msg);
    }

    inline bool SendChatChannelA(char channel, const char* msg)
    {
        return GWCA_CALL_C(GW_Chat_SendChatChannelA, channel, msg);
    }

    inline bool SendChatChannelW(char channel, const wchar_t* msg)
    {
        return GWCA_CALL_C(GW_Chat_SendChatChannelW, channel, msg);
    }

    inline bool SendWhisper(const wchar_t* to, const wchar_t* msg)
    {
        return GWCA_CALL_C(GW_Chat_SendWhisper, to, msg);
    }

    inline void WriteChatV(Channel channel, const wchar_t* format, va_list args)
    {
        return GWCA_CALL_C(GW_Chat_WriteChatV, channel, format, args);
    }

    inline void WriteChat(Channel channel, const wchar_t* message, const wchar_t* sender, bool transient)
    {
        return GWCA_CALL_C(GW_Chat_WriteChat, channel, message, sender, transient);
    }

    inline void WriteChatEnc(Channel channel, const wchar_t* message, const wchar_t* sender, bool transient)
    {
        return GWCA_CALL_C(GW_Chat_WriteChatEnc, channel, message, sender, transient);
    }

    inline void CreateCommand(GW::HookEntry* entry, const wchar_t* cmd, ChatCommandCallback callback)
    {
        return GWCA_CALL_C(GW_Chat_CreateCommand, entry, cmd, callback);
    }

    inline void DeleteCommand(GW::HookEntry* entry, const wchar_t* cmd)
    {
        return GWCA_CALL_C(GW_Chat_DeleteCommand, entry, cmd);
    }

    inline void ToggleTimestamps(bool enable)
    {
        return GWCA_CALL_C(GW_Chat_ToggleTimestamps, enable);
    }

    inline void SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds)
    {
        return GWCA_CALL_C(GW_Chat_SetTimestampsFormat, use_24h, show_timestamp_seconds);
    }

    inline void SetTimestampsColor(Color color)
    {
        return GWCA_CALL_C(GW_Chat_SetTimestampsColor, color);
    }

    inline Color SetSenderColor(Channel chan, Color col)
    {
        return GWCA_CALL_C(GW_Chat_SetSenderColor, chan, col);
    }

    inline Color SetMessageColor(Channel chan, Color col)
    {
        return GWCA_CALL_C(GW_Chat_SetMessageColor, chan, col);
    }

    inline void GetChannelColors(Channel chan, Color* sender, Color* message)
    {
        return GWCA_CALL_C(GW_Chat_GetChannelColors, chan, sender, message);
    }

    inline void GetDefaultColors(Channel chan, Color* sender, Color* message)
    {
        return GWCA_CALL_C(GW_Chat_GetDefaultColors, chan, sender, message);
    }

    inline Channel GetChannel(char opcode)
    {
        return GWCA_CALL_C(GW_Chat_GetChannel, opcode);
    }

    inline Channel GetCurrentChatChannel()
    {
        return GWCA_CALL_C(GW_Chat_GetCurrentChatChannel);
    }
}
#endif
