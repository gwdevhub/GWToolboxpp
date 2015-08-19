#pragma once
#include <stdio.h>

/*
This class is my attempt at creating a logger that can be toggled in release

it can be naive or bad, idk, but works and should be efficient on non-debug build
*/

#define DEBUG_BUILD 1

#if DEBUG_BUILD
#define LOG(msg, ...) fprintf(stdout, msg, ##__VA_ARGS__)
#define LOGW(msg, ...) fwprintf(stdout, msg, ##__VA_ARGS__)
#define ERR(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#define IFLTZERR(var, msg, ...) if (var < 0) fprintf(stderr, msg, ##__VA_ARGS__)
// can make IFLEZERR, IFEQZERR, IFGTZERR, etc if needed
#else
#define LOG(msg, ...)
#define LOGW(msg, ...)
#define ERR(msg, ...)
#define IFLTZERR(var, msg, ...)
#endif
