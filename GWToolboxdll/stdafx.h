#pragma once

#pragma warning(push)

#pragma warning(disable: 4099) // PDB '*.pdb' was not found with '*.lib(*.obj)' or at '..\..\..\*.pdb'; linking object as if no debug info
#pragma warning(disable: 26495) //  Variable 'variable' is uninitialized. Always initialize a member variable(type .6)

#include <GWCA/Source/stdafx.h>

// c++ style c headers
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>

// c++ headers
#include <array>
#include <algorithm>
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

#include <strsafe.h>
#include <DbgHelp.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <TlHelp32.h>
#include <windowsx.h>
#include <WinSock2.h>
#include <WinUser.h>
#include <WS2tcpip.h>
#include <WinInet.h>

#include <d3d9.h>
#include <DirectXMath.h>

// libraries
#include <Logger.h>
#include <discord_game_sdk.h>
#include <ToolboxIni.h>

#include <nlohmann/json.hpp>
#include <easywsclient.hpp>
#include <mp3.h>
#include <IconsFontAwesome5.h>
#include <mapbox/earcut.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

#pragma warning(pop)

#pragma warning(disable: 4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable: 4505) // 'function' : unreferenced local function has been removed
#pragma warning(disable: 28159) // Consider using 'GetTickCount64' instead of 'GetTickCount'.
