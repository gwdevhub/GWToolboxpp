// mod_init -- the loader's uniform entry point; the arguments are the transformed game module's bytes. See README.md.

#include <GWCA/stdafx.h>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <cstdlib>

// Supplied by the loader; suspends until every module loaded after this one has
// returned from its own mod_init. An explicit import so a loader without it fails
// at instantiation naming the symbol, rather than detaching gwca instantly.
extern "C" {
    __attribute__((import_module("env"), import_name("mod_wait_until_unused")))
    void mod_wait_until_unused(void);
}

extern "C" void mod_init(const void* game_bytes, uint32_t len)
{
    if (!game_bytes || !len) {
        GWCA_ERR("[INIT] mod_init: no game bytes (%p, %u) -- gwca cannot scan",
                 game_bytes, len);
        return;
    }
    if (!GW::Scanner::Initialize(game_bytes, len)) {
        GWCA_ERR("[INIT] Scanner::Initialize(%u bytes) failed", len);
        return;
    }

    // The hook table is indexed by function index, so it spans the whole defined range, which Section_TEXT reports as indices.
    uintptr_t first = 0, end = 0;
    GW::Scanner::GetSectionAddressRange(GW::Section_TEXT, &first, &end);
    const uint32_t func_base = (uint32_t)first;

    // The transformed bytes define 2n functions where the game had n; Hook wants n, or a parked index never folds to its dispatcher.
    const uint32_t defined = (uint32_t)(end - first);
    if (defined & 1u) {
        GWCA_ERR("[INIT] transformed module defines an odd number of functions "
                 "(%u) -- not the dispatcher/original pairing gwca expects", defined);
    }
    const uint32_t func_count = defined / 2u;

    // live + pending, both zeroed: a non-zero entry reads as "hooked".
    const size_t bytes = GW::Hook::TableBytes(func_base, func_count) * 2;
    void* table = calloc(1, bytes);
    if (!table) {
        GWCA_ERR("[INIT] hook table alloc of %u bytes failed", (unsigned)bytes);
        return;
    }
    if (!GW::Hook::Initialize(table, func_base, func_count)) {
        GWCA_ERR("[INIT] Hook::Initialize failed");
        free(table);
        return;
    }
    GWCA_INFO("[INIT] provisioned: scanner over %u bytes, hook table %u bytes "
              "for funcs [%u, %u)", len, (unsigned)bytes, func_base, func_base + func_count);

    // A module lives for exactly as long as its mod_init runs, and provisioning is
    // instant -- so park, or gwca is retired before the mods it exists to serve are
    // even instantiated. Suspending here is what hands control back to the loader,
    // which resumes us once nothing loaded after us is still running.
    mod_wait_until_unused();

    // Never just return: the game's dispatchers hold funcrefs into this module's
    // table, so they have to be disarmed before it is dropped. Terminate is a no-op
    // if no mod ever called GW::Initialize.
    GW::Terminate();
}
