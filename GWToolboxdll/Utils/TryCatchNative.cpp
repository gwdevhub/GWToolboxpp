#include "TryCatchNative.h"
#include <excpt.h>

static int CaptureFault(EXCEPTION_RECORD* record, CONTEXT* context, ToolboxFault* fault)
{
    switch (record->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INEXACT_RESULT:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_STACK_CHECK:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            if (fault) {
                fault->record = *record;
                fault->record.ExceptionRecord = NULL;
                fault->context = *context;
                fault->context.ContextFlags &= CONTEXT_FULL | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS | CONTEXT_EXTENDED_REGISTERS;
            }
            return EXCEPTION_EXECUTE_HANDLER;
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
}

#if defined(_MSC_VER)

BOOL __cdecl ToolboxTryCatch(void (__cdecl *operation)(void*), void* context, ToolboxFault* fault)
{
    __try {
        operation(context);
        return TRUE;
    }
    __except (CaptureFault(GetExceptionInformation()->ExceptionRecord, GetExceptionInformation()->ContextRecord, fault)) {
        return FALSE;
    }
}

#elif defined(__GNUC__) && defined(__i386__)

#include <setjmp.h>

typedef struct FaultFrame {
    struct FaultFrame* previous;
    EXCEPTION_DISPOSITION (__cdecl *handler)(EXCEPTION_RECORD*, void*, CONTEXT*, void*);
    jmp_buf jump;
    ToolboxFault* fault;
} FaultFrame;

static EXCEPTION_DISPOSITION __cdecl HandleFault(EXCEPTION_RECORD* record, void* registration, CONTEXT* context, void* dispatcher)
{
    auto* frame = static_cast<FaultFrame*>(registration);
    (void)dispatcher;
    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        return ExceptionContinueSearch;
    if (CaptureFault(record, context, frame->fault) != EXCEPTION_EXECUTE_HANDLER)
        return ExceptionContinueSearch;

    RtlUnwind(frame, NULL, record, NULL);
    longjmp(frame->jump, 1);
}

BOOL __cdecl ToolboxTryCatch(void (__cdecl *operation)(void*), void* context, ToolboxFault* fault)
{
    FaultFrame frame;
    __asm__ __volatile__("movl %%fs:0, %0" : "=r" (frame.previous));
    frame.handler = HandleFault;
    frame.fault = fault;
    if (setjmp(frame.jump) == 0) {
        __asm__ __volatile__("movl %0, %%fs:0" : : "r" (&frame) : "memory");
        operation(context);
        __asm__ __volatile__("movl %0, %%fs:0" : : "r" (frame.previous) : "memory");
        return TRUE;
    }
    __asm__ __volatile__("movl %0, %%fs:0" : : "r" (frame.previous) : "memory");
    return FALSE;
}

#else
#error TryCatch requires native MSVC SEH or 32-bit Windows GCC.
#endif
