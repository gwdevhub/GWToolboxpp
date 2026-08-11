#pragma once

#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// WasmScanner -- the wasm FileScanner. Returns a function INDEX, not a callable address; see ToCallable().

// How a needle has to sit inside a stored string. Suffix is what x86's
// FindUseOfString does -- it matches the needle plus its NUL, then walks back to
// the string start -- which is why a bare "InvBag.cpp" resolves there against a
// stored "../../../../Gw/Ui/Game/Inventory/InvBag.cpp".
//
// Tail is the same match but reports the NEEDLE's address rather than the
// string's. The linker pools a short literal into the end of a longer one and
// points the code at the interior address, so the assertion message "text" is
// stored only as the tail of "L'>' == *text" and is referenced at that offset.
// Whole rejects it (the preceding byte is not a NUL) and Suffix reports the
// wrong address (the enclosing string's start), so neither finds such a message.
// This is what FileScanner::FindAssertion does on x86: it matches the message
// plus its NUL and uses the hit position, walking back only for the file path.
enum class StringMatch { Whole, Suffix, Anywhere, Tail };

class WasmScanner {
public:
    static bool CreateFromPath(const char* path, WasmScanner* result);

    // Build from bytes already in memory: a running module cannot read its own code section.
    static bool CreateFromBytes(const uint8_t* data, size_t length,
                                WasmScanner* result);

    // GW::Scanner::FindUseOfString analogue -- 0 if unresolved or ambiguous beyond nth.
    uintptr_t FindUseOfString(const char* str) const;
    uintptr_t FindNthUseOfString(const char* str, size_t nth) const;

    // GW::Scanner::FindAssertion analogue; assertion_file is accepted in GWCA's Win32 form and normalised.
    uintptr_t FindAssertion(const char* assertion_file,
                            const char* assertion_msg) const;

    // Every function referencing the string, with reference counts.
    std::map<uint32_t, uint32_t> FindUsesOfString(const char* str) const;

    // The Find+FunctionFromNearCall idiom: find where a constant is passed, take the callee. window bounds the gap.
    std::map<uint32_t, uint32_t> FindCallsAfterConst(int32_t value,
                                                     int window = 6) const;

    // Single best callee for the above, or 0 if none/ambiguous.
    uintptr_t FindCallAfterConst(int32_t value, int window = 6) const;

    // Same, disambiguated by parameter count -- SendUIMessage's anchor matches two callees. 0 if still ambiguous.
    uintptr_t FindCallAfterConstWithArity(int32_t value, uint32_t nparams,
                                          int window = 6) const;

    // Parameter / result counts for a function index, via the type section.
    bool FunctionArity(uint32_t func_index, uint32_t* nparams,
                       uint32_t* nresults) const;

    // Byte scanning works, but patterns must be derived against wasm bytecode; a match returns a tagged CODE OFFSET.
    static const uintptr_t kCodeOffset = 0x80000000u;

    static bool IsCodeOffset(uintptr_t v) { return (v & kCodeOffset) != 0; }

    // mask uses 'x' for must-match and anything else for a wildcard, exactly as GW::Scanner::Find does.
    uintptr_t FindPattern(const char* pattern, const char* mask,
                          int offset = 0) const;
    // nth match over the whole code section, 0-based.
    uintptr_t FindPatternNth(const char* pattern, const char* mask, size_t nth,
                             int offset = 0) const;
    // end < start scans backward from start, as GW::Scanner::FindInRange documents on x86.
    uintptr_t FindPatternInRange(const char* pattern, const char* mask,
                                 int offset, uintptr_t start,
                                 uintptr_t end) const;

    // Resolve a scan result to a function index -- the inverse of ToFunctionStart, called at the GW::Hook boundary.
    uintptr_t FunctionAtCodeOffset(uintptr_t tagged) const;

    // The wasm ToFunctionStart: a tagged code offset you can anchor further scans on, like x86's function start.
    uintptr_t ToFunctionStart(uintptr_t x) const;

    // funcidx immediate of the call at a tagged code offset -- the wasm FunctionFromNearCall. 0 if not a call.
    uintptr_t CallTargetAt(uintptr_t tagged) const;

    // memarg offset of the load/store at a tagged code offset -- the per-instruction GlobalFromFunction.
    uintptr_t GlobalAtCodeOffset(uintptr_t tagged) const;

    // Decoded immediate of the i32.const at a tagged code offset. 0 if that offset is not an i32.const.
    uintptr_t ConstAtCodeOffset(uintptr_t tagged) const;

    uintptr_t CodeLow() const { return code_lo_ | kCodeOffset; }
    uintptr_t CodeHigh() const { return code_hi_ | kCodeOffset; }

