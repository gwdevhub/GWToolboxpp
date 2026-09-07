// GW::Hook wasm backend -- same six entry points as Win32, but arming a hook is one store into a table. See README.md.

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>   // resolve a ToFunctionStart address -> index
#include <GWCA/Utilities/Debug.h>     // every CreateHook bail-out names itself

#include <atomic>
#include <cstdint>
#include <cstring>

// Detour out and trampoline back both cross gwca's table and the game's; only the loader can move a funcref between them.
extern "C" {
    // gwca-local function index -> a slot in the game's table holding it.
    __attribute__((import_module("env"), import_name("table_publish")))
    uint32_t table_publish(uint32_t local_index);

    // A slot in the game's table -> a gwca-local index holding that function.
    __attribute__((import_module("env"), import_name("table_adopt")))
    uint32_t table_adopt(uint32_t game_slot);

    // The dispatchers read a mutable wasm global we cannot store to, so push the base to the loader rather than mirror it.
    __attribute__((import_module("env"), import_name("set_hook_base")))
    void set_hook_base(uint32_t base);
}

extern "C" {
    // Emitted by the detour pass: the hook-table base (0 disables every hook) and where the originals were parked.
    extern uint32_t __hook_base;
    extern const uint32_t __orig_slot_base;
}

namespace {
    std::atomic<int> init_count;
    std::atomic<int> in_hook_count;

    uint32_t* live = nullptr;       // live[f]    -> detour slot, 0 = inactive
    uint32_t* pending = nullptr;    // pending[f] -> detour slot registered
    uint32_t  func_base = 0;        // first defined (non-imported) index
    uint32_t  slot_count = 0;       // entries in each table

    inline bool InRange(uint32_t f)
    {
        return f >= func_base && f < slot_count;
    }

    inline uint32_t OrigSlot(uint32_t f)
    {
        return __orig_slot_base + (f - func_base);
    }

    // Scans land in the parked-original range; fold down by n to the dispatcher index the hook table is keyed on.
    inline uint32_t FoldParkedOriginal(uint32_t f)
    {
        const uint32_t n = slot_count - func_base;      // nDefined
        if (f >= slot_count && f - slot_count < n) return f - n;
        return f;
    }

    // The one place a scan result becomes a hookable index -- all four entry points below must fold.
    inline uint32_t ResolveTarget(uintptr_t scan_result)
    {
        return FoldParkedOriginal(
            (uint32_t)GW::Scanner::FunctionAtCodeOffset(scan_result));
    }
}

namespace GW {
    namespace Hook {

        // MH_Initialize analogue; memory must hold TableBytes(first_func, func_count) * 2 and is owned by the caller.
        size_t TableBytes(uint32_t first_func, uint32_t func_count)
        {
            return (size_t)(first_func + func_count) * sizeof(uint32_t);
        }

        bool Initialize(void* memory, uint32_t first_func, uint32_t func_count)
        {
            if (!memory || !func_count) {
                GWCA_ERR("[HOOK] Initialize refused: memory=%p func_count=%u",
                         memory, func_count);
                return false;
            }
            if (++init_count != 1) {
                GWCA_INFO("[HOOK] Initialize called again (count=%d), keeping "
                          "func_base=%u slot_count=%u",
                          (int)init_count, func_base, slot_count);
                return true;
            }

            func_base = first_func;
            slot_count = first_func + func_count;
            live = (uint32_t*)memory;
            pending = live + slot_count;
            memset(live, 0, TableBytes(first_func, func_count) * 2);
            __hook_base = (uint32_t)(uintptr_t)live;
            set_hook_base((uint32_t)(uintptr_t)live);   // the dispatchers' copy
            // The values every later CreateHook failure is judged against, and not visible from JS.
            GWCA_INFO("[HOOK] Initialize: func_base=%u func_count=%u slot_count=%u "
                      "live=0x%x pending=0x%x orig_slot_base=%u",
                      func_base, func_count, slot_count,
                      (unsigned)(uintptr_t)live, (unsigned)(uintptr_t)pending,
                      __orig_slot_base);
            return true;
        }

        void Deinitialize()
        {
            if (--init_count == 0) {
                __hook_base = 0;       // one store disables everything
                set_hook_base(0);      // ...and the dispatchers' copy too
                live = pending = nullptr;
                func_base = slot_count = 0;
            }
        }

