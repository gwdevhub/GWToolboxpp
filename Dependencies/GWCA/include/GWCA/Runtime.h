#pragma once

#include <GWCA/Utilities/Export.h>
#include <cstdarg>
#include <utility>

#if GWCA_WASM
#define GWCA_CALL_C(name, ...) name(__VA_ARGS__)
#else

#include <GWCA/RuntimeLayout.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdlib>
#include <type_traits>

namespace GW {
    struct Vec2f;
    struct Vec3f;
    struct GamePos;
    struct GHKey;
    template <typename T> class BaseArray;
    namespace SkillbarMgr { struct SkillTemplate; }
    namespace Hook { struct HookTarget; }
}

namespace GW::Runtime {
    template <typename T> struct OwnedArray : std::false_type {};
    template <typename T> struct OwnedArray<BaseArray<T>> : std::true_type {};
    template <typename T>
    inline constexpr bool OwnedRecord = std::is_same_v<T, Vec2f> || std::is_same_v<T, Vec3f>
        || std::is_same_v<T, GamePos> || std::is_same_v<T, GHKey>
        || std::is_same_v<T, SkillbarMgr::SkillTemplate> || std::is_same_v<T, Hook::HookTarget>;

    template <typename T>
    inline constexpr bool NativeValue = std::is_void_v<T> || std::is_integral_v<T> || std::is_enum_v<T>
        || std::is_pointer_v<T> || std::is_same_v<T, float> || std::is_same_v<T, double>
        || (OwnedRecord<std::remove_cvref_t<T>> && std::is_trivially_copyable_v<std::remove_cvref_t<T>>)
        || (std::is_reference_v<T> && OwnedArray<std::remove_cvref_t<T>>::value);

    template <typename T> struct NativeSignature : std::false_type {};
    template <typename Result, typename... Args>
    struct NativeSignature<Result (__cdecl *)(Args...) noexcept>
        : std::bool_constant<NativeValue<Result> && (NativeValue<Args> && ...)> {};

    using Address = void (__cdecl *)();

    struct Bindings {
        Address functions[ExportCount]{};
    };

    struct alignas(8) Request {
        uint32_t size;
        uint32_t abi;
        uint64_t schema;
        uint32_t pointer_size;
        uint32_t reserved;
    };
    static_assert(sizeof(Request) == 24);
    static_assert(offsetof(Request, schema) == 8);
    static_assert(offsetof(Request, pointer_size) == 16);

    enum class Status : uint32_t {
        Success,
        InvalidArgument,
        IncompatibleApi,
        IncompatibleArchitecture,
        MissingExport
    };

    using ValidateApi = Status (__cdecl *)(const Request*);

    inline Bindings api{};

    inline Request ClientRequest()
    {
        return {sizeof(Request), AbiVersion, Schema, sizeof(void*), 0};
    }

    inline Status BindGWCA(HMODULE module, Bindings* bindings)
    {
        if (!module || !bindings) return Status::InvalidArgument;
        const auto validate = reinterpret_cast<ValidateApi>(GetProcAddress(module, "GWCA_ValidateApi"));
        if (!validate) return Status::MissingExport;
        const auto request = ClientRequest();
        const auto status = validate(&request);
        if (status != Status::Success) return status;
        Bindings resolved{};
        for (size_t i = 0; i < ExportCount; ++i) {
            if (!ExportNames[i]) continue;
            resolved.functions[i] = reinterpret_cast<Address>(GetProcAddress(module, ExportNames[i]));
            if (!resolved.functions[i]) return Status::MissingExport;
        }
        *bindings = resolved;
        return Status::Success;
    }

    inline HMODULE LoadGWCA(const wchar_t* path)
    {
        const auto module = LoadLibraryW(path);
        if (!module) return nullptr;
        const auto status = BindGWCA(module, &api);
        if (status != Status::Success) {
            FreeLibrary(module);
            SetLastError(status == Status::MissingExport ? ERROR_PROC_NOT_FOUND : ERROR_REVISION_MISMATCH);
            return nullptr;
        }
        return module;
    }

    template <typename FunctionType>
    FunctionType Function(Export name)
    {
        static_assert(NativeSignature<FunctionType>::value, "C exports must use native ABI types");
        const auto index = static_cast<size_t>(name);
        if (index >= ExportCount || !api.functions[index]) std::abort();
        return reinterpret_cast<FunctionType>(api.functions[index]);
    }
}

#define GWCA_CALL_C(name, ...) ::GW::Runtime::Function<decltype(&name)>(::GW::Runtime::Export::name)(__VA_ARGS__)

#endif
