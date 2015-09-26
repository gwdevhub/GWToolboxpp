#include "logger.h"

#include <stdarg.h>
#include <time.h>

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
}

void Logger::LogW(const wchar_t* msg, ...) {
	PrintTimestamp();

	va_list args;
	va_start(args, msg);
	vfwprintf(stdout, msg, args);
	va_end(args);
}