        void EnterHook() { ++in_hook_count; }
        void LeaveHook() { --in_hook_count; }
        int GetInHookCount() { return in_hook_count; }

        // gwca's own table_adopt import, for GWCA_CREATE_HOOK (see Hooker.h)
        // to hand GWCA's own 44 internal call sites -- valid only for a
        // trampoline that will be called from code compiled into gwca.wasm.
        uint32_t GwcaTableAdopt(uint32_t game_slot)
        {
            return table_adopt(game_slot);
        }

        // MinHook signature, registered disabled. Each failure path names itself rather than returning an indistinguishable -1.
        // caller_table_adopt: see Hooker.h -- must be the CALLING module's own
        // "table_adopt" import, not gwca's, or the trampoline this hands back
        // will trap "table index is out of bounds" the instant the caller's
        // own code tries to call through it.
        int CreateHook(void** target, HookTarget detour, void** trampoline, TableAdoptFn caller_table_adopt)
        {
            if (!(target && *target)) {
                GWCA_ERR("[HOOK] CreateHook: null target (target=%p, *target=0x%x) "
                         "-- the scan that produced it failed",
                         (void*)target,
                         (unsigned)(target ? (uintptr_t)*target : 0));
                return -1;
            }
            if (!live) {
                GWCA_ERR("[HOOK] CreateHook(0x%x): hooker not initialised -- the "
                         "loader never called GW::Hook::Initialize(memory, "
                         "first_func, func_count), so no hook can be registered",
                         (unsigned)(uintptr_t)*target);
                return -1;
            }
            // *target is a scan result -- resolve it here, at the hook, and fail if it is not a real function.
            const uintptr_t raw = (uintptr_t)*target;
            const uint32_t resolved = (uint32_t)GW::Scanner::FunctionAtCodeOffset(raw);
            const uint32_t f = FoldParkedOriginal(resolved);
            const uint32_t slot = (uint32_t)detour.value;
            if (!InRange(f)) {
                // f == 0 means the offset fell outside every body; out of range means an import, or past both ranges.
                GWCA_ERR("[HOOK] CreateHook: scan result 0x%x resolved to function "
                         "index %u (folded to %u), outside the hookable range "
                         "[%u, %u) -- %s",
                         (unsigned)raw, resolved, f, func_base, slot_count,
                         f ? "resolved to an import or out-of-module index"
                           : "code offset is inside no function body");
                return -1;
            }
            if (f != resolved) {
                GWCA_TRACE("[HOOK] CreateHook: scan landed in parked original %u, "
                           "folded to dispatcher %u", resolved, f);
            }
            if (!slot) {
                GWCA_ERR("[HOOK] CreateHook(func %u): detour is 0 -- the detour "
                         "function has no gwca table index", f);
                return -1;
            }
            if (pending[f]) {
                GWCA_ERR("[HOOK] CreateHook(func %u): already hooked (pending "
                         "slot %u) -- two managers resolved the same function",
                         f, pending[f]);
                return -1;
            }

            // Zero means the loader could not place it -- reported, not stored, since a zero entry reads as "not hooked".
            const uint32_t game_slot = table_publish(slot);
            if (!game_slot) {
                GWCA_ERR("[HOOK] CreateHook(func %u): table_publish(%u) "
                         "returned 0 -- the loader could not place the detour in "
                         "the game table (reserved headroom exhausted?)", f, slot);
                return -1;
            }

            // Adopt the original before arming, so the trampoline is valid the instant the hook goes live.
            if (trampoline) {
                if (!caller_table_adopt) {
                    GWCA_ERR("[HOOK] CreateHook(func %u): trampoline requested "
                             "but caller_table_adopt is null -- pass the CALLING "
                             "module's own table_adopt import (GW::Hook::GwcaTableAdopt "
                             "if this is GWCA's own code, via GWCA_CREATE_HOOK), or "
                             "pass a null trampoline if nothing will call through", f);
                    return -1;
                }
                const uint32_t orig = OrigSlot(f);
                const uint32_t adopted = caller_table_adopt(orig);
                if (!adopted) {
                    GWCA_ERR("[HOOK] CreateHook(func %u): table_adopt(%u) "
                             "returned 0 -- orig_slot_base=%u + (%u - %u); the "
                             "detour pass did not park an original for this "
                             "function, or that slot is empty in the game table",
                             f, orig, __orig_slot_base, f, func_base);
                    return -1;
                }
                *trampoline = (void*)(uintptr_t)adopted;
            }

            pending[f] = game_slot;
            live[f] = 0;
            GWCA_INFO("[HOOK] CreateHook(func %u): detour gwca slot %u -> game "
                      "slot %u, trampoline %u (registered disabled)",
                      f, slot, game_slot,
                      trampoline ? (unsigned)(uintptr_t)*trampoline : 0u);
            return 0;
        }

