#pragma once

#include "TryCatchNative.h"

#include <exception>
#include <memory>
#include <type_traits>

class TryCatchResult;

template <typename Operation>
bool TryCatch(Operation&& operation, TryCatchResult* result = nullptr);

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
    friend bool TryCatch(Operation&& operation, TryCatchResult* result);

    ToolboxFault fault{};
    std::exception_ptr cpp_exception;
};

// GCC hardware-fault recovery unwinds Windows SEH frames, not GCC C++ object destructors.
template <typename Operation>
bool TryCatch(Operation&& operation, TryCatchResult* result)
{
    if (result) {
        result->fault.record.ExceptionCode = 0;
        result->cpp_exception = nullptr;
    }
    struct Invocation {
        std::remove_reference_t<Operation>* operation;
        TryCatchResult* result;
        bool cpp_failed = false;
    } invocation{std::addressof(operation), result};

    const auto completed = ToolboxTryCatch([](void* context) {
        auto& call = *static_cast<Invocation*>(context);
        try {
            (*call.operation)();
        }
        catch (...) {
            call.cpp_failed = true;
            if (call.result) call.result->cpp_exception = std::current_exception();
        }
    }, &invocation, result ? &result->fault : nullptr);
    return completed && !invocation.cpp_failed;
}
