#pragma once

#include <stdio.h>
#include <time.h>

/*
This class is my attempt at creating a logger that can be toggled in release

it can be naive or bad, idk, but works and should be efficient on non-debug build
*/
#ifdef _DEBUG
#define DEBUG_BUILD 1
#else
#define DEBUG_BUILD 0
#endif

#define LOG(msg, ...) Logger::Log(msg, ##__VA_ARGS__)
#define LOGW(msg, ...) Logger::LogW(msg, ##__VA_ARGS__)

class Logger {
private:
	static void PrintTimestamp();

public:
	static void Log(const char* msg, ...);
	
	static void LogW(const wchar_t* msg, ...);
};
