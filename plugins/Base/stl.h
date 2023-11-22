#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// c++ style c headers
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>

// c++ headers
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <concepts>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// windows headers
#include <Windows.h>

#include <DbgHelp.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <TlHelp32.h>
#include <windowsx.h>
#include <WinInet.h>
#include <WinSock2.h>
#include <WinUser.h>
#include <WS2tcpip.h>

#include <d3d9.h>
#include <DirectXMath.h>

// libraries
#include <ToolboxIni.h>

#include <IconsFontAwesome5.h>
#include <mp3.h>
#include <nlohmann/json.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#define VAR_NAME(v) (#v)
#ifndef PLUGIN_ASSERT
#define PLUGIN_ASSERT(expr) ((void)(!!(expr) || (std::cerr << (#expr, __FILE__, (unsigned)__LINE__), 0)))
#endif

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);
#define LOAD_FLOAT(var) var = static_cast<float>(ini->GetDoubleValue(Name(), #var, static_cast<double>(var)));
#define SAVE_FLOAT(var) ini->SetDoubleValue(Name(), #var, static_cast<double>(var));
#define LOAD_UINT(var) var = static_cast<unsigned int>(ini->GetLongValue(Name(), #var, static_cast<long>(var)));
#define SAVE_UINT(var) ini->SetLongValue(Name(), #var, static_cast<long>(var));
