#include "FastIni.h"

#include <algorithm>
#include <cstdio>

// ===========================================================================
// Utilities
// ===========================================================================

static std::string_view trim(std::string_view s) {
    constexpr std::string_view ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// ===========================================================================
// FastIniSection – private helpers
// ===========================================================================

std::vector<FastIniEntry>* FastIniSection::find(std::string_view key) {
    auto it = keys.find(std::string(key));
    return it != keys.end() ? &it->second : nullptr;
}

const std::vector<FastIniEntry>* FastIniSection::find(std::string_view key) const {
    auto it = keys.find(std::string(key));
    return it != keys.end() ? &it->second : nullptr;
}

std::pair<FastIniEntry&, bool> FastIniSection::findOrCreate(std::string_view key, bool multiKey) {
    auto [it, inserted] = keys.try_emplace(std::string(key));
    auto& vec = it->second;
    if (inserted || vec.empty()) {
        vec.emplace_back();
        return {vec.back(), true};
    }
    if (multiKey) {
        // Append a new slot for this duplicate key
        vec.emplace_back();
        return {vec.back(), false}; // section existed, key existed → SI_UPDATED
    }
    // Single-key mode: overwrite the first value
    return {vec.front(), false};
}

// ===========================================================================
// FastIniSection – typed getters (operate on first value)
// ===========================================================================

const char* FastIniSection::GetValue(std::string_view key, const char* def) const {
    const auto* vec = find(key);
    return (vec && !vec->empty()) ? (*vec)[0].raw.c_str() : def;
}

long FastIniSection::GetLong(std::string_view key, long def) const {
    auto* vec = const_cast<FastIniSection*>(this)->find(key);
    if (!vec || vec->empty()) return def;
    FastIniEntry& e = (*vec)[0];
    if (std::holds_alternative<long>(e.cached))
        return std::get<long>(e.cached);

    long v = def;
    auto sv = trim(e.raw);
    int base = 10;
    if (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
        base = 16;
        sv = sv.substr(2);
    }
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v, base);
    if (ec == std::errc{}) e.cached = v;
    return v;
}

double FastIniSection::GetDouble(std::string_view key, double def) const {
    auto* vec = const_cast<FastIniSection*>(this)->find(key);
    if (!vec || vec->empty()) return def;
    FastIniEntry& e = (*vec)[0];
    if (std::holds_alternative<double>(e.cached))
        return std::get<double>(e.cached);

    double v = def;
    char buf[64];
    auto sv = trim(e.raw);
    auto len = std::min(sv.size(), sizeof(buf) - 1);
    std::memcpy(buf, sv.data(), len);
    buf[len] = '\0';
    char* end = nullptr;
    double parsed = std::strtod(buf, &end);
    if (end != buf) { v = parsed; e.cached = v; }
    return v;
}

bool FastIniSection::GetBool(std::string_view key, bool def) const {
    auto* vec = const_cast<FastIniSection*>(this)->find(key);
    if (!vec || vec->empty()) return def;
    FastIniEntry& e = (*vec)[0];
    if (std::holds_alternative<bool>(e.cached))
        return std::get<bool>(e.cached);

    auto sv = trim(e.raw);
    bool v = def;
    if (sv=="1"||sv=="true"||sv=="True"||sv=="TRUE"||sv=="yes"||sv=="Yes"||sv=="YES")
        v = true;
    else if (sv=="0"||sv=="false"||sv=="False"||sv=="FALSE"||sv=="no"||sv=="No"||sv=="NO")
        v = false;
    e.cached = v;
    return v;
}

// ===========================================================================
// FastIniSection – typed setters (always write the first/only value)
// ===========================================================================

SI_Error FastIniSection::SetValue(std::string_view key, const char* value) {
    auto [e, inserted] = findOrCreate(key);
    e.raw    = value ? value : "";
    e.cached = std::monostate{};
    return inserted ? SI_INSERTED : SI_UPDATED;
}

SI_Error FastIniSection::SetLong(std::string_view key, long value) {
    auto [e, inserted] = findOrCreate(key);
    e.raw    = std::to_string(value);
    e.cached = value;
    return inserted ? SI_INSERTED : SI_UPDATED;
}

SI_Error FastIniSection::SetDouble(std::string_view key, double value, int precision) {
    auto [e, inserted] = findOrCreate(key);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", precision, value);
    e.raw    = buf;
    e.cached = value;
    return inserted ? SI_INSERTED : SI_UPDATED;
}

SI_Error FastIniSection::SetBool(std::string_view key, bool value) {
    auto [e, inserted] = findOrCreate(key);
    e.raw    = value ? "true" : "false";
    e.cached = value;
    return inserted ? SI_INSERTED : SI_UPDATED;
}

// ===========================================================================
// FastIniSection – multi-value / existence / deletion
// ===========================================================================

bool FastIniSection::GetAllValues(std::string_view key, TNamesDepend& out) const {
    const auto* vec = find(key);
    if (!vec || vec->empty()) return false;
    for (const auto& e : *vec)
        out.push_back(SI_Entry(e.raw.c_str()));
    return true;
}

