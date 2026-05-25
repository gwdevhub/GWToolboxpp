#pragma once

#include <cassert>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "gwtoolboxdll_export.h"

// ---------------------------------------------------------------------------
// FastIni – a fast INI parser designed for GWToolbox++.
//
// Drop-in surface mirroring the CSimpleIni API:
//   GetValue / SetValue / GetLongValue / SetLongValue / GetDoubleValue /
//   SetDoubleValue / GetBoolValue / SetBoolValue / GetAllValues / Delete /
//   GetAllSections / GetAllKeys / LoadFile / SaveFile / Reset
//
// Internally:
//  - The whole file is read into a single buffer in one syscall.
//  - One linear scan finds section boundaries; each section is then parsed
//    in a second pass — all on the calling thread, no overhead.
//  - Per-entry typed conversions are cached so repeated reads of the same
//    key as the same type are free after the first call.
//  - Multi-key is supported: each key maps to a vector of entries. In the
//    single-value case (overwhelmingly common) that vector has one element
//    and the typed getters address [0] directly.
//  - GetAllSections is O(n) on first call after a structural change, then
//    O(n) copy-only on repeated calls (no map traversal).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// SI_Error – mirrors CSimpleIni's return codes so call sites need no changes.
// ---------------------------------------------------------------------------
enum SI_Error {
    SI_OK       =  0,  // success, no change to existing data
    SI_UPDATED  =  1,  // success, existing key/value was updated
    SI_INSERTED =  2,  // success, new key/section was inserted
    SI_FAIL     = -1,  // generic failure (e.g. null argument, I/O error)
};

// ---------------------------------------------------------------------------
// SI_Entry / TNamesDepend – mirrors CSimpleIni's structure exactly.
//
// pItem    → points into string storage owned by FastIni; valid until the
//            next Reset(), LoadFile(), or destruction of the FastIni object.
// pComment → always nullptr (we strip comments on parse).
// nOrder   → always 0 (we use an unordered_map; no insertion order).
// ---------------------------------------------------------------------------
struct SI_Entry {
    const char* pItem    = nullptr;
    const char* pComment = nullptr;
    int         nOrder   = 0;

    SI_Entry() = default;
    explicit SI_Entry(const char* item) : pItem(item) {}

    struct LoadOrder {
        bool operator()(const SI_Entry& a, const SI_Entry& b) const {
            return a.nOrder < b.nOrder;
        }
    };
};

using TNamesDepend = std::list<SI_Entry>;

// ---------------------------------------------------------------------------
// FastIniEntry – one value for a key, with a lazily-cached typed conversion.
// ---------------------------------------------------------------------------
using FastIniCachedValue = std::variant<
    std::monostate,  // not yet converted
    long,
    double,
    bool
>;

struct FastIniEntry {
    std::string        raw;      // always valid after parse
    FastIniCachedValue cached{}; // populated on first typed get

    FastIniEntry() = default;
    explicit FastIniEntry(std::string_view v) : raw(v) {}
    explicit FastIniEntry(std::string v)      : raw(std::move(v)) {}
};

// ---------------------------------------------------------------------------
// FastIniSection – owns the key→entries map for one [section].
// Each key maps to a vector of FastIniEntry; single-value keys have one
// element, multi-value keys have more.
// ---------------------------------------------------------------------------
struct FastIniSection {
    std::unordered_map<std::string, std::vector<FastIniEntry>> keys;

    // Typed getters – operate on the first value for the key.
    const char* GetValue (std::string_view key, const char* def = "")   const;
    long        GetLong  (std::string_view key, long        def = 0)    const;
    double      GetDouble(std::string_view key, double      def = 0.0)  const;
    bool        GetBool  (std::string_view key, bool        def = false) const;

    // Typed setters – always write/replace the first (or only) value.
    SI_Error SetValue (std::string_view key, const char* value);
    SI_Error SetLong  (std::string_view key, long        value);
    SI_Error SetDouble(std::string_view key, double      value, int precision = 6);
    SI_Error SetBool  (std::string_view key, bool        value);

    // Retrieve all values for a key into a TNamesDepend list.
    bool GetAllValues(std::string_view key, TNamesDepend& out) const;

