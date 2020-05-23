#pragma once

#include <GWCA/Source/stdafx.h>

#pragma warning(push)
#pragma warning(disable: 4061) // enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
#pragma warning(disable: 4100) // 'identifier' : unreferenced formal parameter
#pragma warning(disable: 4365) // 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning(disable: 4514) // 'function' : unreferenced inline function has been removed
#pragma warning(disable: 4548) // expression before comma has no effect; expected expression with side-effect
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4577) // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#pragma warning(disable: 4623) // 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
#pragma warning(disable: 4625) // 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
#pragma warning(disable: 4626) // 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4710) // 'function' : function not inlined
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 5026) // 'type': move constructor was implicitly defined as deleted
#pragma warning(disable: 5027) // 'type': move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5039) // 'function': pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception.
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#ifndef _USRDLL
#define _USRDLL
#endif

// c style headers
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <strsafe.h>
#include <time.h>

// c++ headers
#include <algorithm>
#include <bitset>
#include <cmath>
#include <deque>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <list>
#include <map> // ?
#include <memory>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// windows headers
#include <Windows.h>
#include <windowsx.h>
#include <WinUser.h>
#include <ShellApi.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <dbghelp.h>

// libraries
#include <SimpleIni.h>
#include <json.hpp>
#include <easywsclient\easywsclient.hpp>

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
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 5027) // 'type': move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5029) // nonstandard extension used: alignment attributes in C++ apply to variables, data members and tag types only
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
