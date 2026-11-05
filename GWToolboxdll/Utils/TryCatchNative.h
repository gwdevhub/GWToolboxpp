#pragma once

#include <windows.h>

typedef struct ToolboxFault {
    EXCEPTION_RECORD record;
    CONTEXT context;
} ToolboxFault;

#ifdef __cplusplus
extern "C" {
#endif

void __cdecl ToolboxTryCatch(void (__cdecl *operation)(void*), void* context, ToolboxFault* fault);

#ifdef __cplusplus
}
#endif
