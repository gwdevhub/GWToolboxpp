#include "WasmScanner.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

// Immediate shapes; the module's feature set (no SIMD, atomics, EH or GC) makes this table complete.
enum Imm {
    IMM_NONE, IMM_BLOCKTYPE, IMM_U32, IMM_MEMARG, IMM_I32, IMM_I64,
    IMM_F32, IMM_F64, IMM_BRTABLE, IMM_VALTYPE, IMM_SELECTVEC,
    IMM_CALL_INDIRECT, IMM_BAD
};

Imm ImmFor(uint8_t op)
{
    switch (op) {
    case 0x00: case 0x01: case 0x05: case 0x0B: case 0x0F:
    case 0x1A: case 0x1B: case 0xD1:
        return IMM_NONE;
    case 0x02: case 0x03: case 0x04:
        return IMM_BLOCKTYPE;
    case 0x0C: case 0x0D: case 0x10: case 0x20: case 0x21: case 0x22:
    case 0x23: case 0x24: case 0x25: case 0x26: case 0x3F: case 0x40:
    case 0xD2:
        return IMM_U32;
    case 0x0E: return IMM_BRTABLE;
    case 0x11: return IMM_CALL_INDIRECT;
    case 0x1C: return IMM_SELECTVEC;
    case 0x41: return IMM_I32;
    case 0x42: return IMM_I64;
    case 0x43: return IMM_F32;
    case 0x44: return IMM_F64;
    case 0xD0: return IMM_VALTYPE;
    default:
        if (op >= 0x28 && op <= 0x3E) return IMM_MEMARG;   // loads + stores
        if (op >= 0x45 && op <= 0xC4) return IMM_NONE;     // numeric, sign-ext
        return IMM_BAD;
    }
}

// 0xFC sub-opcode -> count of u32 immediates.
int FcImmCount(uint32_t sub)
{
    if (sub <= 7) return 0;                       // nontrapping fp->int
    switch (sub) {
    case 8: case 10: case 12: case 14: return 2;  // memory.init/copy, table.init/copy
    case 9: case 11: case 13: case 15:
    case 16: case 17: return 1;
    default: return -1;
    }
}

bool ULeb(const std::vector<uint8_t>& d, size_t& i, uint64_t& out)
{
    uint64_t r = 0; unsigned s = 0;
    while (true) {
        if (i >= d.size() || s > 63) return false;
        uint8_t b = d[i++];
        r |= (uint64_t)(b & 0x7F) << s; s += 7;
        if (!(b & 0x80)) { out = r; return true; }
    }
}

bool SLeb(const std::vector<uint8_t>& d, size_t& i, int64_t& out)
{
    uint64_t r = 0; unsigned s = 0; uint8_t b = 0;
    while (true) {
        if (i >= d.size() || s > 63) return false;
        b = d[i++];
        r |= (uint64_t)(b & 0x7F) << s; s += 7;
        if (!(b & 0x80)) break;
    }
    if (s < 64 && (b & 0x40)) r |= ~0ULL << s;
    out = (int64_t)r;
    return true;
}

bool IsValType(uint8_t b)
{
    return b == 0x7F || b == 0x7E || b == 0x7D || b == 0x7C ||
           b == 0x70 || b == 0x6F;
}

}  // namespace

// ---------------------------------------------------------------- loading

bool WasmScanner::CreateFromPath(const char* path, WasmScanner* result)
{
    if (!path || !result) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return false; }
    result->data_.resize((size_t)n);
    size_t got = fread(result->data_.data(), 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) return false;
    return result->Parse() && result->BuildRefIndex();
}

bool WasmScanner::CreateFromBytes(const uint8_t* data, size_t length,
                                  WasmScanner* result)
{
    if (!data || !length || !result) return false;
    result->data_.assign(data, data + length);
    return result->Parse() && result->BuildRefIndex();
}

