#pragma once

#include <ToolboxModule.h>

#ifdef BUILD_DLL
#define DLLAPI extern "C" __declspec(dllexport)
#else
#define DLLAPI
#endif

DLLAPI ToolboxModule* getObj();

DLLAPI const char* getName();

