#pragma once

#include <GWCA/Utilities/Export.h>

#include <cstdint>

namespace GW {
    namespace Hook {

        // A code address on x86, a function index on wasm; not void*, since function-pointer-to-void* is ill-formed and clang rejects it.
        struct HookTarget {
            uintptr_t value = 0;
            constexpr HookTarget() = default;
            constexpr HookTarget(decltype(nullptr)) {}
            template <class T> HookTarget(T* p)
                : value((uintptr_t)p) {}
            constexpr explicit operator bool() const { return value != 0; }
        };

#if GWCA_WASM
        // The loader sizes the dispatch table with TableBytes and passes it in -- the no-arg form below has no wasm definition.
        GWCA_API size_t TableBytes(uint32_t first_func, uint32_t func_count);
        GWCA_API bool Initialize(void* memory, uint32_t first_func, uint32_t func_count);

        // Move one funcref between GWCA's table and the game's, for a callback written into a game data structure (see StoCMgr).
        GWCA_API uint32_t PublishFuncref(HookTarget local);
        GWCA_API uint32_t AdoptFuncref(uint32_t game_slot);

        // A scanned function made callable. A scan yields a tagged code offset,
        // which is not a table index -- calling one directly traps with "table
        // index is out of bounds". A hooked function gets a callable form for
        // free as its trampoline; anything called directly and never hooked has
        // to be converted here first. 0 if it does not resolve.
        GWCA_API uintptr_t CallableFromScan(uintptr_t scan_result);
#else
        GWCA_API void Initialize();

        // x86 scan results are already code addresses, so call sites can be unconditional.
        inline uintptr_t CallableFromScan(uintptr_t scan_result) { return scan_result; }
#endif
        GWCA_API void Deinitialize();

        // static void EnqueueHook(HookBase*); static void RemoveHook(HookBase*);

        GWCA_API void EnableHooks(HookTarget target = nullptr);
        GWCA_API void DisableHooks(HookTarget target = nullptr);

#if GWCA_WASM
        // A trampoline is only valid for call_indirect FROM WHICHEVER MODULE'S
        // OWN "table_adopt" import populated it -- every wasm module gets its
        // own table (see gw_in_browser's inject.js TableManager, one instance
        // per module), and CreateHook is code compiled into gwca.wasm. A mod
        // calling it cross-module got its trampoline written into GWCA's own
        // table, not the mod's; the mod's own call_indirect through it then
        // traps "table index is out of bounds" -- silently, since CreateHook
        // itself reports success (the adopt genuinely succeeded, just into
        // the wrong table). Found hooking a wasm test mod's own model-loading
        // experiment; every existing hook avoided this only because GWCA
        // hooks itself, so the code that creates the trampoline and the code
        // that later calls it are always the same module by construction.
        //
        // This parameter closes that hole: it is mandatory, not defaulted, so
        // a mod cannot compile a call to CreateHook without deciding what to
        // pass. Supply the SAME kind of import GWCA's own uses internally --
        // an `extern "C" __attribute__((import_module("env"),
        // import_name("table_adopt"))) uint32_t table_adopt(uint32_t);` declared
        // in the mod's own translation unit, so the loader resolves it against
        // the mod's own TableManager. GWCA's own 44 internal call sites use
        // GW::Hook::GwcaTableAdopt via the GWCA_CREATE_HOOK macro below rather
        // than repeat this by hand.
        typedef uint32_t (*TableAdoptFn)(uint32_t game_slot);

        // gwca's OWN table_adopt import, valid ONLY for a trampoline that will
        // be called from code compiled into gwca.wasm. Passing this from a mod
        // reproduces exactly the bug this parameter exists to prevent -- a mod
        // must supply its own import instead (see above).
        GWCA_API uint32_t GwcaTableAdopt(uint32_t game_slot);

        GWCA_API int CreateHook(void** target, HookTarget detour, void** trampoline,
                                 TableAdoptFn caller_table_adopt);

        // Prefer this form. The void** overload erases both signatures, so a
        // detour whose type differs from its target's compiles on either
        // platform: x86 ignores the difference, and wasm's call_indirect traps
        // at the first call -- in a map load, far from the declaration. Here the
        // two must deduce the same T, so a mismatch is a compile error instead.
        template <class T>
        int CreateHook(T* target, T detour, T* trampoline, TableAdoptFn caller_table_adopt)
        {
            return CreateHook((void**)target, HookTarget(detour), (void**)trampoline, caller_table_adopt);
        }

        // GWCA's own call sites: `GWCA_CREATE_HOOK(&Func, Detour, &Ret)`, no
        // different from the pre-parameter shape -- this is what supplies
        // GwcaTableAdopt on their behalf so 44 internal call sites did not
        // each need editing by hand. Not for mod code: a mod including this
        // header gets the real, mandatory-parameter CreateHook above, and
        // reaching for this macro to sidestep that is exactly the mistake the
        // parameter exists to prevent -- pass your own import, not this one.
        #define GWCA_CREATE_HOOK(target, detour, trampoline) \
            GW::Hook::CreateHook(target, detour, trampoline, GW::Hook::GwcaTableAdopt)
#else
        GWCA_API int CreateHook(void** target, HookTarget detour, void** trampoline);

        // Prefer this form. The void** overload erases both signatures, so a
        // detour whose type differs from its target's compiles on either
        // platform: x86 ignores the difference, and wasm's call_indirect traps
        // at the first call -- in a map load, far from the declaration. Here the
        // two must deduce the same T, so a mismatch is a compile error instead.
        template <class T>
        int CreateHook(T* target, T detour, T* trampoline)
        {
            return CreateHook((void**)target, HookTarget(detour), (void**)trampoline);
        }

        // x86 has no wasm-style cross-module trampoline trap, so this is just
        // CreateHook -- same macro name on both platforms so GWCA's own 44
        // internal call sites do not need an #if.
        #define GWCA_CREATE_HOOK(target, detour, trampoline) \
            GW::Hook::CreateHook(target, detour, trampoline)
#endif
        GWCA_API void RemoveHook(HookTarget target);

        GWCA_API void EnterHook();
        GWCA_API void LeaveHook();
        GWCA_API int  GetInHookCount();
    }
}
