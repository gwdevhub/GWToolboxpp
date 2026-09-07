// GW::Scanner wasm backend -- scans the module bytes; a result is a function index, not an address. See README.md.

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Debug.h>

#include <GWCA/Utilities/Hooker.h>   // ToFunctionStart hands its result to Hook::CallableFromScan

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "WasmScanner.h"

namespace {

    WasmScanner g_scanner;
    bool g_ready = false;

    // gwca table index -> function index, for scan results made callable. A
    // callable is a small integer with no relation to a function index, so
    // everything that still needs the index -- CheckArity, GlobalFromFunction,
    // CreateHook -- resolves through here first.
    std::unordered_map<uint32_t, uint32_t> g_callable_to_index;

    // The one place a scan result of either shape becomes a function index.
    uint32_t ResolveIndex(uintptr_t value)
    {
        const auto found = g_callable_to_index.find((uint32_t)value);
        if (found != g_callable_to_index.end())
            return found->second;
        return (uint32_t)g_scanner.FunctionAtCodeOffset(value);
    }

    // Scans with no wasm equivalent, recorded so a port can be audited.
    std::vector<std::string> g_unported;

    // Hex the pattern -- raw opcode bytes print as mojibake.
    std::string HexOf(const char* p)
    {
        static const char* kHex = "0123456789abcdef";
        std::string out;
        for (; p && *p; ++p) {
            unsigned char b = (unsigned char)*p;
            out += "\\x";
            out += kHex[b >> 4];
            out += kHex[b & 0xf];
        }
        return out;
    }

    void Unported(const char* what, const char* detail)
    {
        std::string s(what);
        if (detail && *detail) { s += ": "; s += HexOf(detail); }
        for (const std::string& e : g_unported)
            if (e == s) return;                  // once per distinct scan
        g_unported.push_back(s);
    }

    // GWCA writes Win32 assertion paths; wasm stores them relative with forward slashes.
    std::string NormalizeAssertionFile(const char* f)
    {
        return WasmScanner::NormalizePath(f);
    }

    // Wide strings are UTF-16LE here; host wchar_t is 4 bytes, so narrow by hand.
    std::string ToUtf16Bytes(const wchar_t* s)
    {
        std::string out;
        for (; s && *s; ++s) {
            uint16_t u = (uint16_t)*s;
            out.push_back((char)(u & 0xff));
            out.push_back((char)(u >> 8));
        }
        return out;
    }

}  // namespace

namespace GW {
    namespace Scanner {

        // No module to look up by name, so the caller supplies the bytes.
        bool Initialize(const void* wasm_bytes, size_t length)
        {
            if (!wasm_bytes || !length) return false;
            g_ready = WasmScanner::CreateFromBytes(
                (const uint8_t*)wasm_bytes, length, &g_scanner);
            return g_ready;
        }

        bool IsInitialized() { return g_ready; }

        const std::vector<std::string>& GetUnportedScans() { return g_unported; }

        // -- these port ---------------------------------------------------

        // These return the tagged CODE OFFSET of the reference, matching x86, where the
        // result is the operand address from the reloc table. Callers may add an offset
        // to it, scan a window from it, or hand it to ToFunctionStart -- all of which a
        // bare function index cannot support.
        uintptr_t FindUseOfString(const char* str, int offset,
                                  ScannerSection /*section*/)
        {
            if (!g_ready || !str) return 0;
            uintptr_t r = g_scanner.FindNthUseSiteOfString(str, 0);
            return r ? r + offset : 0;
        }

        uintptr_t FindNthUseOfString(const char* str, size_t nth, int offset,
                                     ScannerSection /*section*/)
        {
            if (!g_ready || !str) return 0;
            uintptr_t r = g_scanner.FindNthUseSiteOfString(str, nth);
            return r ? r + offset : 0;
        }

        uintptr_t FindUseOfString(const wchar_t* str, int offset,
                                  ScannerSection /*section*/)
        {
            if (!g_ready || !str) return 0;
            std::string bytes = ToUtf16Bytes(str);
            uintptr_t r = g_scanner.FindNthUseSiteOfBytes(bytes.data(),
                                                          bytes.size(), 0);
            return r ? r + offset : 0;
        }

