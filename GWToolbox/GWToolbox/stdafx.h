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
#include <cmath>
#include <list>
#include <vector>
#include <map> // ?
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <deque>
#include <string>
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

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