bool WasmScanner::Parse()
{
    const std::vector<uint8_t>& d = data_;
    if (d.size() < 8 || memcmp(d.data(), "\0asm", 4) != 0) return false;

    std::vector<std::pair<size_t, size_t>> sec[13];   // id -> (offset, len)
    size_t p = 8;
    while (p < d.size()) {
        uint8_t id = d[p++];
        uint64_t len;
        if (!ULeb(d, p, len)) return false;
        if (id < 13) sec[id].push_back({p, (size_t)len});
        p += (size_t)len;
    }

    // imports: count function imports so defined-func indices are right
    if (!sec[2].empty()) {
        size_t q = sec[2][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t l;
            if (!ULeb(d, q, l)) return false;
            q += (size_t)l;                                     // module
            if (!ULeb(d, q, l)) return false;
            q += (size_t)l;                                     // name
            uint8_t kind = d[q++];
            uint64_t tmp;
            if (kind == 0) { num_func_imports_++; ULeb(d, q, tmp); }
            else if (kind == 1) {
                q++; uint8_t fl = d[q++]; ULeb(d, q, tmp);
                if (fl) ULeb(d, q, tmp);
            } else if (kind == 2) {
                uint8_t fl = d[q++]; ULeb(d, q, tmp);
                if (fl) ULeb(d, q, tmp);
            } else if (kind == 3) { q += 2; }
        }
    }

    // data segments
    if (!sec[11].empty()) {
        size_t q = sec[11][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t flags, base, size;
            if (!ULeb(d, q, flags) || flags != 0) return false;
            q++;                                    // i32.const
            if (!ULeb(d, q, base)) return false;
            q++;                                    // end
            if (!ULeb(d, q, size)) return false;
            segs_.push_back({(uint32_t)base, q, (size_t)size});
            q += (size_t)size;
        }
    }
    if (segs_.empty()) return false;
    data_lo_ = segs_.front().base;
    data_hi_ = segs_.back().base + (uint32_t)segs_.back().size;

    // types, and which type each function has -- needed for arity checks
    if (!sec[1].empty()) {
        size_t q = sec[1][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        for (uint64_t i = 0; i < n; i++) {
            if (d[q++] != 0x60) return false;
            uint64_t np, nr;
            if (!ULeb(d, q, np)) return false;
            q += (size_t)np;
            if (!ULeb(d, q, nr)) return false;
            q += (size_t)nr;
            types_.push_back({(uint32_t)np, (uint32_t)nr});
        }
    }
    if (!sec[3].empty()) {
        size_t q = sec[3][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t t;
            if (!ULeb(d, q, t)) return false;
            func_type_[num_func_imports_ + (uint32_t)i] = (uint32_t)t;
        }
    }

    // code
    if (sec[10].empty()) return false;
    {
        size_t q = sec[10][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        funcs_.reserve((size_t)n);
        for (uint64_t i = 0; i < n; i++) {
            uint64_t size;
            if (!ULeb(d, q, size)) return false;
            funcs_.push_back({q, q + (size_t)size,
                              num_func_imports_ + (uint32_t)i});
            q += (size_t)size;
        }
    }
    code_lo_ = funcs_.front().start;
    code_hi_ = funcs_.back().end;

    // elem -> indirect table membership
    if (!sec[9].empty()) {
        size_t q = sec[9][0].first;
        uint64_t n;
        if (!ULeb(d, q, n)) return false;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t flags, base, cnt;
            if (!ULeb(d, q, flags)) return false;
            if (flags != 0) continue;
            q++;
            if (!ULeb(d, q, base)) return false;
            q++;
            if (!ULeb(d, q, cnt)) return false;
            for (uint64_t k = 0; k < cnt; k++) {
                uint64_t v;
                if (!ULeb(d, q, v)) return false;
                table_[(uint32_t)v] = (int)(base + k);
            }
        }
    }
    return true;
}

// --------------------------------------------------------------- decoding

bool WasmScanner::DecodeBody(const Func& f)
{
    const std::vector<uint8_t>& d = data_;
    size_t q = f.start;
    uint64_t nlocals;
    if (!ULeb(d, q, nlocals)) return false;
    for (uint64_t i = 0; i < nlocals; i++) {
        uint64_t c;
        if (!ULeb(d, q, c)) return false;
        q++;                                        // valtype
    }

    while (q < f.end) {
        uint8_t op = d[q++];

        if (op == 0xFC) {
            uint64_t sub;
            if (!ULeb(d, q, sub)) return false;
            int k = FcImmCount((uint32_t)sub);
            if (k < 0) return false;
            for (int i = 0; i < k; i++) {
                uint64_t t;
                if (!ULeb(d, q, t)) return false;
            }
            continue;
        }

        uint64_t t; int64_t s;
        switch (ImmFor(op)) {
        case IMM_NONE: break;
        case IMM_BLOCKTYPE:
            if (d[q] == 0x40 || IsValType(d[q])) q++;
            else if (!SLeb(d, q, s)) return false;
            break;
        case IMM_U32:
            if (!ULeb(d, q, t)) return false;
            break;
        case IMM_MEMARG: {
            uint64_t align, moff;
            if (!ULeb(d, q, align)) return false;
            if (!ULeb(d, q, moff)) return false;
            if (align & 0x40 && !ULeb(d, q, t)) return false;
            // An offset inside or just past the data image is an absolutely addressed global, not a struct field offset.
            if (moff >= data_lo_) {
                std::vector<uint32_t>& g = globals_[f.index];
                uint32_t a = (uint32_t)moff;
                if (std::find(g.begin(), g.end(), a) == g.end())
                    g.push_back(a);
            }
            break;
        }
        case IMM_I32: {
            // Keep where the immediate sits, not just which function held it: x86's
            // FindUseOfString returns the operand address, and callers scan from it.
            const size_t imm = q;
            if (!SLeb(d, q, s)) return false;
            if (s >= (int64_t)data_lo_ && s < (int64_t)data_hi_) {
                refs_[(uint32_t)s][f.index]++;
                ref_sites_[(uint32_t)s].push_back((uint32_t)imm);
            }
            break;
        }
        case IMM_I64:
            if (!SLeb(d, q, s)) return false;
            break;
        case IMM_F32: q += 4; break;
        case IMM_F64: q += 8; break;
        case IMM_BRTABLE: {
            uint64_t cnt;
            if (!ULeb(d, q, cnt)) return false;
            for (uint64_t i = 0; i <= cnt; i++)
                if (!ULeb(d, q, t)) return false;
            break;
        }
        case IMM_VALTYPE: q += 1; break;
        case IMM_SELECTVEC: {
            uint64_t cnt;
            if (!ULeb(d, q, cnt)) return false;
            q += (size_t)cnt;
            break;
        }
        case IMM_CALL_INDIRECT:
            if (!ULeb(d, q, t)) return false;        // typeidx
            if (!ULeb(d, q, t)) return false;        // tableidx
            break;
        case IMM_BAD:
        default:
            return false;
        }
        if (q > f.end) return false;
    }
    return q == f.end;
}

bool WasmScanner::BuildRefIndex()
{
    for (const Func& f : funcs_)
        if (!DecodeBody(f)) decode_failures_++;
    return true;
}

// ---------------------------------------------------------------- queries

std::vector<uint32_t> WasmScanner::FindStrings(const std::string& s,
                                               StringMatch mode) const
{
    std::vector<uint32_t> out;
    for (const Segment& sg : segs_) {
        const char* blob = (const char*)data_.data() + sg.off;
        size_t pos = 0;
        while (pos + s.size() <= sg.size) {
            const void* hit = memmem(blob + pos, sg.size - pos,
                                     s.data(), s.size());
            if (!hit) break;
            size_t i = (const char*)hit - blob;
            // Whole and Tail anchor on the needle itself; Suffix and Anywhere
            // report the string it lands in, which is the address the code
            // references for a normally-stored literal.
            size_t start = i;
            if (mode == StringMatch::Suffix || mode == StringMatch::Anywhere)
                while (start > 0 && blob[start - 1] != '\0') start--;
            // Tail accepts an interior hit: a pooled literal has a real
            // character before it, and that address is what the code holds.
            bool ok = (mode == StringMatch::Tail
                       || start == 0 || blob[start - 1] == '\0');
            // Everything but Anywhere requires the needle to reach the end of
            // the string; a segment ending without a NUL counts as terminated,
            // as the whole-string form always did.
            if (ok && mode != StringMatch::Anywhere) {
                const size_t end = i + s.size();
                ok = (end == sg.size || blob[end] == '\0');
            }
            if (ok) out.push_back(sg.base + (uint32_t)start);
            pos = i + 1;
        }
    }
    return out;
}

// Bodies are decoded in index order and each body front to back, so ref_sites_ is
// already ascending; collecting across pooled duplicates of one literal is not.
uintptr_t WasmScanner::FindNthUseSiteOfString(const char* str, size_t nth) const
{
    if (!str) return 0;
    std::vector<uint32_t> sites;
    for (uint32_t addr : FindStrings(str, StringMatch::Suffix))
        for (uint32_t off : RefSitesTo(addr)) sites.push_back(off);
    std::sort(sites.begin(), sites.end());
    return nth < sites.size() ? ((uintptr_t)sites[nth] | kCodeOffset) : 0;
}

uintptr_t WasmScanner::FindNthUseSiteOfBytes(const void* bytes, size_t len,
                                             size_t nth) const
{
    if (!bytes || !len) return 0;
    std::string needle((const char*)bytes, len);
    std::vector<uint32_t> sites;
    for (uint32_t addr : FindStrings(needle, StringMatch::Anywhere))
        for (uint32_t off : RefSitesTo(addr)) sites.push_back(off);
    std::sort(sites.begin(), sites.end());
    return nth < sites.size() ? ((uintptr_t)sites[nth] | kCodeOffset) : 0;
}

uintptr_t WasmScanner::FirstRefSiteInFunction(const std::vector<uint32_t>& addrs,
                                              uint32_t func_index) const
{
    uintptr_t best = 0;
    for (uint32_t addr : addrs) {
        for (uint32_t off : RefSitesTo(addr)) {
            if (FuncAt(off) != func_index) continue;
            if (!best || off < (best & ~kCodeOffset)) best = off | kCodeOffset;
        }
    }
    return best;
}

uintptr_t WasmScanner::FindUseOfBytes(const void* bytes, size_t len) const
{
    return FindNthUseOfBytes(bytes, len, 0);
}

uintptr_t WasmScanner::FindNthUseOfBytes(const void* bytes, size_t len,
                                         size_t nth) const
{
    if (!bytes || !len) return 0;
    std::string needle((const char*)bytes, len);
    std::map<uint32_t, uint32_t> total;
    for (uint32_t addr : FindStrings(needle, StringMatch::Anywhere)) {
        auto it = refs_.find(addr);
        if (it == refs_.end()) continue;
        for (const auto& kv : it->second) total[kv.first] += kv.second;
    }
    if (nth >= total.size()) return 0;
    auto it = total.begin();
    std::advance(it, nth);
    return (uintptr_t)it->first;
}

std::map<uint32_t, uint32_t> WasmScanner::FindUsesOfString(const char* str) const
{
    std::map<uint32_t, uint32_t> total;
    if (!str) return total;
    for (uint32_t addr : FindStrings(str, StringMatch::Whole)) {
        auto it = refs_.find(addr);
        if (it == refs_.end()) continue;
        for (const auto& kv : it->second) total[kv.first] += kv.second;
    }
    return total;
}

uintptr_t WasmScanner::FindUseOfString(const char* str) const
{
    return FindNthUseOfString(str, 0);
}

uintptr_t WasmScanner::FindNthUseOfString(const char* str, size_t nth) const
{
    std::map<uint32_t, uint32_t> hits = FindUsesOfString(str);
    if (nth >= hits.size()) return 0;
    auto it = hits.begin();
    std::advance(it, nth);
    return (uintptr_t)it->first;
}

std::string WasmScanner::NormalizePath(const char* p)
{
    std::string s(p ? p : "");
    for (char& c : s) if (c == '\\') c = '/';
    size_t i = 0;
    while (i < s.size() && s[i] == '/') i++;
    s = s.substr(i);
    if (s.rfind("Code/", 0) == 0) s = s.substr(5);
    return s;
}

uintptr_t WasmScanner::FindAssertion(const char* assertion_file,
                                     const char* assertion_msg) const
{
    // Prefer the message: it is far more selective than the file.
    if (assertion_msg && *assertion_msg) {
        uintptr_t r = FindUseOfString(assertion_msg);
        if (r) return r;
    }
    if (!assertion_file || !*assertion_file) return 0;
    // Paths are stored as ../../../../Gw/Ui/UiRoot.cpp -- match the tail and walk back to the string start.
    std::string norm = NormalizePath(assertion_file);
    std::map<uint32_t, uint32_t> total;
    for (uint32_t addr : FindStrings(norm, StringMatch::Anywhere)) {
        auto it = refs_.find(addr);
        if (it == refs_.end()) continue;
        for (const auto& kv : it->second) total[kv.first] += kv.second;
    }
    return total.empty() ? 0 : (uintptr_t)total.begin()->first;
}

std::map<uint32_t, uint32_t> WasmScanner::FindCallsAfterConst(int32_t value,
                                                              int window) const
{
    std::map<uint32_t, uint32_t> out;                 // callee -> site count
    const std::vector<uint8_t>& d = data_;
    for (const Func& f : funcs_) {
        size_t q = f.start;
        uint64_t nlocals;
        if (!ULeb(d, q, nlocals)) continue;
        for (uint64_t i = 0; i < nlocals; i++) { uint64_t c; ULeb(d, q, c); q++; }

        int n = 0, seen = -1;
        bool bad = false;
        while (q < f.end && !bad) {
            uint8_t op = d[q++];
            n++;
            if (op == 0xFC) {
                uint64_t sub; if (!ULeb(d, q, sub)) { bad = true; break; }
                int k = FcImmCount((uint32_t)sub);
                if (k < 0) { bad = true; break; }
                for (int i = 0; i < k; i++) { uint64_t t; ULeb(d, q, t); }
                continue;
            }
            uint64_t t; int64_t s;
            switch (ImmFor(op)) {
            case IMM_NONE: break;
            case IMM_BLOCKTYPE:
                if (d[q] == 0x40 || IsValType(d[q])) q++;
                else if (!SLeb(d, q, s)) bad = true;
                break;
            case IMM_U32:
                if (!ULeb(d, q, t)) { bad = true; break; }
                if (op == 0x10 && seen >= 0 && n - seen <= window) {
                    out[(uint32_t)t]++;
                    seen = -1;
                }
                break;
            case IMM_MEMARG: {
                uint64_t a;
                if (!ULeb(d, q, a) || !ULeb(d, q, t)) { bad = true; break; }
                if (a & 0x40) ULeb(d, q, t);
                break;
            }
            case IMM_I32:
                if (!SLeb(d, q, s)) { bad = true; break; }
                if ((int32_t)s == value) seen = n;
                break;
            case IMM_I64: if (!SLeb(d, q, s)) bad = true; break;
            case IMM_F32: q += 4; break;
            case IMM_F64: q += 8; break;
            case IMM_BRTABLE: {
                uint64_t cnt;
                if (!ULeb(d, q, cnt)) { bad = true; break; }
                for (uint64_t i = 0; i <= cnt; i++) ULeb(d, q, t);
                break;
            }
            case IMM_VALTYPE: q += 1; break;
            case IMM_SELECTVEC: {
                uint64_t cnt;
                if (!ULeb(d, q, cnt)) { bad = true; break; }
                q += (size_t)cnt;
                break;
            }
            case IMM_CALL_INDIRECT:
                if (!ULeb(d, q, t) || !ULeb(d, q, t)) bad = true;
                break;
            default: bad = true; break;
            }
        }
    }
    return out;
}

uintptr_t WasmScanner::FindCallAfterConst(int32_t value, int window) const
{
    std::map<uint32_t, uint32_t> hits = FindCallsAfterConst(value, window);
    if (hits.size() != 1) return 0;                   // none, or ambiguous
    return (uintptr_t)hits.begin()->first;
}

bool WasmScanner::FunctionArity(uint32_t func_index, uint32_t* nparams,
                                uint32_t* nresults) const
{
    auto it = func_type_.find(func_index);
    if (it == func_type_.end() || it->second >= types_.size()) return false;
    if (nparams) *nparams = types_[it->second].first;
    if (nresults) *nresults = types_[it->second].second;
    return true;
}

uintptr_t WasmScanner::FindCallAfterConstWithArity(int32_t value,
                                                   uint32_t nparams,
                                                   int window) const
{
    std::map<uint32_t, uint32_t> hits = FindCallsAfterConst(value, window);
    uintptr_t found = 0;
    for (const auto& kv : hits) {
        uint32_t np = 0;
        if (!FunctionArity(kv.first, &np, nullptr) || np != nparams) continue;
        if (found) return 0;                       // still ambiguous
        found = kv.first;
    }
    return found;
}

uintptr_t WasmScanner::FindPattern(const char* pattern, const char* mask,
                                   int offset) const
{
    return FindPatternInRange(pattern, mask, offset,
                              code_lo_ | kCodeOffset, code_hi_ | kCodeOffset);
}

uintptr_t WasmScanner::FindPatternNth(const char* pattern, const char* mask,
                                      size_t nth, int offset) const
{
    const uintptr_t end = (uintptr_t)code_hi_ | kCodeOffset;
    uintptr_t cursor = (uintptr_t)code_lo_ | kCodeOffset;
    for (;;) {
        // Locate with offset 0 so the cursor advances past the match, not past match+offset.
        const uintptr_t found = FindPatternInRange(pattern, mask, 0, cursor, end);
        if (!found) return 0;
        if (!nth) return found + offset;
        --nth;
        cursor = found + 1;
    }
}

uintptr_t WasmScanner::FindPatternInRange(const char* pattern, const char* mask,
                                          int offset, uintptr_t start,
                                          uintptr_t end) const
{
    if (!pattern) return 0;
    const size_t len = mask ? strlen(mask) : strlen(pattern);
    if (!len) return 0;

    const size_t s = (size_t)(start & ~kCodeOffset);
    const size_t e = (size_t)(end & ~kCodeOffset);

    // The pattern is always read forwards from a candidate; only the walk reverses.
    auto matches = [&](size_t i) {
        if (i < code_lo_ || i + len > code_hi_) return false;
        for (size_t k = 0; k < len; k++) {
            if (mask && mask[k] != 'x') continue;          // wildcard
            if (data_[i + k] != (uint8_t)pattern[k]) return false;
        }
        return true;
    };

    // end < start means scan backward, as on x86. x86 subtracts the pattern length from
    // `end` before choosing a direction, so the window reaches that much further back.
    if (e < s) {
        size_t hi = s;
        if (hi + len > code_hi_) {
            if (code_hi_ < len) return 0;
            hi = code_hi_ - len;
        }
        size_t lo = e < len ? 0 : e - len;
        if (lo < code_lo_) lo = code_lo_;
        if (hi < lo) return 0;
        for (size_t i = hi + 1; i-- > lo; )
            if (matches(i)) return ((uintptr_t)(i + offset)) | kCodeOffset;
        return 0;
    }

    size_t lo = s;
    size_t hi = e;
    if (lo < code_lo_) lo = code_lo_;
    if (hi > code_hi_ || hi == 0) hi = code_hi_;
    if (lo + len > hi) return 0;

    for (size_t i = lo; i + len <= hi; i++)
        if (matches(i)) return ((uintptr_t)(i + offset)) | kCodeOffset;
    return 0;
}

uintptr_t WasmScanner::FunctionAtCodeOffset(uintptr_t tagged) const
{
    if (!IsCodeOffset(tagged)) return tagged;      // already a function index
    return FuncAt((size_t)(tagged & ~kCodeOffset));
}

uintptr_t WasmScanner::ToFunctionStart(uintptr_t x) const
{
    // Normalise to a function index, then return that function's body start as a tagged code offset to anchor further scans.
    uint32_t idx = (uint32_t)FunctionAtCodeOffset(x);
    if (idx < num_func_imports_) return 0;         // 0/import/out-of-range
    size_t i = idx - num_func_imports_;
    if (i >= funcs_.size()) return 0;
    return (uintptr_t)funcs_[i].start | kCodeOffset;
}

uintptr_t WasmScanner::CallTargetAt(uintptr_t tagged) const
{
    size_t off = (size_t)(tagged & ~kCodeOffset);
    if (off < code_lo_ || off >= code_hi_) return 0;
    if (data_[off] != 0x10) return 0;              // not a `call`
    size_t q = off + 1;
    uint64_t f;
    if (!ULeb(data_, q, f)) return 0;
    return (uintptr_t)f;
}

uintptr_t WasmScanner::GlobalAtCodeOffset(uintptr_t tagged) const
{
    size_t off = (size_t)(tagged & ~kCodeOffset);
    if (off < code_lo_ || off >= code_hi_) return 0;
    // Memory load/store opcodes 0x28..0x3E all carry a memarg (align, offset).
    if (data_[off] < 0x28 || data_[off] > 0x3E) return 0;
    size_t q = off + 1;
    uint64_t align, moff;
    if (!ULeb(data_, q, align)) return 0;          // align (the 0x40 bit adds a
    if (!ULeb(data_, q, moff)) return 0;           // memidx after offset; unread)
    return (uintptr_t)moff;
}

uintptr_t WasmScanner::ConstAtCodeOffset(uintptr_t tagged) const
{
    size_t off = (size_t)(tagged & ~kCodeOffset);
    if (off < code_lo_ || off >= code_hi_) return 0;
    if (data_[off] != 0x41) return 0;              // i32.const
    size_t q = off + 1;
    int64_t v;
    if (!SLeb(data_, q, v)) return 0;
    return (uintptr_t)(uint32_t)v;
}

std::vector<uint32_t> WasmScanner::GlobalsInFunction(uint32_t func_index) const
{
    auto it = globals_.find(func_index);
    return it == globals_.end() ? std::vector<uint32_t>() : it->second;
}

uintptr_t WasmScanner::GlobalFromFunction(uintptr_t func_index, size_t nth) const
{
    std::vector<uint32_t> g = GlobalsInFunction((uint32_t)func_index);
    return nth < g.size() ? (uintptr_t)g[nth] : 0;
}

std::vector<uint32_t> WasmScanner::BssConstsInFunction(uint32_t func_index) const
{
    std::vector<uint32_t> out;
    auto it = std::find_if(funcs_.begin(), funcs_.end(),
                           [&](const Func& f) { return f.index == func_index; });
    if (it == funcs_.end()) return out;

    const std::vector<uint8_t>& d = data_;
    size_t q = it->start;
    uint64_t nlocals;
    if (!ULeb(d, q, nlocals)) return out;
    for (uint64_t i = 0; i < nlocals; i++) {
        uint64_t c;
        if (!ULeb(d, q, c)) return out;
        q++;
    }
    while (q < it->end) {
        uint8_t op = d[q++];
        if (op == 0x41) {                       // i32.const
            int64_t v;
            if (!SLeb(d, q, v)) break;
            if (v >= (int64_t)data_hi_ &&
                std::find(out.begin(), out.end(), (uint32_t)v) == out.end())
                out.push_back((uint32_t)v);
            continue;
        }
        // skip the rest by immediate shape
        uint64_t t; int64_t s;
        switch (ImmFor(op)) {
        case IMM_NONE: break;
        case IMM_BLOCKTYPE:
            if (d[q] == 0x40 || IsValType(d[q])) q++;
            else if (!SLeb(d, q, s)) return out;
            break;
        case IMM_U32: if (!ULeb(d, q, t)) return out; break;
        case IMM_MEMARG: {
            uint64_t a;
            if (!ULeb(d, q, a) || !ULeb(d, q, t)) return out;
            if (a & 0x40) ULeb(d, q, t);
            break;
        }
        case IMM_I64: if (!SLeb(d, q, s)) return out; break;
        case IMM_F32: q += 4; break;
        case IMM_F64: q += 8; break;
        case IMM_BRTABLE: {
            uint64_t cnt;
            if (!ULeb(d, q, cnt)) return out;
            for (uint64_t i = 0; i <= cnt; i++) ULeb(d, q, t);
            break;
        }
        case IMM_VALTYPE: q += 1; break;
        case IMM_SELECTVEC: {
            uint64_t cnt;
            if (!ULeb(d, q, cnt)) return out;
            q += (size_t)cnt;
            break;
        }
        case IMM_CALL_INDIRECT:
            if (!ULeb(d, q, t) || !ULeb(d, q, t)) return out;
            break;
        default:
            if (op == 0xFC) {
                uint64_t sub;
                if (!ULeb(d, q, sub)) return out;
                int k = FcImmCount((uint32_t)sub);
                if (k < 0) return out;
                for (int i = 0; i < k; i++) ULeb(d, q, t);
            }
            break;
        }
    }
    return out;
}

uintptr_t WasmScanner::BssConstFromFunction(uintptr_t func_index, size_t nth) const
{
    std::vector<uint32_t> v = BssConstsInFunction((uint32_t)func_index);
    return nth < v.size() ? (uintptr_t)v[nth] : 0;
}

std::vector<uint32_t> WasmScanner::FunctionsUsingGlobal(uint32_t addr) const
{
    std::vector<uint32_t> out;
    for (const auto& kv : globals_)
        if (std::find(kv.second.begin(), kv.second.end(), addr) != kv.second.end())
            out.push_back(kv.first);
    return out;
}

int WasmScanner::TableSlot(uint32_t func_index) const
{
    auto it = table_.find(func_index);
    return it == table_.end() ? -1 : it->second;
}

uintptr_t WasmScanner::ToCallable(uintptr_t func_index) const
{
    int slot = TableSlot((uint32_t)func_index);
    return slot < 0 ? 0 : (uintptr_t)slot;
}

uint32_t WasmScanner::FuncAt(size_t code_off) const
{
    size_t lo = 0, hi = funcs_.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (code_off < funcs_[mid].start) hi = mid;
        else if (code_off >= funcs_[mid].end) lo = mid + 1;
        else return funcs_[mid].index;
    }
    return 0;
}
