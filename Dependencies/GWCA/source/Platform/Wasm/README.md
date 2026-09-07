# Building GWCA for wasm

Produces an Emscripten **side module** (`gwca.wasm`): it imports the game
module's memory and indirect function table rather than owning its own, so
GWCA dereferences game pointers directly instead of going through accessors.
Its statics relocate at load time into memory obtained from the game's own
`malloc`, so both modules share one heap.

## Windows

1. **emsdk** — needs Python 3.10+ (`emsdk` bundles its own if the system one
   is older):

   ```bat
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   emsdk install latest
   emsdk activate latest
   emsdk_env.bat
   ```

   `emsdk_env.bat` sets `EMSDK`, which is what the preset reads. It is
   per-shell — run it in every new terminal, or use `emsdk activate --permanent`.

2. **Ninja** — the Visual Studio generators cannot drive `emcc`, so the preset
   uses Ninja. It ships with Visual Studio; a "Developer Command Prompt for VS"
   already has it on PATH. Otherwise `winget install Ninja-build.Ninja`.

3. **Build**:

   ```bat
   cmake --preset wasm
   cmake --build --preset wasm
   ```

No `emcmake` wrapper: the preset points `CMAKE_TOOLCHAIN_FILE` at
`Emscripten.cmake` itself, which is all `emcmake` does. Output lands in
`build-wasm/`.

## Linux / macOS

Identical, with `source ./emsdk_env.sh` instead of `emsdk_env.bat`.

## Gotchas

- **`emcmake cmake -B build-wasm` also works**, but only with a Makefile
  generator. With Ninja, `CMAKE_CXX_STANDARD 23` triggers C++20 module
  dependency scanning and `emscan-deps` cannot answer it, so every compile
  fails before it starts. The CMakeLists sets `CXX_SCAN_FOR_MODULES OFF` for
  this target because GWCA uses no modules.
- **The 32-bit guard passes.** `CMAKE_SIZEOF_VOID_P EQUAL 8` is the x86 check
  and wasm32 pointers are 4 bytes, so no `-A Win32` equivalent is needed.
- **`-fshort-wchar` is required, not cosmetic.** clang defaults `wchar_t` to 4
  bytes and MSVC uses 2; without it every struct holding a name doubles in size
  and trips its own `static_assert`. Two bytes is also correct — the game
  stores wide strings as UTF-16. Consequence: emscripten's libc was built with
  4-byte `wchar_t`, so its `wcs*` stride two of our characters at a time —
  `wcslen` returns half the length, `wcschr` and `wcsstr` never match. That is
  silent: it compiles and links, and only the results are wrong.

  `WideChar.cpp` therefore defines `wcslen`, `wcschr`, `wcsrchr`, `wcscmp`,
  `wcsncmp`, `wmemcmp`, `wcscpy`, `wcsncpy`, `wcsstr` and `wcstol` at our width.
  The linker takes them over libc's because an archive member is only pulled in
  for a symbol still undefined, so no duplicate-symbol conflict arises. The file
  must be compiled `-fno-builtin`, or clang recognises each loop and replaces
  the body with a call to itself. With it in the link, `wcs*` may be used
  normally throughout GWCA; before it, roughly fifty call sites across `UIMgr`,
  `Frame`, `Scanner`, `FriendListMgr` and `ChatMgr` were quietly wrong.
- **MinHook is not built.** There is no runtime code patching on wasm; see
  `Hooker.cpp` here and the build-time detour pass in the `gw_wasm` repo.
- **Platform.h and Win32Shim.h live under `Source/`, not `Include/`**, because
  they are not in the release bundle and `stdafx.h` ships. `GWCA_WASM` comes
  from the build as a compile definition so public headers can test it without
  needing either file.

## State

29 of 34 translation units compile. RenderMgr is the holdout: its D3D9 device
hooks have no counterpart, since the wasm client renders through WebGL. It
needs per-function gating rather than just gating the include -- stubbing the
device accessors would hand callers a null `IDirect3DDevice9` to dereference.