    bool HasKey(std::string_view key) const;
    bool Delete(std::string_view key);
    void GetAllKeys(TNamesDepend& out) const;

private:
    // Returns {first entry, wasNewKey}. Appends if multiKey, replaces if not.
    std::pair<FastIniEntry&, bool> findOrCreate(std::string_view key, bool multiKey = false);
    std::vector<FastIniEntry>*       find(std::string_view key);
    const std::vector<FastIniEntry>* find(std::string_view key) const;
};

// ---------------------------------------------------------------------------
// FastIni – the document.
// ---------------------------------------------------------------------------
class FastIni {
public:
    GWTOOLBOXDLL_EXPORT explicit FastIni(bool a_bIsUtf8 = false, bool a_bMultiKey = false, bool a_bMultiLine = false)
        : m_multiKey(a_bMultiKey)
    {
        assert(!a_bIsUtf8   && "FastIni: UTF-8 mode is not supported");
        assert(!a_bMultiLine && "FastIni: multi-line mode is not supported");
        (void)a_bIsUtf8; (void)a_bMultiLine;
    }

    // load / save – filesystem::path is the real implementation; all other
    // overloads construct a path and forward so wide strings work on Windows.
    GWTOOLBOXDLL_EXPORT bool LoadFile(const std::filesystem::path& path);
    GWTOOLBOXDLL_EXPORT void LoadBuffer(std::string_view buf);

    GWTOOLBOXDLL_EXPORT SI_Error SaveFile(const std::filesystem::path& path, bool addUtf8BOM = false) const;

    GWTOOLBOXDLL_EXPORT void Reset();

    // CSimpleIni-compatible surface
    GWTOOLBOXDLL_EXPORT const char* GetValue(const char* section, const char* key, const char* def = "") const;
    GWTOOLBOXDLL_EXPORT SI_Error SetValue(const char* section, const char* key, const char* value,
                               const char* = nullptr, bool = false);
    GWTOOLBOXDLL_EXPORT long GetLongValue(const char* section, const char* key, long def = 0) const;
    GWTOOLBOXDLL_EXPORT SI_Error SetLongValue(const char* section, const char* key, long value,
                               const char* = nullptr, bool = false, bool = false);
    GWTOOLBOXDLL_EXPORT double GetDoubleValue(const char* section, const char* key, double def = 0.0) const;
    GWTOOLBOXDLL_EXPORT SI_Error SetDoubleValue(const char* section, const char* key, double value,
                               const char* = nullptr, bool = false);
    GWTOOLBOXDLL_EXPORT bool GetBoolValue(const char* section, const char* key, bool def = false) const;
    GWTOOLBOXDLL_EXPORT SI_Error SetBoolValue(const char* section, const char* key, bool value,
                               const char* = nullptr, bool = false);

    bool GetAllValues(const char* section, const char* key, TNamesDepend& out) const;

    GWTOOLBOXDLL_EXPORT bool IsEmpty() const { return m_sections.empty(); }
    void SetMultiKey(bool a_bAllowMultiKey) { m_multiKey = a_bAllowMultiKey; }

    // key == nullptr → delete whole section
    GWTOOLBOXDLL_EXPORT bool Delete(const char* section, const char* key, bool = false);
    GWTOOLBOXDLL_EXPORT bool KeyExists(const char* section, const char* key) const;
    GWTOOLBOXDLL_EXPORT bool SectionExists(const char* section) const;

    void GetAllSections(TNamesDepend& out) const;
    bool GetAllKeys(const char* section, TNamesDepend& out) const;

    // Direct section access
    GWTOOLBOXDLL_EXPORT FastIniSection* GetSection(std::string_view name);
    GWTOOLBOXDLL_EXPORT const FastIniSection* GetSection(std::string_view name) const;
    GWTOOLBOXDLL_EXPORT FastIniSection& GetOrCreateSection(std::string_view name);

private:
    std::unordered_map<std::string, FastIniSection> m_sections;
    bool                                            m_multiKey = false;

    // Cache for GetAllSections – rebuilt lazily after any structural change.
    // Stores const char* into m_sections key strings, which are stable for
    // the lifetime of each node (unordered_map does not move keys on
    // insert/erase of other entries).
    mutable std::vector<const char*> m_sectionNameCache;
    mutable bool                     m_sectionCacheDirty = true;

    void rebuildSectionCache() const;

    struct SectionSpan { std::string name; std::string_view text; };
    static std::vector<SectionSpan> splitSections(std::string_view buf);
    void parseSection(FastIniSection& sec, std::string_view text) const;
};
