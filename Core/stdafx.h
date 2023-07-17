#pragma once

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
# define NOMINMAX
#endif
#ifdef NDEBUG
#define assert(expression) (expression)
#endif
#include <Windows.h>

#include <Shlobj.h>
#include <shlwapi.h>
#include <psapi.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <ranges>
#include <filesystem>
