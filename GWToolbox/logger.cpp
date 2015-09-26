#include "logger.h"

#include <stdarg.h>
#include <time.h>

#include "GuiUtils.h"

FILE* Logger::logfile = NULL;

void Logger::Init() {
#if _DEBUG
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitleA("GWTB++ Debug Console");
#else
	freopen_s(&Logger::logfile, GuiUtils::getPathA("log.txt").c_str(), "w", stdout);
	//freopen_s(&logfile, GuiUtils::getPathA("log.txt").c_str(), "w", stderr);
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