bool FastIniSection::HasKey(std::string_view key) const {
    const auto* vec = find(key);
    return vec && !vec->empty();
}

bool FastIniSection::Delete(std::string_view key) {
    return keys.erase(std::string(key)) > 0;
}

void FastIniSection::GetAllKeys(TNamesDepend& out) const {
    for (auto& [k, vec] : keys)
        if (!vec.empty())
            out.push_back(SI_Entry(k.c_str()));
}

// ===========================================================================
// FastIni – I/O
// ===========================================================================

bool FastIni::LoadFile(const std::filesystem::path& path) {
#ifdef _WIN32
    std::FILE* f = _wfopen(path.c_str(), L"rb");
#else
    std::FILE* f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);

    if (sz <= 0) { std::fclose(f); return true; }

    std::string buf(static_cast<size_t>(sz), '\0');
    [[maybe_unused]] auto n = std::fread(buf.data(), 1, static_cast<size_t>(sz), f);
    std::fclose(f);

    std::string_view view(buf);
    // Strip UTF-8 BOM
    if (view.size() >= 3 &&
        (unsigned char)view[0] == 0xEF &&
        (unsigned char)view[1] == 0xBB &&
        (unsigned char)view[2] == 0xBF)
    {
        view = view.substr(3);
    }

    LoadBuffer(view);
    return true;
}

void FastIni::LoadBuffer(std::string_view buf) {
    Reset();
    for (auto& sp : splitSections(buf)) {
        auto& sec = m_sections[sp.name];
        parseSection(sec, sp.text);
    }
    // Reset() already dirtied the cache; no need to dirty again here.
}

