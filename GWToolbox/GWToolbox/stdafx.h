#pragma once

#ifndef __STDC__
# define __STDC__ 1
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS 1
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
# define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <set>
#include <list>
#include <deque>
#include <queue>
#include <regex>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <initializer_list>

#ifndef NOMINMAX
# define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif

#ifndef STRICT
# define STRICT
#endif

#include <Windows.h>
#include <ShellApi.h>

#include <d3d9.h>

#include <SimpleIni.h>

#include <imgui.h>
#include <imgui_internal.h>

#pragma warning(push)
#pragma warning(suppress : 4503)
#include <json.hpp>
#pragma warning(pop)
