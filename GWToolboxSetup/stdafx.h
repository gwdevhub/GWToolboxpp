#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <Commctrl.h>
#include <shellapi.h>
#include <Shlobj.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <ranges>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
