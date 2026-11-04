#pragma once

#include <Windows.h>

template <typename Function = FARPROC>
Function GetModuleFunctionW(HMODULE module, const char* name)
{
    return module ? reinterpret_cast<Function>(GetProcAddress(module, name)) : nullptr;
}

template <typename Function = FARPROC>
Function GetModuleFunctionW(const wchar_t* module_name, const char* name)
{
    return GetModuleFunctionW<Function>(GetModuleHandleW(module_name), name);
}
