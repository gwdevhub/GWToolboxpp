#include "../GWToolboxdll/Utils/TryCatch.h"

#include <stdexcept>

int main()
{
    auto completed = false;
    const auto success = TryCatch([&] { completed = true; });
    if (!completed || success.HasException()) return 1;

    const auto cpp = TryCatch([] { throw std::runtime_error("expected"); });
    if (!cpp.HasException() || !cpp.CppException() || cpp.ExceptionCode()) return 2;
    try {
        std::rethrow_exception(cpp.CppException());
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

    auto fault = TryCatch(read);
    auto copy = fault;
    auto pointers = copy.ExceptionPointers();
    const auto captured = copy.HasException() && copy.ExceptionCode() == EXCEPTION_ACCESS_VIOLATION
        && pointers.ExceptionRecord && pointers.ContextRecord
        && !pointers.ExceptionRecord->ExceptionRecord
        && pointers.ExceptionRecord->NumberParameters >= 2
        && pointers.ExceptionRecord->ExceptionInformation[1] == reinterpret_cast<ULONG_PTR>(memory)
        && pointers.ContextRecord->Eip != 0;

    auto nested = false;
    const auto outer = TryCatch([&] {
        const auto inner = TryCatch(read);
        nested = inner.ExceptionCode() == EXCEPTION_ACCESS_VIOLATION;
    });
    const auto subsequent = TryCatch([] {});
    VirtualFree(memory, 0, MEM_RELEASE);

    if (!captured) return 5;
    if (!nested || outer.HasException()) return 6;
    if (subsequent.HasException()) return 7;
    return 0;
}
