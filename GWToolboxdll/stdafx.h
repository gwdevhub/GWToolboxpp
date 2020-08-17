#pragma once

#pragma warning(disable: 4619) // there is no warning number 'number'

#pragma warning(push)
// Warning 5204 was removed, so it trigger a warning about a warning not existing, so we disable warning about warning.
#pragma warning(disable: 4619) // #pragma warning : there is no warning number 'number'

#pragma warning(disable: 4061) // enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
#pragma warning(disable: 4091) // 'keyword' : ignored on left of 'type' when no variable is declared
#pragma warning(disable: 4099) // PDB '*.pdb' was not found with '*.lib(*.obj)' or at '..\..\..\*.pdb'; linking object as if no debug info
#pragma warning(disable: 4100) // 'identifier' : unreferenced formal parameter
#pragma warning(disable: 4365) // 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning(disable: 4514) // 'function' : unreferenced inline function has been removed
#pragma warning(disable: 4548) // expression before comma has no effect; expected expression with side-effect
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4574) // 'Identifier' is defined to be '0': did you mean to use '#if identifier'?
#pragma warning(disable: 4577) // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#pragma warning(disable: 4623) // 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
#pragma warning(disable: 4625) // 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
#pragma warning(disable: 4626) // 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
#pragma warning(disable: 4628) // digraphs not supported with -Ze. Character sequence 'digraph' not interpreted as alternate token for 'char'
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4710) // 'function' : function not inlined
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4738) // storing 32-bit float result in memory, possible loss of performance
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 4917) // 'declarator' : a GUID can only be associated with a class, interface or namespace
#pragma warning(disable: 5026) // 'type': move constructor was implicitly defined as deleted
#pragma warning(disable: 5027) // 'type': move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5029) // nonstandard extension used: alignment attributes in C++ apply to variables, data members and tag types only
#pragma warning(disable: 5039) // 'function': pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception.
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#pragma warning(disable: 5204) // class has virtual functions, but its trivial destructor is not virtual
#pragma warning(disable: 6011) // Dereferencing NULL pointer
#pragma warning(disable: 26495) // Variable is uninitialized

#include <GWCA/Source/stdafx.h>

// c style headers
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <strsafe.h>
#include <time.h>

// c++ headers
#include <algorithm>
#include <bitset>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
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
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>

// windows headers
#include <Windows.h>

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
#include <d3dx9math.h>
#include <d3dx9tex.h>

// libraries
#include <Logger.h>
#include <discord_game_sdk.h>
#include <SimpleIni.h>
#include <json.hpp>
#include <easywsclient.hpp>
#include <mp3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx9.h>

#pragma warning(pop)

#pragma warning(disable: 4061) // enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
#pragma warning(disable: 4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable: 4464) // relative include path contains '..'
#pragma warning(disable: 4514) // 'function' : unreferenced inline function has been removed
#pragma warning(disable: 4505) // 'function' : unreferenced local function has been removed
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4577) // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#pragma warning(disable: 4623) // 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
#pragma warning(disable: 4626) // 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
#pragma warning(disable: 4710) // 'function' : function not inlined
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4774) // 'string' : format string expected in argument number is not a string literal
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 5027) // 'type': move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5029) // nonstandard extension used: alignment attributes in C++ apply to variables, data members and tag types only
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified

// @Enhancement: Can we fix those?
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4738) // storing 32-bit float result in memory, possible loss of performance
