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

#include "resource.h"