        uintptr_t FindNthUseOfString(const wchar_t* str, size_t nth, int offset,
                                     ScannerSection /*section*/)
        {
            if (!g_ready || !str) return 0;
            std::string bytes = ToUtf16Bytes(str);
            uintptr_t r = g_scanner.FindNthUseSiteOfBytes(bytes.data(),
                                                          bytes.size(), nth);
            return r ? r + offset : 0;
        }

        uintptr_t FindAssertion(const char* assertion_file,
                                const char* assertion_msg,
                                uint32_t /*line_number*/, int offset)
        {
            if (!g_ready) return 0;
            // Match file AND message -- messages repeat ("!s_context" in 7 subsystems). line_number is not kept.
            // Keep the string addresses, not just the function tallies: the result has to be
            // the address the anchor is referenced at, as on x86, so callers can scan from it.
            std::vector<uint32_t> msg_addrs, file_addrs;
            // Tail, not Whole: the linker pools a short message into the end of a
            // longer literal and references the interior address, so "text" exists
            // only as the tail of "L'>' == *text". This mirrors x86, which matches
            // the message plus its NUL and uses the hit position.
            if (assertion_msg && *assertion_msg)
                msg_addrs = g_scanner.FindStringsPublic(assertion_msg, StringMatch::Tail);

            std::string file = NormalizeAssertionFile(assertion_file);
            if (!file.empty())
                file_addrs = g_scanner.FindStringsPublic(file, StringMatch::Anywhere);

            std::map<uint32_t, uint32_t> by_msg, by_file;
            for (uint32_t a : msg_addrs)
                for (const auto& kv : g_scanner.RefsTo(a)) by_msg[kv.first] += kv.second;
            for (uint32_t a : file_addrs)
                for (const auto& kv : g_scanner.RefsTo(a)) by_file[kv.first] += kv.second;

            if (by_msg.empty() && by_file.empty()) return 0;

            // Whichever anchor selected the function is the one whose site we report.
            uint32_t func;
            const std::vector<uint32_t>* anchor;
            if (by_file.empty()) {
                func = by_msg.begin()->first;
                anchor = &msg_addrs;
            } else if (by_msg.empty()) {
                func = by_file.begin()->first;
                anchor = &file_addrs;
            } else {
                auto it = by_msg.begin();
                for (; it != by_msg.end(); ++it)           // intersection
                    if (by_file.count(it->first)) break;
                if (it == by_msg.end()) return 0;          // no function has both
                func = it->first;
                anchor = &msg_addrs;
            }

            const uintptr_t site = g_scanner.FirstRefSiteInFunction(*anchor, func);
            return site ? site + offset : 0;
        }

        void NoteCallable(uintptr_t callable, uintptr_t func_index)
        {
            if (callable && func_index)
                g_callable_to_index[(uint32_t)callable] = (uint32_t)func_index;
        }

        // The function's start as a code offset -- what ToFunctionStart returned
        // before it began handing back something callable. For scanning on from
        // the result, never for calling it.
        uintptr_t FunctionStartAddress(uintptr_t address, uint32_t /*scan_range*/)
        {
            if (!g_ready) return address;
            return g_scanner.ToFunctionStart(address);
        }

        // Accepts either form -- a tagged code offset (what the string and pattern scans
        // return) or a bare function index -- and returns something CALLABLE.
        //
        // A code offset is not a table index, so calling one traps with "table
        // index is out of bounds"; that used to be every direct call to a scanned
        // function that nothing hooked. Anything that wants to scan on from the
        // start instead wants FunctionStartAddress.
        uintptr_t ToFunctionStart(uintptr_t address, uint32_t /*scan_range*/)
        {
            if (!g_ready) return address;
            const uintptr_t start = g_scanner.ToFunctionStart(address);
            // A 0 here is the usual root cause of a later, far-away CreateHook failure -- name it now.
            if (!start) {
                GWCA_WARN("[SCAN] ToFunctionStart(0x%x) -> 0 (input was %s, "
                          "resolved to index %u) -- nothing to hook or anchor on",
                          (unsigned)address,
                          (address & 0x80000000u) ? "a code offset" : "a function index",
                          (unsigned)g_scanner.FunctionAtCodeOffset(address));
                return 0;
            }
            // Adopt it into gwca's table so the result can simply be called. The
            // mapping back is recorded because CheckArity, GlobalFromFunction and
            // CreateHook all still need the function index.
            const uintptr_t callable = GW::Hook::CallableFromScan(start);
            if (!callable) {
                GWCA_WARN("[SCAN] ToFunctionStart(0x%x) -> 0x%x (func %u) but it "
                          "could not be made callable -- is GW::Hook::Initialize done?",
                          (unsigned)address, (unsigned)start,
                          (unsigned)g_scanner.FunctionAtCodeOffset(start));
                return 0;
            }
            GWCA_TRACE("[SCAN] ToFunctionStart(0x%x) -> callable %u (func %u, start 0x%x)",
                       (unsigned)address, (unsigned)callable,
                       (unsigned)g_scanner.FunctionAtCodeOffset(start), (unsigned)start);
            return callable;
        }

