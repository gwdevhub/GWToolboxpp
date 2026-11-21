#pragma once

#include <windows.h>

using FaultFilter = LONG(__cdecl*)(EXCEPTION_POINTERS* pointers);

bool RunGuarded(void(__cdecl* operation)(void* context), void* context, FaultFilter on_fault);
