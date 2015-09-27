#pragma once

#include <Windows.h>
#include <stdio.h>
#include <time.h>


#ifdef _DEBUG
#define DEBUG_BUILD 1
#else
#define DEBUG_BUILD 0
#endif

#define LOG(msg, ...) Logger::Log(msg, ##__VA_ARGS__)
#define LOGW(msg, ...) Logger::LogW(msg, ##__VA_ARGS__)

class Logger {
private:
	static FILE* logfile;
	static void PrintTimestamp();

public:
	// in release redirects stdout and stderr to log file
	// in debug creates console
	static void Init();

	// printf-style log
	static void Log(const char* msg, ...);
	
	// printf-style wide-string log
	static void LogW(const wchar_t* msg, ...);

	// flushes log file.
	static void FlushFile();

	// in release it closes the file
	// in debug frees the console
	static void Close();

	// Creates minidump, to be called from within __except()
	static LONG WINAPI GenerateDump(
		EXCEPTION_POINTERS* pExceptionPointers);
};
