#include "TryCatchNative.h"

#include <excpt.h>

namespace {
    bool IsHardwareFault(const DWORD code)
    {
        switch (code) {
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
                return true;
            default:
                return false;
        }
    }

    LONG Filter(EXCEPTION_POINTERS* pointers, const FaultFilter on_fault)
    {
        if (on_fault) return on_fault(pointers);
        return IsHardwareFault(pointers->ExceptionRecord->ExceptionCode) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
    }
}

#if defined(_MSC_VER)

bool RunGuarded(void(__cdecl* operation)(void*), void* context, const FaultFilter on_fault)
{
    __try {
        operation(context);
        return true;
    } __except (Filter(GetExceptionInformation(), on_fault)) {
        return false;
    }
}

#elif defined(__GNUC__) && defined(__i386__)

#include <setjmp.h>

namespace {
    // GCC has no __try, so this registers a frame on the x86 SEH chain (fs:0) by hand; unwound frames don't run C++ destructors.
    struct FaultFrame {
        FaultFrame* previous;
        EXCEPTION_DISPOSITION(__cdecl* handler)(EXCEPTION_RECORD*, void*, CONTEXT*, void*);
        jmp_buf jump;
        FaultFilter on_fault;
    };

    EXCEPTION_DISPOSITION __cdecl HandleFault(EXCEPTION_RECORD* record, void* registration, CONTEXT* context, void*)
    {
        const auto frame = static_cast<FaultFrame*>(registration);
        if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) return ExceptionContinueSearch;
        EXCEPTION_POINTERS pointers{record, context};
        if (Filter(&pointers, frame->on_fault) != EXCEPTION_EXECUTE_HANDLER) return ExceptionContinueSearch;
        RtlUnwind(frame, nullptr, record, nullptr);
        longjmp(frame->jump, 1);
    }
}

bool RunGuarded(void(__cdecl* operation)(void*), void* context, const FaultFilter on_fault)
{
    FaultFrame frame{};
    __asm__ __volatile__("movl %%fs:0, %0" : "=r"(frame.previous));
    frame.handler = HandleFault;
    frame.on_fault = on_fault;
    if (setjmp(frame.jump) == 0) {
        __asm__ __volatile__("movl %0, %%fs:0" : : "r"(&frame) : "memory");
        operation(context);
        __asm__ __volatile__("movl %0, %%fs:0" : : "r"(frame.previous) : "memory");
        return true;
    }
    __asm__ __volatile__("movl %0, %%fs:0" : : "r"(frame.previous) : "memory");
    return false;
}

#else
#error RunGuarded needs MSVC SEH or 32-bit GCC
#endif
