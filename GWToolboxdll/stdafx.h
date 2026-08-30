#pragma once

#pragma warning(push)

#pragma warning(disable: 4099) // PDB '*.pdb' was not found with '*.lib(*.obj)' or at '..\..\..\*.pdb'; linking object as if no debug info
#pragma warning(disable: 26495) //  Variable 'variable' is uninitialized. Always initialize a member variable(type .6)

#include <GWCA/stdafx.h>

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
#include <numbers>
#include <print>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <vector>

// windows headers -- none of this exists under Emscripten; mirrors the GWCA_WASM split GWCA/stdafx.h already makes for its own Win32Shim.h.
#ifndef __EMSCRIPTEN__
#include <Windows.h>
#include <Psapi.h>

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
#endif

// libraries
#include <Logger.h>
#ifndef __EMSCRIPTEN__
#include <discord-game-sdk/discord_game_sdk.h>
#endif
#include <ToolboxIni.h>

#ifndef __EMSCRIPTEN__
// glaze/ctre are portable but vcpkg-managed, and vcpkg isn't part of an `emcmake` configure; deferred until a kept-for-wasm file needs one (pull in via FetchContent then, matching cmake/imgui.cmake).
#include <glaze/glaze.hpp>
// winhttp-backed and Windows-only mp3 playback both need a browser-native replacement (JS WebSocket, Web Audio) before any wasm-compiled file can use them.
#include <easywsclient.hpp>
#include <mp3.h>
#endif
#ifndef __EMSCRIPTEN__
// IconsFontAwesome5.h: upstream declares its data `static` against an `extern` decl, which clang/emcc rejects; mapbox_earcut.h is vcpkg-provided. Neither needed by the wasm source list yet.
#include <IconsFontAwesome5.h>
#include <mapbox_earcut.h>
#endif

#ifndef __EMSCRIPTEN__
#define __forceinline
#include <ctre.hpp>
#undef __forceinline
#endif

#include <imgui.h>
#include <imgui_internal.h>
#ifndef __EMSCRIPTEN__
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#else
// gw_in_browser's loader wires up a real GLES3 binding (harness/gllib.js) - this is the standard Dear ImGui backend, not a stub.
#include "imgui_impl_opengl3.h"
#endif

#ifndef __EMSCRIPTEN__
// Pulls in glaze (see above) transitively; not needed by the wasm source list either.
#include <Utils/SettingsRegistry.h>
#endif

#pragma warning(pop)

#pragma warning(disable: 4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable: 4505) // 'function' : unreferenced local function has been removed
#pragma warning(disable: 4864) // expected 'template' keyword before dependent template name
#pragma warning(disable: 28159) // Consider using 'GetTickCount64' instead of 'GetTickCount'
