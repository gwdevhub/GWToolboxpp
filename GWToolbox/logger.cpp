#include "logger.h"

#include <strsafe.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdarg.h>
#include <time.h>

#include "GuiUtils.h"
#include "GWToolbox.h"

FILE* Logger::logfile = NULL;

void Logger::Init() {
#if _DEBUG
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitleA("GWTB++ Debug Console");
#else
	FILE* fh;
	freopen_s(&fh, GuiUtils::getPathA("log.txt").c_str(), "w", stdout);
	//freopen_s(&logfile, GuiUtils::getPathA("log.txt").c_str(), "w", stderr);
	logfile = fh;
#endif
}

void Logger::Close() {
#if _DEBUG
	FreeConsole();
#else
	if (logfile) fclose(logfile);
#endif
}

void Logger::PrintTimestamp() {
	time_t rawtime;
	time(&rawtime);
	
	struct tm timeinfo;
	localtime_s(&timeinfo, &rawtime);

	char buffer[16];
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

	fprintf(stdout, "[%s] ", buffer);
}

void Logger::Log(const char* msg, ...) {
	PrintTimestamp();

	va_list args;
	va_start(args, msg);
	vfprintf(stdout, msg, args);
	va_end(args);

#ifndef _DEBUG
	fflush(logfile);
#endif
}

void Logger::LogW(const wchar_t* msg, ...) {
	PrintTimestamp();

	va_list args;
	va_start(args, msg);
	vfwprintf(stdout, msg, args);
	va_end(args);

#ifndef _DEBUG
	fflush(logfile);
#endif
}

LONG WINAPI Logger::GenerateDump(EXCEPTION_POINTERS* pExceptionPointers) {
	BOOL bMiniDumpSuccessful;
	WCHAR szFileName[MAX_PATH];
	HANDLE hDumpFile;
	SYSTEMTIME stLocalTime;
	MINIDUMP_EXCEPTION_INFORMATION ExpParam;

	GetLocalTime(&stLocalTime);

	StringCchPrintf(szFileName, MAX_PATH, L"%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
		GuiUtils::getSettingsFolderW().c_str() , GWToolbox::VersionW,
		stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
		GetCurrentProcessId(), GetCurrentThreadId());
	hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	ExpParam.ThreadId = GetCurrentThreadId();
	ExpParam.ExceptionPointers = pExceptionPointers;
	ExpParam.ClientPointers = TRUE;

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