        // Resolve a code offset or bare index to a function index; 0 if it does not resolve.
        uintptr_t FunctionAtCodeOffset(uintptr_t address)
        {
            if (!g_ready) return address;
            return ResolveIndex(address);
        }

        bool IsValidPtr(uintptr_t address, ScannerSection section)
        {
            if (!g_ready) return false;
            if (section == Section_TEXT)
                return address >= g_scanner.FirstFunctionIndex() &&
                       address < g_scanner.FirstFunctionIndex() +
                                 g_scanner.FunctionCount();
            // RDATA and DATA are one read-write region in wasm.
            return address >= g_scanner.DataLow() &&
                   address < g_scanner.DataHigh();
        }

        void GetSectionAddressRange(ScannerSection section, uintptr_t* start,
                                    uintptr_t* end)
        {
            uintptr_t lo = 0, hi = 0;
            if (g_ready) {
                if (section == Section_TEXT) {
                    // Function INDICES, not linear addresses -- not comparable with DATA values.
                    lo = g_scanner.FirstFunctionIndex();
                    hi = lo + g_scanner.FunctionCount();
                } else {
                    lo = g_scanner.DataLow();
                    hi = g_scanner.DataHigh();
                }
            }
            if (start) *start = lo;
            if (end) *end = hi;
        }

        // -- replacement for the Find + FunctionFromNearCall idiom ---------

        // Both return the callee's start address, like FunctionFromNearCall and like x86.
        uintptr_t FindCallAfterConst(int32_t value, int window)
        {
            if (!g_ready) return 0;
            return g_scanner.ToFunctionStart(
                g_scanner.FindCallAfterConst(value, window));
        }

        // Signature picks the overload an ordinal cannot: a getter is (i32)->(i32) and its
        // setter (i32,i32)->(), so the pair is separable even sharing one assert string.
        uintptr_t FindUseOfStringWithSignature(const char* str, uint32_t params,
                                               uint32_t results)
        {
            if (!g_ready || !str) return 0;
            bool have = false;
            uint32_t found = 0;
            for (const auto& kv : g_scanner.FindUsesOfString(str)) {
                uint32_t np = 0, nr = 0;
                if (!g_scanner.FunctionArity(kv.first, &np, &nr)) continue;
                if (np != params || nr != results) continue;
                if (have) {
                    GWCA_INFO("[SCAN] '%s' (%u,%u) matches more than one function", str,
                              params, results);
                    return 0;
                }
                found = kv.first;
                have = true;
            }
            if (!have) return 0;
            // Report where the string is used, not which function uses it -- same contract
            // as FindUseOfString, so the two compose with ToFunctionStart identically.
            return g_scanner.FirstRefSiteInFunction(
                g_scanner.FindStringsPublic(str, StringMatch::Whole), found);
        }

        // Arity-disambiguated: the SendUIMessage anchor matches two callees.
        uintptr_t FindCallAfterConstWithArity(int32_t value, uint32_t nparams,
                                              int window)
        {
            if (!g_ready) return 0;
            return g_scanner.ToFunctionStart(
                g_scanner.FindCallAfterConstWithArity(value, nparams, window));
        }

        // Belt-and-braces arity guard -- see Scanner.h and GWCA_SCAN_CHECK_ARITY.
        bool CheckArity(uintptr_t func_index, uint32_t expected, const char* what)
        {
            if (!g_ready || !func_index) return true;   // nothing resolved to check
            // May be a ToFunctionStart address -- resolve to the index the type section is keyed on.
            uint32_t idx = (uint32_t)ResolveIndex(func_index);
            uint32_t np = 0;
            if (!g_scanner.FunctionArity(idx, &np, nullptr))
                return true;                            // imported / typeless: can't judge
            if (np == expected) return true;
            // Log and discard; the module's own _DEBUG assert block is the single place that bombs out.
            GWCA_ERR("[SCAN] %s: resolved function takes %u wasm params, expected %u -- discarding\n",
                     what ? what : "(scan)", np, expected);
            return false;
        }