    // LLVM puts an absolutely addressed global in the load's memarg offset, so it is read out of the instruction, not dereferenced.

    // Distinct static addresses touched by a function, in first-use order.
    std::vector<uint32_t> GlobalsInFunction(uint32_t func_index) const;

    // The nth of those, or 0. Mirrors the x86 deref at a scan result.
    uintptr_t GlobalFromFunction(uintptr_t func_index, size_t nth = 0) const;

    // Statics passed as an i32.const rather than loaded; restricted to .bss, which separates them from .data string constants.
    std::vector<uint32_t> BssConstsInFunction(uint32_t func_index) const;
    uintptr_t BssConstFromFunction(uintptr_t func_index, size_t nth = 0) const;

    // Reverse lookup: which functions touch this global.
    std::vector<uint32_t> FunctionsUsingGlobal(uint32_t addr) const;

    // Indirect-table slot for a function index, or -1 -- the gate between resolved and hookable.
    int TableSlot(uint32_t func_index) const;

    // Convert a scan result into something callable, or 0 if it cannot be.
    uintptr_t ToCallable(uintptr_t func_index) const;

    // Binary-safe variants, for UTF-16 string anchors.
    uintptr_t FindUseOfBytes(const void* bytes, size_t len) const;
    uintptr_t FindNthUseOfBytes(const void* bytes, size_t len, size_t nth) const;

    size_t FunctionCount() const { return funcs_.size(); }
    uint32_t FirstFunctionIndex() const {
        return funcs_.empty() ? 0 : funcs_.front().index;
    }
    size_t TableCount() const { return table_.size(); }
    size_t DecodeFailureCount() const { return decode_failures_; }
    uint32_t DataLow() const { return data_lo_; }
    uint32_t DataHigh() const { return data_hi_; }

    static std::string NormalizePath(const char* p);

    // Exposed so the GW::Scanner layer can intersect message and file matches.
    std::vector<uint32_t> FindStringsPublic(const std::string& s, StringMatch mode) const
    { return FindStrings(s, mode); }
    std::map<uint32_t, uint32_t> RefsTo(uint32_t addr) const
    { auto it = refs_.find(addr);
      return it == refs_.end() ? std::map<uint32_t, uint32_t>() : it->second; }

    // Code offsets at which `addr` appears as an i32.const immediate, ascending.
    // The wasm analogue of x86's reloc use-site list, so a use SITE -- not the
    // function containing it -- is what a string scan can hand back.
    std::vector<uint32_t> RefSitesTo(uint32_t addr) const
    { auto it = ref_sites_.find(addr);
      return it == ref_sites_.end() ? std::vector<uint32_t>() : it->second; }

    // nth use site of a string, as a tagged code offset. `nth` counts sites, as
    // on x86: a function referencing the string twice contributes two.
    uintptr_t FindNthUseSiteOfString(const char* str, size_t nth) const;
    uintptr_t FindNthUseSiteOfBytes(const void* bytes, size_t len, size_t nth) const;

    // Lowest tagged code offset inside `func_index` referencing any of `addrs`.
    // Lets an anchor picked at function granularity still report an address.
    uintptr_t FirstRefSiteInFunction(const std::vector<uint32_t>& addrs,
                                     uint32_t func_index) const;

private:
    struct Segment { uint32_t base; size_t off; size_t size; };
    struct Func { size_t start; size_t end; uint32_t index; };

    bool Parse();
    bool BuildRefIndex();
    bool DecodeBody(const Func& f);
    uint32_t FuncAt(size_t code_off) const;
    std::vector<uint32_t> FindStrings(const std::string& s, StringMatch mode) const;

    std::vector<uint8_t> data_;
    std::vector<Segment> segs_;
    std::vector<Func> funcs_;
    std::unordered_map<uint32_t, int> table_;          // funcidx -> slot
    std::map<uint32_t, std::map<uint32_t, uint32_t>> refs_;  // addr -> func -> n
    std::map<uint32_t, std::vector<uint32_t>> ref_sites_;    // addr -> code offsets
    std::map<uint32_t, std::vector<uint32_t>> globals_;       // func -> addrs
    std::vector<std::pair<uint32_t, uint32_t>> types_;        // (nparams, nresults)
    std::unordered_map<uint32_t, uint32_t> func_type_;        // funcidx -> typeidx

    uint32_t num_func_imports_ = 0;
    uint32_t data_lo_ = 0, data_hi_ = 0;
    size_t code_lo_ = 0, code_hi_ = 0;
    size_t decode_failures_ = 0;
};
