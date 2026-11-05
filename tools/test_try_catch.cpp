#include "../GWToolboxdll/Utils/TryCatch.h"

#include <stdexcept>

int main()
{
    auto completed = false;
    const auto success = TryCatch([&] { completed = true; });
    if (!completed || !success) return 1;

    TryCatchResult result;
    if (TryCatch([] { throw std::runtime_error("expected"); }, &result)) return 2;
    if (!result.HasException() || !result.CppException() || result.ExceptionCode()) return 2;
    try {
        std::rethrow_exception(result.CppException());
    }
    catch (const std::runtime_error&) {
    }
    catch (...) {
        return 3;
    }

    auto* memory = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    if (!memory) return 4;
    const auto read = [memory] {
        const auto value = *static_cast<volatile unsigned char*>(memory);
        (void)value;
    };

    const auto fault_succeeded = TryCatch(read, &result);
    auto copy = result;
    auto pointers = copy.ExceptionPointers();
    const auto captured = !fault_succeeded && !copy.CppException() && copy.HasException() && copy.ExceptionCode() == EXCEPTION_ACCESS_VIOLATION
        && pointers.ExceptionRecord && pointers.ContextRecord
        && !pointers.ExceptionRecord->ExceptionRecord
        && pointers.ExceptionRecord->NumberParameters >= 2
        && pointers.ExceptionRecord->ExceptionInformation[1] == reinterpret_cast<ULONG_PTR>(memory)
        && pointers.ContextRecord->Eip != 0;

    auto nested = false;
    const auto outer = TryCatch([&] {
        TryCatchResult inner;
        nested = !TryCatch(read, &inner) && inner.ExceptionCode() == EXCEPTION_ACCESS_VIOLATION;
    });
    const auto omitted_result = TryCatch(read);
    const auto omitted_cpp_result = TryCatch([] { throw std::runtime_error("expected"); });
    const auto subsequent = TryCatch([] {}, &result);
    VirtualFree(memory, 0, MEM_RELEASE);

    if (!captured) return 5;
    if (!nested || !outer) return 6;
    if (!subsequent || result.HasException() || result.ExceptionCode() || result.CppException()) return 7;
    const auto reset_pointers = result.ExceptionPointers();
    if (reset_pointers.ExceptionRecord || reset_pointers.ContextRecord) return 8;
    if (omitted_result || omitted_cpp_result) return 9;
    return 0;
}
