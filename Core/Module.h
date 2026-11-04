#pragma once

#include <Windows.h>

FARPROC GetModuleFunctionW(HMODULE module, const char* name);
FARPROC GetModuleFunctionW(const wchar_t* module_name, const char* name);