It does not link yet, and the managers' `#if GWCA_WASM` scan branches are still
empty, so nothing resolves at runtime.

## What a scan result means

A running wasm module's code lives in the code section, which is not mapped into
linear memory and has no address — nothing in the instruction set reaches it. The
loader fetched `Gw.wasm` over HTTP, so the module *bytes* are available;
`GWCA::Initialise` copies them into linear memory and passes them to the scanner.
Code is immutable after instantiation, so those bytes are the running code exactly.

The x86 scanners return an address you can cast to a function pointer. This one
returns a wasm **function index**. Both travel in `uintptr_t` so shared call sites
compile, but they are different things: an index cannot be dereferenced and is
only callable via the indirect table. `GW::Hook` takes indices directly, so the
difference stays invisible along the hooking path.

### What ports

| primitive | status |
|---|---|
| `FindUseOfString`, `FindNthUseOfString` | unchanged — strings survive in `.data` |
| `FindAssertion` | ports, after path normalisation (Win32 → relative, forward slashes) |
| `ToFunctionStart` | returns the enclosing function's start as a tagged code offset |
| `IsValidPtr` | ports, reinterpreted per section (RDATA and DATA are one region) |
| `FunctionFromNearCall` | lands cleaner than x86 — `call` carries the callee's funcidx, no rel32 to decode |
| `Find`, `FindInRange` | work, but every x86 pattern must be re-derived against wasm bytecode |
| `GetGameTlsIndex` | dead — single-threaded, no TLS |

Roughly 180 of ~280 scan call sites survive. The dead ones do not fail quietly:
each records what it was asked for and `GetUnportedScans()` returns the list, so a
missing scan is auditable rather than a mysteriously half-initialised manager.

The replacement idiom for `Find`+`FunctionFromNearCall` is `FindCallAfterConst`:
`Find("\x68\xde\x00\x00\x10\xe8", "xxxxxx", 5)` (push 0x100000de ; call) becomes
"find `i32.const 0x100000de`, take the following call's target".

## How hooking works

The Win32 backend calls MinHook (`MH_CreateHook` / `MH_EnableHook` / …). The wasm
backend writes 32-bit words into a table. Same six functions, same signatures, and
nothing above `GW::Hook` changes — a `Hook::CreateHook(&Fn, Detour, &Ret)` call
site compiles unmodified, because on wasm32 a C function pointer *is* an indirect
table index, and calling the trampoline compiles to a `call_indirect`.

The build pass (`wasmdetour.py --all`) replaced every function body with:

```
base = hook_base               ; 0 => all hooks off
slot = base[funcindex]         ; 0 => this one not hooked
slot ? call_indirect(slot)(args) : F_orig(args)
```

so arming a hook is a single store, effective on the next call — including direct
`call N` sites, which table redirection cannot reach. `pending`, a second array
parallel to the live table, provides MinHook's create-disabled-then-enable
semantics without allocating.

### Two tables