        // A null target means all hooks, matching the Win32 backend's MH_ALL_HOOKS forwarding.
        void EnableHooks(HookTarget target)
        {
            if (!live) return;
            if (!target) {
                for (uint32_t f = func_base; f < slot_count; ++f)
                    live[f] = pending[f];
                return;
            }
            const uint32_t f = ResolveTarget(target.value);
            if (InRange(f)) live[f] = pending[f];
            else GWCA_WARN("[HOOK] EnableHooks: target 0x%x is not a hookable "
                           "function (%u)", (unsigned)target.value, f);
        }

        void DisableHooks(HookTarget target)
        {
            if (!live) return;
            if (!target) {
                memset(live + func_base, 0,
                       (slot_count - func_base) * sizeof(uint32_t));
                return;
            }
            const uint32_t f = ResolveTarget(target.value);
            if (InRange(f)) live[f] = 0;
            else GWCA_WARN("[HOOK] DisableHooks: target 0x%x is not a hookable "
                           "function (%u)", (unsigned)target.value, f);
        }

        // Unlike Disable, this drops the registration so a later EnableHooks will not resurrect the detour.
        void RemoveHook(HookTarget target)
        {
            if (!live || !target) return;
            const uint32_t f = ResolveTarget(target.value);
            if (!InRange(f)) {
                GWCA_WARN("[HOOK] RemoveHook: target 0x%x is not a hookable "
                          "function (%u)", (unsigned)target.value, f);
                return;
            }
            live[f] = 0;
            pending[f] = 0;
        }

        // wasm-only funcref move between tables, for a manager that installs a callback into a game data structure.
        uint32_t PublishFuncref(HookTarget local)
        {
            return table_publish((uint32_t)local.value);
        }

        uint32_t AdoptFuncref(uint32_t game_slot)
        {
            return table_adopt(game_slot);
        }

        // Same derivation CreateHook uses for a trampoline: the detour pass parks
        // every original at a known game-table slot, so any function GWCA can
        // resolve can also be adopted -- it does not have to be hooked first.
        uintptr_t CallableFromScan(uintptr_t scan_result)
        {
            if (!live || !scan_result)
                return 0;
            // Two different indices, and they are not interchangeable: `resolved`
            // is where the scan landed -- the parked original, the copy that still
            // holds the code, globals and strings -- while `f` is its dispatcher,
            // which is the ~30 byte stub the hook table is keyed on.
            const uint32_t resolved = (uint32_t)GW::Scanner::FunctionAtCodeOffset(scan_result);
            const uint32_t f = FoldParkedOriginal(resolved);
            if (!InRange(f)) {
                GWCA_ERR("[HOOK] CallableFromScan(0x%x): resolved to index %u, "
                         "outside [%u, %u)", (unsigned)scan_result, f, func_base, slot_count);
                return 0;
            }
            const uint32_t adopted = table_adopt(OrigSlot(f));
            if (!adopted) {
                GWCA_ERR("[HOOK] CallableFromScan(0x%x): table_adopt(%u) returned 0 "
                         "for func %u -- no original parked for it",
                         (unsigned)scan_result, OrigSlot(f), f);
                return 0;
            }
            // Record `resolved`, NOT `f`: everything that looks a scan result up
            // again -- GlobalFromFunction, CheckArity, BssConstFromFunction --
            // wants the parked original, because the dispatcher stub references no
            // globals, constants or strings. CreateHook folds it itself.
            GW::Scanner::NoteCallable(adopted, resolved);
            return adopted;
        }

    }  // namespace Hook
}  // namespace GW