        // Arity plus results -- two functions can reference identical strings and differ only in return type.
        bool CheckSignature(uintptr_t func_index, uint32_t expected_params,
                            uint32_t expected_results, const char* what)
        {
            if (!g_ready || !func_index) return true;
            uint32_t idx = (uint32_t)ResolveIndex(func_index);
            uint32_t np = 0, nr = 0;
            if (!g_scanner.FunctionArity(idx, &np, &nr))
                return true;                       // imported / typeless
            if (np == expected_params && nr == expected_results) return true;
            GWCA_ERR("[SCAN] %s: resolved function is (%u params, %u results), "
                     "expected (%u, %u) -- discarding\n",
                     what ? what : "(scan)", np, nr,
                     expected_params, expected_results);
            return false;
        }

        // Replaces *(uintptr_t*)address: the global lives in the load's memarg offset. Takes either scan form.
        uintptr_t GlobalFromFunction(uintptr_t func_index, size_t nth)
        {
            if (!g_ready || !func_index) return 0;
            return g_scanner.GlobalFromFunction(
                ResolveIndex(func_index), nth);
        }

        // Per-instruction form: reads the global straight out of a load/store you already located.
        uintptr_t GlobalAtCodeOffset(uintptr_t code_offset)
        {
            if (!g_ready || !code_offset) return 0;
            return g_scanner.GlobalAtCodeOffset(code_offset);
        }

        uintptr_t ConstAtCodeOffset(uintptr_t code_offset)
        {
            if (!g_ready || !code_offset) return 0;
            return g_scanner.ConstAtCodeOffset(code_offset);
        }

        std::vector<uint32_t> GlobalsInFunction(uintptr_t func_index)
        {
            if (!g_ready) return {};
            return g_scanner.GlobalsInFunction(
                (uint32_t)ResolveIndex(func_index));
        }

        // A static handed to a call as an i32.const -- GlobalFromFunction only sees load/store memargs.
        uintptr_t BssConstFromFunction(uintptr_t func_index, size_t nth)
        {
            if (!g_ready || !func_index) return 0;
            return g_scanner.BssConstFromFunction(
                ResolveIndex(func_index), nth);
        }

        // -- these have no wasm equivalent ---------------------------------

        // Byte scanning is exact here: the pattern is matched against the module bytes as given.
        // A pattern written for x86 will simply not match, or match something meaningless -- there
        // is no way to tell the two apart from the bytes, so nothing is rejected up front.
        uintptr_t Find(const char* pattern, const char* mask, int offset,
                       ScannerSection /*section*/)
        {
            if (!g_ready) return 0;
            return g_scanner.FindPattern(pattern, mask, offset);
        }

        uintptr_t FindNth(const char* pattern, const char* mask, size_t nth,
                          int offset, ScannerSection /*section*/)
        {
            if (!g_ready) return 0;
            return g_scanner.FindPatternNth(pattern, mask, nth, offset);
        }

        uintptr_t FindInRange(const char* pattern, const char* mask, int offset,
                              uint32_t start, uint32_t end)
        {
            if (!g_ready) return 0;
            return g_scanner.FindPatternInRange(pattern, mask, offset, start, end);
        }

        // Reads the funcidx immediate of the call at a Find/FindInRange offset -- no rel32 to decode.
        uintptr_t FunctionFromNearCall(uintptr_t call_site, bool check_valid_ptr)
        {
            if (!g_ready) return 0;
            const uintptr_t f = g_scanner.CallTargetAt(call_site);
            if (!f) return 0;
            // Apply to the raw funcidx: Section_TEXT ranges over indices, and an import has no body.
            if (check_valid_ptr && !IsValidPtr(f, Section_TEXT)) return 0;
            // Return the callee's start address (tagged offset) like x86, so scan results compose.
            return g_scanner.ToFunctionStart(f);
        }

        // No index to return: single-threaded, no TLS. s_propContext is resolved in GWCA.cpp instead.
        uint32_t GetGameTlsIndex()
        {
            Unported("Scanner::GetGameTlsIndex (no wasm equivalent)", nullptr);
            return 0;
        }


    }  // namespace Scanner
}  // namespace GW