`gwca.wasm` is linked as its own main module and cannot import the game's table
(`--import-table` is rejected alongside Emscripten's `--export-table`), so it has
its own. A function pointer here indexes *that* table, and the two numberings are
unrelated. Both directions need translating, and both were observed failing
against a live client:

- **Detour out.** Dispatchers `call_indirect` against the game's table, so storing
  a gwca-local index calls whatever the game has at that slot. Five hooks armed as
  66..121 hit unrelated functions in the game's 35,266-entry table. Nothing fired.
- **Trampoline back.** `OrigSlot(f)` is a game slot, but GWCA called it through its
  own 221-entry table — `RuntimeError: table index is out of bounds`, after the
  detour body had already run.

Moving a funcref between tables is host-side work, so `PublishFuncref` /
`AdoptFuncref` are declared as explicit wasm imports: a loader that omits them
fails at instantiation naming the missing import, rather than leaving hooks that
arm but never fire.

### Folding scan results

The transform gives the module two ranges of the same functions: dispatchers at
`[func_base, slot_count)` — what the hook table is keyed on — and the original
bodies moved aside at `[slot_count, slot_count + n)`. A dispatcher body references
no strings, so every scan lands in the *upper* range and must be folded down by
`n` before it means anything to the hook table. Without that, `CreateHook` rejects
every hook as out of range (observed: `renderElapsed >= 0` → 24258, against a
hookable range of `[219, 17819)`).

## One heap, the game's

`gwca.wasm` links as a static main module, so it would otherwise bring its own
dlmalloc — and both allocators would manage the *same* linear memory from
different notions of where the heap starts. gwca's sbrk begins near its own
`__heap_base` (~0x16500) while the game's is past its data at ~0x28c85f, so the
two hand out overlapping blocks almost immediately. Observed as a trap inside the
game's own malloc on the first allocation after GWCA's static constructors ran:

```
RuntimeError: memory access out of bounds
  at wasm-function[17777]        <- the game's malloc
```

The constructors allocate through libc++ (`std::vector`, `std::function`,
`std::unordered_map`), so the damage is done before any GWCA code runs.

`-sMALLOC=none` keeps Emscripten's allocator out of the link and leaves these
symbols to us; each forwards to the game's exported allocator, supplied by the
loader as an import. Overriding `malloc`/`free` captures libc++ too, since
`operator new`/`delete` are defined in libc++abi in terms of malloc.

The corollary is that GWCA must not allocate before the loader has wired these
up — which is why nothing in `Alloc.cpp` runs during instantiation. `_initialize()`
runs the constructors, and the loader calls it explicitly.

## Calling conventions and the fastcall edx artifact

x86 has cdecl/stdcall/fastcall; wasm has exactly one convention and no way to
express these, so ~130 annotations across the tree become no-ops. The macros live
in `Source/Platform/Platform.h` — build-internal, *not* part of the release
bundle, which is why `stdafx.h` must not include it. `GWCA_WASM` itself comes from
the build as a compile definition so public headers can test it; undefined
evaluates to 0 in `#if`, the right answer for a consumer building on Windows.

The convention macros are pure erasure. **`GWCA_FASTCALL_EDX` is not**, and is the
one to be careful with. 11 of GWCA's 15 `__fastcall` typedefs declare a dummy
second parameter to account for x86 passing the first two integer arguments in
`ecx`/`edx`:

```cpp
typedef void(__fastcall* StartWhisper_pt)(GW::UI::Frame* ctx, uint32_t edx, wchar_t* name);
```

That `edx` is not an argument the function has — it exists so the compiler lays
the real arguments into the right registers. wasm has no registers and no such
convention, so the real function takes *two* parameters and the arity above is
wrong by one. Erasing `__fastcall` leaves the phantom behind, so wrap it instead:

```cpp
typedef void(GWCA_FASTCALL* StartWhisper_pt)(
    GW::UI::Frame* ctx, GWCA_FASTCALL_EDX(uint32_t) wchar_t* name);
```

On x86 that expands to `uint32_t edx,` unchanged; on wasm it expands to nothing.
The artifact leaks into **call sites** too — use `GWCA_FASTCALL_EDX_ARG` rather
than passing a literal dummy. Omitting it is a compile error on one target or the
other, never a silent wrong call, which is the reason for a macro rather than
`#if` blocks scattered through the managers. Getting it wrong does not corrupt
silently either: `call_indirect` type-checks, so a mismatch traps with
`RuntimeError: function signature mismatch` on the first call.

## Scan-site gating

Scans are gated with plain `#if GWCA_WASM`, **not** a macro taking both
expressions: `#if` discards the dead branch before compilation, so a wasm-only
scanner entry point needs no x86 declaration and vice versa. A macro would have to
parse both branches on both targets.

Most scans need no gate at all — string anchors port unchanged, and
`ToFunctionStart` is identity on wasm because `FindUseOfString` already returns
the enclosing function. Roughly 180 of ~280 call sites compile verbatim on both.
For a scan with no wasm resolution yet, gate it and leave the pointer null: the
existing `Verify()` / `GWCA_ASSERT` at the call site handles that, and
`Scanner::GetUnportedScans()` lists anything that fell through.
