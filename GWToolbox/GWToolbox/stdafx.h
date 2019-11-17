#pragma once

#include <GWCA/Source/stdafx.h>

#ifndef WIN32
#define WIN32
#endif

#ifndef _WINDOWS
#define _WINDOWS
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#ifndef _USRDLL
#define _USRDLL
#endif

// c style headers
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <strsafe.h>
#include <assert.h>

// c++ headers
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <list>
#include <map> // ?
#include <memory>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
