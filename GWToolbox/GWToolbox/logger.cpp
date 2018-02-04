#include "logger.h"

#include <stdio.h>
#include <strsafe.h>
#include <dbghelp.h>
#include <time.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ChatMgr.h>

#include "Defines.h"
#include <Modules\Resources.h>

#define CHAN_WARNING GW::Chat::CHANNEL_GWCA2
#define CHAN_INFO    GW::Chat::CHANNEL_EMOTE
#define CHAN_ERROR   GW::Chat::CHANNEL_GWCA3

namespace {
	FILE* logfile = nullptr;
}

// === Setup and cleanup ====
void Log::InitializeLog() {
#if _DEBUG
	logfile = stdout;
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitle("GWTB++ Debug Console");
#else
	logfile = freopen(Resources::GetPath("log.txt").c_str(), "w", stdout);
#endif
}

void Log::InitializeChat() {
	GW::Chat::SetMessageColor(CHAN_WARNING, 0xFFFFFF44); // warning
	GW::Chat::SetMessageColor(CHAN_INFO, 0xFFFFFFFF); // info
	GW::Chat::SetMessageColor(CHAN_ERROR, 0xFFFF4444); // error
}

void Log::Terminate() {
#if _DEBUG
	FreeConsole();
#else
	if (logfile) {
		fflush(logfile);
		fclose(logfile);
	}
#endif
}

// === File/console logging ===
static void PrintTimestamp() {
	time_t rawtime;
	time(&rawtime);
	
	struct tm timeinfo;
	localtime_s(&timeinfo, &rawtime);

	char buffer[16];
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

	fprintf(logfile, "[%s] ", buffer);
}

void Log::Log(const char* msg, ...) {
	if (!logfile) return;
	PrintTimestamp();

	va_list args;
	va_start(args, msg);
	vfprintf(logfile, msg, args);
	va_end(args);
}

void Log::LogW(const wchar_t* msg, ...) {
	if (!logfile) return;
	PrintTimestamp();

	va_list args;
	va_start(args, msg);
	vfwprintf(logfile, msg, args);
	va_end(args);
}

// === Game chat logging ===
static void _vchatlog(GW::Chat::Channel chan, const char* format, va_list argv) {
	char buf1[256];
	vsprintf_s(buf1, format, argv);

	char buf2[256];
	snprintf(buf2, 256, "<c=#00ccff>GWToolbox++</c>: %s", buf1);
	GW::Chat::WriteChat(chan, buf2);

	const char* c = [](GW::Chat::Channel chan) -> const char* {
		switch (chan) {
		case CHAN_INFO: return "Info";
		case CHAN_WARNING: return "Warning";
		case CHAN_ERROR: return "Error";
		default: return "";
		}
	}(chan);
	Log::Log("[%s] %s\n", c, buf1);
}

void Log::Info(const char* format, ...) {
	va_list vl;
	va_start(vl, format);
	_vchatlog(CHAN_INFO, format, vl);
	va_end(vl);
}

void Log::Error(const char* format, ...) {
	va_list vl;
	va_start(vl, format);
	_vchatlog(CHAN_ERROR, format, vl);
	va_end(vl);
}

void Log::Warning(const char* format, ...) {
	va_list vl;
	va_start(vl, format);
	_vchatlog(CHAN_WARNING, format, vl);
	va_end(vl);
}

// === Crash Dump ===
LONG WINAPI Log::GenerateDump(EXCEPTION_POINTERS* pExceptionPointers) {
	BOOL bMiniDumpSuccessful;
	CHAR szFileName[MAX_PATH];
	HANDLE hDumpFile;
	SYSTEMTIME stLocalTime;
	MINIDUMP_EXCEPTION_INFORMATION ExpParam;

	GetLocalTime(&stLocalTime);

	StringCchPrintf(szFileName, MAX_PATH, "%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
		Resources::GetSettingsFolderPath().c_str(), GWTOOLBOX_VERSION,
		stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
		GetCurrentProcessId(), GetCurrentThreadId());
	hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	ExpParam.ThreadId = GetCurrentThreadId();
	ExpParam.ExceptionPointers = pExceptionPointers;
	ExpParam.ClientPointers = TRUE;

	//MINIDUMP_TYPE flags = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithPrivateReadWriteMemory);
	bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
		hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);

	if (bMiniDumpSuccessful) {
		MessageBoxA(0,
			"GWToolbox crashed, oops\n\n"
			"Log and dump files have been created in the GWToolbox data folder.\n"
			"Open it by typing running %LOCALAPPDATA% and looking for GWToolboxpp folder\n"
			"Please send the files to the GWToolbox++ developers.\n"
			"Thank you and sorry for the inconvenience.",
			"GWToolbox++ Crash!", 0);
	} else {
		MessageBoxA(0,
			"GWToolbox crashed, oops\n\n"
			"Error creating the dump file\n"
			"I don't really know what to do, sorry, contact the developers.\n",
			"GWToolbox++ Crash!", 0);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