SI_Error FastIni::SaveFile(const std::filesystem::path& path, bool addUtf8BOM) const {
    // Pre-calculate total size to avoid reallocations.
    size_t total = addUtf8BOM ? 3 : 0;
    for (auto& [secName, sec] : m_sections) {
        total += 1 + secName.size() + 2; // '[' + name + "]\n"
        for (auto& [k, vec] : sec.keys)
            for (auto& e : vec)
                total += k.size() + 1 + e.raw.size() + 1; // key=value\n
        total += 1; // blank line between sections
    }

    std::string buf;
    buf.reserve(total);

    if (addUtf8BOM) {
        buf += '\xef';
        buf += '\xbb';
        buf += '\xbf';
    }

    for (auto& [secName, sec] : m_sections) {
        buf += '['; buf += secName; buf += "]\n";
        for (auto& [k, vec] : sec.keys)
            for (auto& e : vec) {
                buf += k; buf += '='; buf += e.raw; buf += '\n';
            }
        buf += '\n';
    }

#ifdef _WIN32
    std::FILE* f = _wfopen(path.c_str(), L"wb");
#else
    std::FILE* f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return SI_FAIL;
    [[maybe_unused]] auto written = std::fwrite(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return SI_OK;
}

void FastIni::Reset() {
    m_sections.clear();
    m_sectionNameCache.clear();
    m_sectionCacheDirty = true;
}

// ===========================================================================
// FastIni – section name cache
// ===========================================================================

void FastIni::rebuildSectionCache() const {
    m_sectionNameCache.clear();
    m_sectionNameCache.reserve(m_sections.size());
    for (const auto& [name, _] : m_sections)
        m_sectionNameCache.push_back(name.c_str());
    m_sectionCacheDirty = false;
}

// ===========================================================================
// FastIni – parsing
// ===========================================================================

std::vector<FastIni::SectionSpan> FastIni::splitSections(std::string_view buf) {
    std::vector<SectionSpan> spans;
    std::string_view remaining = buf;
    std::string      currentName;
    const char*      bodyStart = nullptr;

    auto flush = [&](const char* bodyEnd) {
        if (bodyStart && !currentName.empty()) {
            spans.push_back({
                std::move(currentName),
                std::string_view(bodyStart, static_cast<size_t>(bodyEnd - bodyStart))
            });
        }
    };

    while (!remaining.empty()) {
        auto nl = remaining.find('\n');
        std::string_view line = (nl == std::string_view::npos)
            ? remaining : remaining.substr(0, nl + 1);

        auto t = trim(line);
        if (!t.empty() && t[0] == '[') {
            flush(line.data());
            auto close = t.find(']');
            if (close != std::string_view::npos)
                currentName = std::string(t.substr(1, close - 1));
            bodyStart = line.data() + line.size();
        }

        if (nl == std::string_view::npos) break;
        remaining = remaining.substr(nl + 1);
    }

    flush(buf.data() + buf.size());
    return spans;
}

void FastIni::parseSection(FastIniSection& sec, std::string_view text) const {
    while (!text.empty()) {
        auto nl = text.find('\n');
        std::string_view line = (nl == std::string_view::npos)
            ? text : text.substr(0, nl);

        auto t = trim(line);
        if (!t.empty() && t[0] != ';' && t[0] != '#') {
            auto eq = t.find('=');
            if (eq != std::string_view::npos) {
                auto k = trim(t.substr(0, eq));
                auto v = trim(t.substr(eq + 1));
                // Strip inline comments (must be preceded by whitespace)
                for (size_t i = 0; i < v.size(); ++i) {
                    if ((v[i] == ';' || v[i] == '#') &&
                        i > 0 && (v[i-1] == ' ' || v[i-1] == '\t'))
                    {
                        v = trim(v.substr(0, i));
                        break;
                    }
                }
                if (!k.empty()) {
                    auto [it, inserted] = sec.keys.try_emplace(std::string(k));
                    if (inserted || m_multiKey)
                        it->second.emplace_back(std::string(v));
                    else
                        it->second[0] = FastIniEntry(std::string(v)); // overwrite in single-key mode
                }
            }
        }

        if (nl == std::string_view::npos) break;
        text = text.substr(nl + 1);
    }
}

// ===========================================================================
// FastIni – section access
// ===========================================================================

FastIniSection* FastIni::GetSection(std::string_view name) {
    auto it = m_sections.find(std::string(name));
    return it != m_sections.end() ? &it->second : nullptr;
}

const FastIniSection* FastIni::GetSection(std::string_view name) const {
    auto it = m_sections.find(std::string(name));
    return it != m_sections.end() ? &it->second : nullptr;
}

FastIniSection& FastIni::GetOrCreateSection(std::string_view name) {
    auto [it, inserted] = m_sections.emplace(std::string(name), FastIniSection{});
    if (inserted) m_sectionCacheDirty = true;
    return it->second;
}

// ===========================================================================
// FastIni – CSimpleIni-compatible surface
// ===========================================================================

const char* FastIni::GetValue(const char* section, const char* key, const char* def) const {
    const FastIniSection* s = GetSection(section); return s ? s->GetValue(key, def) : def;
}
long FastIni::GetLongValue(const char* section, const char* key, long def) const {
    const FastIniSection* s = GetSection(section); return s ? s->GetLong(key, def) : def;
}
double FastIni::GetDoubleValue(const char* section, const char* key, double def) const {
    const FastIniSection* s = GetSection(section); return s ? s->GetDouble(key, def) : def;
}
bool FastIni::GetBoolValue(const char* section, const char* key, bool def) const {
    const FastIniSection* s = GetSection(section); return s ? s->GetBool(key, def) : def;
}

SI_Error FastIni::SetValue(const char* section, const char* key, const char* value,
                            const char*, bool) {
    auto [sec, secInserted] = m_sections.try_emplace(std::string(section));
    if (secInserted) m_sectionCacheDirty = true;
    SI_Error r = sec->second.SetValue(key, value);
    return secInserted ? SI_INSERTED : r;
}
SI_Error FastIni::SetLongValue(const char* section, const char* key, long value,
                                const char*, bool, bool) {
    auto [sec, secInserted] = m_sections.try_emplace(std::string(section));
    if (secInserted) m_sectionCacheDirty = true;
    SI_Error r = sec->second.SetLong(key, value);
    return secInserted ? SI_INSERTED : r;
}
SI_Error FastIni::SetDoubleValue(const char* section, const char* key, double value,
                                  const char*, bool) {
    auto [sec, secInserted] = m_sections.try_emplace(std::string(section));
    if (secInserted) m_sectionCacheDirty = true;
    SI_Error r = sec->second.SetDouble(key, value);
    return secInserted ? SI_INSERTED : r;
}
SI_Error FastIni::SetBoolValue(const char* section, const char* key, bool value,
                                const char*, bool) {
    auto [sec, secInserted] = m_sections.try_emplace(std::string(section));
    if (secInserted) m_sectionCacheDirty = true;
    SI_Error r = sec->second.SetBool(key, value);
    return secInserted ? SI_INSERTED : r;
}

bool FastIni::GetAllValues(const char* section, const char* key, TNamesDepend& out) const {
    const FastIniSection* s = GetSection(section);
    return s && s->GetAllValues(key, out);
}

bool FastIni::KeyExists(const char* section, const char* key) const {
    const FastIniSection* s = GetSection(section);
    return s && s->HasKey(key);
}

bool FastIni::SectionExists(const char* section) const {
    return GetSection(section) != nullptr;
}

bool FastIni::Delete(const char* section, const char* key, bool) {
    auto it = m_sections.find(section);
    if (it == m_sections.end()) return false;
    if (key == nullptr) {
        m_sections.erase(it);
        m_sectionCacheDirty = true;
        return true;
    }
    return it->second.Delete(key);
}

void FastIni::GetAllSections(TNamesDepend& out) const {
    if (m_sectionCacheDirty) rebuildSectionCache();
    for (const char* p : m_sectionNameCache)
        out.push_back(SI_Entry(p));
}

bool FastIni::GetAllKeys(const char* section, TNamesDepend& out) const {
    const FastIniSection* s = GetSection(section);
    if (!s) return false;
    s->GetAllKeys(out);
    return true;
}
