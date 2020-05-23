#pragma once

#pragma warning(push)
#pragma warning(disable: 4061) // enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
#pragma warning(disable: 4365) // 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4577) // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#pragma warning(disable: 4623) // 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
#pragma warning(disable: 4625) // 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
#pragma warning(disable: 4626) // 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
#pragma warning(disable: 4710) // 'function' : function not inlined
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 5026) // 'type': move constructor was implicitly defined as deleted
#pragma warning(disable: 5027) // 'type': move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5039) // 'function': pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception.
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>
#include <Commctrl.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <WinInet.h>
#include <psapi.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>

#include <json.hpp>

#include "resource.h"
#pragma warning(pop)

#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable: 4514) // 'function' : unreferenced inline function has been removed
#pragma warning(disable: 4505) // 'function' : unreferenced local function has been removed
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4577) // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#pragma warning(disable: 4710) // 'function' : function not inlined
#pragma warning(disable: 4711) // function 'function' selected for inline expansion
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
