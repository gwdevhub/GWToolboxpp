#include "stdafx.h"

#include "Module.h"

FARPROC GetModuleFunctionW(HMODULE module, const char* name)
{
    return module ? GetProcAddress(module, name) : nullptr;
}

FARPROC GetModuleFunctionW(const wchar_t* module_name, const char* name)
{
    return GetModuleFunctionW(GetModuleHandleW(module_name), name);
}
