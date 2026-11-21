#pragma once

#include <memory>
#include <type_traits>

#include "TryCatchNative.h"

template <typename Operation>
bool TryCatch(Operation&& operation)
{
    struct Call {
        std::remove_reference_t<Operation>* operation;
        bool threw = false;
    } call{std::addressof(operation)};

    const auto completed = RunGuarded([](void* context) {
        auto& guarded = *static_cast<Call*>(context);
#if __cpp_exceptions
        try {
            (*guarded.operation)();
        }
        catch (...) {
            guarded.threw = true;
        }
#else
        (*guarded.operation)();
#endif
    }, &call, nullptr);
    return completed && !call.threw;
}

// on_fault sees every SEH exception while the faulting stack is intact; C++ exceptions end in std::terminate.
template <typename Operation>
bool RunWithFaultFilter(Operation&& operation, const FaultFilter on_fault)
{
    return RunGuarded([](void* context) noexcept {
        (*static_cast<std::remove_reference_t<Operation>*>(context))();
    }, std::addressof(operation), on_fault);
}
