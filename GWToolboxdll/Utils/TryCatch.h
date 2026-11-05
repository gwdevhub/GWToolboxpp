#pragma once

#include "TryCatchNative.h"

#include <exception>
#include <memory>
#include <type_traits>

class TryCatchResult {
public:
    bool HasException() const noexcept
    {
        return fault.record.ExceptionCode != 0 || cpp_exception != nullptr;
    }

    DWORD ExceptionCode() const noexcept
    {
        return fault.record.ExceptionCode;
    }

    EXCEPTION_POINTERS ExceptionPointers() noexcept
    {
        if (!fault.record.ExceptionCode) return {};
        return {&fault.record, &fault.context};
    }

    const std::exception_ptr& CppException() const noexcept
    {
        return cpp_exception;
    }

private:
    template <typename Operation>
    friend TryCatchResult TryCatch(Operation&& operation);

    ToolboxFault fault{};
    std::exception_ptr cpp_exception;
};

// GCC hardware-fault recovery unwinds Windows SEH frames, not GCC C++ object destructors.
template <typename Operation>
TryCatchResult TryCatch(Operation&& operation)
{
    TryCatchResult result;
    struct Invocation {
        std::remove_reference_t<Operation>* operation;
        std::exception_ptr* exception;
    } invocation{std::addressof(operation), &result.cpp_exception};

    ToolboxTryCatch([](void* context) {
        auto& call = *static_cast<Invocation*>(context);
        try {
            (*call.operation)();
        }
        catch (...) {
            *call.exception = std::current_exception();
        }
    }, &invocation, &result.fault);
    return result;
}
