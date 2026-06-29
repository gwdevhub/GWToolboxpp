#include "TeamBuildEncoder.h"

#include <algorithm>
#include <cstring>
#include <format>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include "TextUtils.h"

namespace TeamBuildEncoder {

    // Generated from account unlocked skills array (70 entries, 2240 skill slots)
    constexpr uint32_t kUnlockedSkills[] = {0xFFEFEFE6u, 0xEFFFFFFFu, 0xFFFFFF3Eu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFDu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFDFFFFu, 0x9BFDFFFFu, 0xFFFFFFC0u,
                                            0x3DFFFFFFu, 0x00000000u, 0x00000000u, 0x1C000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x58000000u, 0x8FFFFFBEu, 0xDFFFFF7Fu, 0xDC7FFFFDu, 0x3BFFF0EFu,
                                            0xEBFEFFFCu, 0xEFFDEFFFu, 0xFEFFE01Fu, 0xFC00003Fu, 0xF33F7FFFu, 0x7EF8FC6Bu, 0xEE000FEDu, 0x0563EF4Bu, 0x00000000u, 0xBA6FFD80u, 0xF1FFFFFEu, 0x003FFEBEu, 0x00000000u, 0xFFE00000u,
                                            0xFFFDDFFFu, 0xFFFFC07Fu, 0x000001FFu, 0xFE000000u, 0xDFAFFFDFu, 0xFFFFFF7Fu, 0xFFFFFFFFu, 0xFFFFBFFFu, 0x0000003Fu, 0xFFFFFFFEu, 0xFFF80001u, 0xFF80007Fu, 0xFFFBFF7Fu, 0x01FFFFFFu,
                                            0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xFFFFFFDCu, 0x00000007u, 0x0FFFFFF0u, 0x00000000u, 0xFF800000u, 0x0000007Fu, 0xFFFFFC00u, 0x70000007u};

    bool IsSkillEquippable(const GW::Constants::SkillID skill_id)
    {
        const uint32_t id = static_cast<uint32_t>(skill_id);
        const uint32_t idx = id / 32;
        if (idx >= std::size(kUnlockedSkills)) return false;
        return (kUnlockedSkills[idx] & (1u << (id % 32))) != 0;
    }

    // ---------------------------------------------------------------------------
    // Bit I/O
    // ---------------------------------------------------------------------------

    struct BitWriter {
        std::vector<uint8_t> buf;
        int pos = 0;

        void write(uint32_t val, int n)
        {
            for (int i = 0; i < n; i++) {
                if (pos / 8 >= static_cast<int>(buf.size())) buf.push_back(0);
                if ((val >> i) & 1u) buf[pos / 8] |= static_cast<uint8_t>(1u << (pos % 8));
                pos++;
            }
        }
    };

    struct BitReader {
        const uint8_t* data = nullptr;
        int byte_len = 0;
        int pos = 0;
        bool ok = true;

        BitReader() = default;
        BitReader(const uint8_t* d, int len) : data(d), byte_len(len) {}

        uint32_t read(int n)
        {
            uint32_t v = 0;
            for (int i = 0; i < n; i++) {
                if (pos / 8 >= byte_len) {
                    ok = false;
                    return 0;
                }
                if ((data[pos / 8] >> (pos % 8)) & 1u) v |= 1u << i;
                pos++;
            }
            return v;
        }
    };

    // ---------------------------------------------------------------------------
    // EncodedTeamBuild wchar encoding
    //
    // Two encoding tables, selected by GW region at encode time and identified
    // by the magic nibble at decode time. All confirmed working as chat link chars.
    //
    // Both tables share a 479-codepoint base:
    //   Basic Latin (excl ;<>[]): 90   U+0020-U+007E
    //   Latin-1+Extended-A:      223   U+00A1-U+017F
    //   General Punctuation:     166   U+2010-U+20B5
    //
    // Topped up with 33 region-specific codepoints:
    //   Western (0xE): U+0391-U+03B1  Greek+Cyrillic  (America, Europe, International)
    //   Asian   (0xD): U+FF01-U+FF21  Halfwidth Forms  (Korea, China, Japan)
    //
    // Halfwidth confirmed working in all Asian districts as chat link chars.
    // Greek+Cyrillic confirmed working in Western districts.
    // Magic nibble is essential — first wchar is always from shared Basic Latin base.
    //
    // Excludes ; U+003B, < U+003C, > U+003E, [ U+005B, ] U+005D
    // ---------------------------------------------------------------------------

    constexpr uint32_t kMagicWestern = 0xE;
    constexpr uint32_t kMagicAsian = 0xD;

    struct kRanges {
        uint32_t base, size;
    };

#define TEAMBUILD_BASE_RANGES                                                \
    {0x0020, 27},      /* U+0020-U+003A  Basic Latin: space-colon        */  \
        {0x003D, 1},   /* U+003D         Basic Latin: =                   */ \
        {0x003F, 28},  /* U+003F-U+005A  Basic Latin: ?@A-Z              */  \
        {0x005C, 1},   /* U+005C         Basic Latin: backslash           */ \
        {0x005E, 33},  /* U+005E-U+007E  Basic Latin: ^_`a-z{|}~         */  \
        {0x00A1, 223}, /* U+00A1-U+017F  Latin-1 Supplement + Extended-A */  \
        {0x2010, 166}, /* U+2010-U+20B5  General Punctuation + Currency  */

    constexpr kRanges kRangesWestern[] = {
        TEAMBUILD_BASE_RANGES{0x0391, 33}, // U+0391-U+03B1  Greek+Cyrillic (Western/Europe)
    };

    constexpr kRanges kRangesAsian[] = {
        TEAMBUILD_BASE_RANGES{0xFF01, 33}, // U+FF01-U+FF21  Halfwidth Forms (Korea/China/Japan)
    };

#undef TEAMBUILD_BASE_RANGES

    // ---------------------------------------------------------------------------
    // Generic Val<->wchar conversion, templated on range table
    // ---------------------------------------------------------------------------

    template <typename RangeEntry>
    static wchar_t ValToWcharT(uint32_t val, const RangeEntry* ranges, int count)
    {
        for (int i = 0; i < count; i++) {
            if (val < ranges[i].size) return static_cast<wchar_t>(ranges[i].base + val);
            val -= ranges[i].size;
        }
        return L'?';
    }

    template <typename RangeEntry>
    static int WcharToValT(wchar_t wc, const RangeEntry* ranges, int count)
    {
        uint32_t offset = 0;
        const uint32_t cp = static_cast<uint32_t>(wc);
        for (int i = 0; i < count; i++) {
            if (cp >= ranges[i].base && cp < ranges[i].base + ranges[i].size) return static_cast<int>(offset + (cp - ranges[i].base));
            offset += ranges[i].size;
        }
        return -1;
    }

    // ---------------------------------------------------------------------------
    // Generic BytesToEncoded / EncodedToBytes
    // ---------------------------------------------------------------------------

    template <typename RangeEntry>
    static EncodedTeamBuild BytesToEncodedT(const std::vector<uint8_t>& bytes, const RangeEntry* ranges, int range_count, int bits_per_wchar)
    {
        EncodedTeamBuild result;
        result.reserve((static_cast<int>(bytes.size()) * 8 + bits_per_wchar - 1) / bits_per_wchar);
        int bit = 0;
        const int total_bits = static_cast<int>(bytes.size()) * 8;
        while (bit < total_bits) {
            uint32_t val = 0;
            for (int i = 0; i < bits_per_wchar && bit < total_bits; i++, bit++) {
                if ((bytes[bit / 8] >> (bit % 8)) & 1u) val |= 1u << i;
            }
            result += ValToWcharT(val, ranges, range_count);
        }
        return result;
    }

    template <typename RangeEntry>
    static std::vector<uint8_t> EncodedToBytesT(const EncodedTeamBuild& encoded, const RangeEntry* ranges, int range_count, int bits_per_wchar)
    {
        std::vector<uint8_t> bytes;
        bytes.reserve((static_cast<int>(encoded.size()) * bits_per_wchar + 7) / 8);
        int bit = 0;
        for (const wchar_t wc : encoded) {
            const int val = WcharToValT(wc, ranges, range_count);
            if (val < 0) return {};
            for (int i = 0; i < bits_per_wchar; i++, bit++) {
                if (bit / 8 >= static_cast<int>(bytes.size())) bytes.push_back(0);
                if ((val >> i) & 1u) bytes[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
            }
        }
        return bytes;
    }

    // ---------------------------------------------------------------------------
    // Region / magic helpers
    // ---------------------------------------------------------------------------

    static uint32_t GetEncodedMagic()
    {
        switch (GW::Map::GetRegion()) {
            case GW::Constants::ServerRegion::Korea:
            case GW::Constants::ServerRegion::China:
            case GW::Constants::ServerRegion::Japan:
                return kMagicAsian;
            default:
                return kMagicWestern;
        }
    }

    static uint32_t DetectMagic(const EncodedTeamBuild& encoded)
    {
        if (encoded.empty()) return kMagicWestern;
        const wchar_t wc = encoded[0];
        for (uint32_t magic : {kMagicWestern, kMagicAsian}) {
            const auto* ranges = magic == kMagicAsian ? kRangesAsian : kRangesWestern;
            const int count = magic == kMagicAsian ? static_cast<int>(std::size(kRangesAsian)) : static_cast<int>(std::size(kRangesWestern));
            const int val = WcharToValT(wc, ranges, count);
            if (val >= 0 && (static_cast<uint32_t>(val) & 0xF) == magic) return magic;
        }
        return kMagicWestern;
    }

    static EncodedTeamBuild BytesToEncoded(const std::vector<uint8_t>& bytes, uint32_t magic)
    {
        if (magic == kMagicAsian) return BytesToEncodedT(bytes, kRangesAsian, static_cast<int>(std::size(kRangesAsian)), 9);
        return BytesToEncodedT(bytes, kRangesWestern, static_cast<int>(std::size(kRangesWestern)), 9);
    }

    static std::vector<uint8_t> EncodedToBytes(const EncodedTeamBuild& encoded, uint32_t magic)
    {
        if (magic == kMagicAsian) return EncodedToBytesT(encoded, kRangesAsian, static_cast<int>(std::size(kRangesAsian)), 9);
        return EncodedToBytesT(encoded, kRangesWestern, static_cast<int>(std::size(kRangesWestern)), 9);
    }

    // ---------------------------------------------------------------------------
    // DaybreakTeamBuild base64 encoding
    // GW's native base64 alphabet, LSB-first packed bits
    // ---------------------------------------------------------------------------

    constexpr char kDaybreakAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    constexpr uint8_t kDaybreakMagicByte = 0x1Fu; // header=15, type=1 packed LSB-first

    static DaybreakTeamBuild BytesToDaybreak(const std::vector<uint8_t>& bytes)
    {
        DaybreakTeamBuild result;
        const int total_bits = static_cast<int>(bytes.size()) * 8;
        for (int bit = 0; bit < total_bits; bit += 6) {
            uint32_t val = 0;
            for (int i = 0; i < 6 && (bit + i) < total_bits; i++) {
                if ((bytes[(bit + i) / 8] >> ((bit + i) % 8)) & 1u) val |= 1u << i;
            }
            result += kDaybreakAlpha[val & 0x3Fu];
        }
        return result;
    }

    static std::vector<uint8_t> DaybreakToBytes(const DaybreakTeamBuild& daybreak)
    {
        std::vector<uint8_t> result;
        int bit = 0;
        for (const char c : daybreak) {
            const char* p = strchr(kDaybreakAlpha, c);
            if (!p) break;
            const uint32_t val = static_cast<uint32_t>(p - kDaybreakAlpha);
            for (int i = 0; i < 6; i++) {
                if (bit / 8 >= static_cast<int>(result.size())) result.push_back(0);
                if ((val >> i) & 1u) result[bit / 8] |= 1u << (bit % 8);
                bit++;
            }
        }
        return result;
    }

    // ---------------------------------------------------------------------------
    // Attribute tables
    // ---------------------------------------------------------------------------

    using namespace GW::Constants;

    // Ordered by frequency of use within each profession — most commonly invested
    // attributes first. This minimises the attribute bitmask cost and keeps
    // the most-used skill indices low for the variable-width skill flag.
    const std::array<Attribute, 5> kProfessionAttributes[11] = {
        // 0: None
        {Attribute::None, Attribute::None, Attribute::None, Attribute::None, Attribute::None},
        // 1: Warrior
        {Attribute::Strength, Attribute::Tactics, Attribute::Swordsmanship, Attribute::AxeMastery, Attribute::HammerMastery},
        // 2: Ranger
        {Attribute::Expertise, Attribute::Marksmanship, Attribute::BeastMastery, Attribute::WildernessSurvival, Attribute::None},
        // 3: Monk
        {Attribute::HealingPrayers, Attribute::DivineFavor, Attribute::ProtectionPrayers, Attribute::SmitingPrayers, Attribute::None},
        // 4: Necromancer
        {Attribute::Curses, Attribute::DeathMagic, Attribute::SoulReaping, Attribute::BloodMagic, Attribute::None},
        // 5: Mesmer
        {Attribute::DominationMagic, Attribute::FastCasting, Attribute::IllusionMagic, Attribute::InspirationMagic, Attribute::None},
        // 6: Elementalist
        {Attribute::FireMagic, Attribute::EnergyStorage, Attribute::EarthMagic, Attribute::WaterMagic, Attribute::AirMagic},
        // 7: Assassin
        {Attribute::ShadowArts, Attribute::CriticalStrikes, Attribute::DaggerMastery, Attribute::DeadlyArts, Attribute::None},
        // 8: Ritualist
        {Attribute::RestorationMagic, Attribute::ChannelingMagic, Attribute::Communing, Attribute::SpawningPower, Attribute::None},
        // 9: Paragon
        {Attribute::Command, Attribute::Leadership, Attribute::Motivation, Attribute::SpearMastery, Attribute::None},
        // 10: Dervish
        {Attribute::EarthPrayers, Attribute::ScytheMastery, Attribute::Mysticism, Attribute::WindPrayers, Attribute::None},
    };


    // Sort attributes by investment value descending, so the most-invested
    // attributes' skills get the lowest indices.
    static void SortAttrsByInvestment(Attribute attrs[], int count, const GW::SkillbarMgr::SkillTemplate& tmpl)
    {
        // Build investment lookup
        uint32_t investment[11] = {};
        for (uint32_t i = 0; i < tmpl.attributes_count; i++)
            investment[static_cast<int>(tmpl.attribute_ids[i])] = tmpl.attribute_values[i];

        // Sort attrs by investment descending, keeping unset attrs at end
        std::stable_sort(attrs, attrs + count, [&](Attribute a, Attribute b) {
            return investment[static_cast<int>(a)] > investment[static_cast<int>(b)];
        });
    }


    // ---------------------------------------------------------------------------
    // Skill list
    // ---------------------------------------------------------------------------

    // Skill ordering is tuned to minimise the highest accessible-list index
    // actually used by a build, maximising the chance the 8-bit skill flag fires.
    //
    // Heroes (is_hero=true):  primary → secondary → (PvE omitted, heroes can't use them)
    // Players (is_hero=false): primary hot attrs → PvE → secondary hot attrs →
    //                          primary cold attrs → secondary cold attrs → no-attr skills
    //
    // "Hot" = first 3 attributes in kProfessionAttributes order (most commonly invested).
    // "Cold" = remaining attributes.
    static int GetProfAttributes(Profession prof, Attribute out[], int max_count)
    {
        int count = 0;
        if (prof == Profession::None) return 0;
        for (const auto attr : kProfessionAttributes[static_cast<int>(prof)]) {
            if (attr == Attribute::None || count >= max_count) break;
            out[count++] = attr;
        }
        return count;
    }


    std::vector<SkillID> GetAccessibleSkills(Profession primary, Profession secondary, bool is_hero, const GW::SkillbarMgr::SkillTemplate* tmpl = nullptr)
    {
        Attribute pri_attrs[5], sec_attrs[5];
        int pri_count = GetProfAttributes(primary, pri_attrs, 5);
        int sec_count = GetProfAttributes(secondary, sec_attrs, 5);

        // If we have actual attribute investments, reorder so most-invested come first
        if (tmpl) {
            SortAttrsByInvestment(pri_attrs, pri_count, *tmpl);
            SortAttrsByInvestment(sec_attrs, sec_count, *tmpl);
        }

        // 7 buckets:
        //   0: primary hot attrs (indices 0-2)
        //   1: primary cold attrs (indices 3+)
        //   2: secondary hot attrs (indices 0-2)
        //   3: secondary cold attrs (indices 3+)
        //   4: no-attribute primary skills
        //   5: no-attribute secondary skills
        //   6: PvE skills (players only)
        std::vector<SkillID> buckets[7];

        auto attr_bucket = [&](Attribute attr, bool is_primary) -> int {
            const Attribute* attrs = is_primary ? pri_attrs : sec_attrs;
            const int count = is_primary ? pri_count : sec_count;
            for (int i = 0; i < count; i++) {
                if (attrs[i] == attr) return (is_primary ? 0 : 2) + (i >= 3 ? 1 : 0);
            }
            return -1;
        };

        for (size_t i = 1; i < static_cast<size_t>(SkillID::Count); i++) {
            const auto skill_id = static_cast<SkillID>(i);
            const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill || !skill->IsPlayable() || skill->IsPvP()) continue;

            if (skill->IsPvE() && skill->title < (uint16_t)GW::Constants::TitleID::Codex) {
                if (!is_hero) buckets[6].push_back(skill_id);
                continue;
            }
            if (!skill->IsPvE() && !IsSkillEquippable(skill_id)) continue;

            const auto prof = static_cast<Profession>(skill->profession);
            const auto attr = static_cast<Attribute>(skill->attribute);

            if (prof == primary) {
                const int b = attr_bucket(attr, true);
                buckets[b >= 0 ? b : 4].push_back(skill_id);
            }
            else if (prof == secondary) {
                const int b = attr_bucket(attr, false);
                buckets[b >= 0 ? b : 5].push_back(skill_id);
            }
        }

        std::vector<SkillID> result;
        if (is_hero) {
            // primary hot → primary cold → secondary hot → secondary cold → no-attr
            for (int b : {0, 1, 2, 3, 4, 5})
                result.insert(result.end(), buckets[b].begin(), buckets[b].end());
        }
        else {
            // primary hot → PvE → secondary hot → primary cold → secondary cold → no-attr
            for (int b : {0, 6, 2, 1, 3, 4, 5})
                result.insert(result.end(), buckets[b].begin(), buckets[b].end());
        }
        return result;
    }

    // ---------------------------------------------------------------------------
    // Shared hero write/read for EncodedTeamBuild
    //
    // Attribute encoding:
    //   - 4-bit slot_count (number of attribute slots for this profession pair)
    //   - slot_count-bit presence mask (1 bit per slot in kProfessionAttributes order)
    //   - Fixed 4-bit value for each set bit only (no total_points header)
    //
    // Skill encoding:
    //   - 1-bit width flag: 0 = indices fit in 8 bits, 1 = need 9 bits
    //   - 8 skill indices at the determined width
    //   - PvE skills are at the end of the hero accessible list (heroes never equip
    //     them), keeping hero build skill indices small.
    // ---------------------------------------------------------------------------

    static int BuildAttrSlots(Profession primary, Profession secondary, Attribute out_attrs[], int max_slots)
    {
        int count = 0;
        for (int prof_idx = 0; prof_idx < 2 && count < max_slots; prof_idx++) {
            const auto prof = prof_idx == 0 ? primary : secondary;
            if (prof == Profession::None) continue;
            for (const auto attr : kProfessionAttributes[static_cast<int>(prof)]) {
                if (attr == Attribute::None || count >= max_slots) break;
                out_attrs[count++] = attr;
            }
        }
        return count;
    }

    static void WriteHero(BitWriter& bw, const GW::SkillbarMgr::SkillTemplate& tmpl, GW::Constants::HeroID hero_id)
    {
        const auto primary = static_cast<Profession>(tmpl.primary);
        const auto secondary = static_cast<Profession>(tmpl.secondary);
        const bool is_hero = hero_id != GW::Constants::HeroID::NoHero;

        if (primary == GW::Constants::Profession::None) 
            return;

        bw.write(static_cast<uint32_t>(hero_id), 6);
        bw.write(static_cast<uint32_t>(primary), 4);
        bw.write(static_cast<uint32_t>(secondary), 4);

        // --- Attributes ---
        Attribute slots[10];
        const int slot_count = BuildAttrSlots(primary, secondary, slots, 10);

        uint32_t mask = 0;
        uint32_t values[10];
        int value_count = 0;
        for (int s = 0; s < slot_count; s++) {
            uint32_t val = 0;
            for (uint32_t i = 0; i < tmpl.attributes_count; i++) {
                if (tmpl.attribute_ids[i] == slots[s]) {
                    val = tmpl.attribute_values[i];
                    break;
                }
            }
            if (val > 0) {
                mask |= (1u << s);
                values[value_count++] = val;
            }
        }

        bw.write(static_cast<uint32_t>(slot_count), 4);
        bw.write(mask, slot_count);
        for (int i = 0; i < value_count; i++)
            bw.write(values[i], 4);

        // --- Skills ---
        const auto accessible = GetAccessibleSkills(primary, secondary, is_hero);

        uint32_t max_idx = 0;
        for (int i = 0; i < 8; i++) {
            const auto skill_id = tmpl.skills[i];
            if (skill_id == SkillID::No_Skill) continue;
            const auto it = std::find(accessible.begin(), accessible.end(), skill_id);
            if (it != accessible.end()) max_idx = std::max(max_idx, static_cast<uint32_t>(it - accessible.begin()) + 1);
        }

        const uint32_t wide = max_idx > 255 ? 1u : 0u;
        const int skill_bits = wide ? 9 : 8;
        bw.write(wide, 1);

        for (int i = 0; i < 8; i++) {
            const auto skill_id = tmpl.skills[i];
            if (skill_id == SkillID::No_Skill) {
                bw.write(0, skill_bits);
                continue;
            }
            const auto it = std::find(accessible.begin(), accessible.end(), skill_id);
            bw.write(it != accessible.end() ? static_cast<uint32_t>(it - accessible.begin()) + 1 : 0u, skill_bits);
        }
    }

    static bool ReadHero(BitReader& br, GW::SkillbarMgr::SkillTemplate& tmpl, GW::Constants::HeroID& hero_id)
    {
        hero_id = static_cast<GW::Constants::HeroID>(br.read(6));
        tmpl.primary = static_cast<Profession>(br.read(4));
        tmpl.secondary = static_cast<Profession>(br.read(4));

        const auto primary = tmpl.primary;
        if (primary == GW::Constants::Profession::None) 
            return false;
        const auto secondary = tmpl.secondary;
        const bool is_hero = hero_id != GW::Constants::HeroID::NoHero;

        // --- Attributes ---
        const int slot_count = static_cast<int>(br.read(4));
        Attribute slots[10];
        {
            Attribute expected[10];
            if (slot_count != BuildAttrSlots(primary, secondary, expected, 10)) {
                br.ok = false;
                return false;
            }
            for (int i = 0; i < slot_count; i++)
                slots[i] = expected[i];
        }

        const uint32_t mask = br.read(slot_count);
        uint32_t attr_idx = 0;
        for (int s = 0; s < slot_count; s++) {
            if (!(mask & (1u << s))) continue;
            const uint32_t val = br.read(4);
            if (val > 0 && attr_idx < 12) {
                tmpl.attribute_ids[attr_idx] = slots[s];
                tmpl.attribute_values[attr_idx] = val;
                attr_idx++;
            }
        }
        tmpl.attributes_count = attr_idx;

        // --- Skills ---
        const auto accessible = GetAccessibleSkills(primary, secondary, is_hero);
        const int skill_bits = br.read(1) ? 9 : 8;
        for (int i = 0; i < 8; i++) {
            const uint32_t idx = br.read(skill_bits);
            tmpl.skills[i] = (idx == 0 || idx > accessible.size()) ? SkillID::No_Skill : accessible[idx - 1];
        }

        return br.ok;
    }

    // ---------------------------------------------------------------------------
    // EncodedTeamBuild
    // ---------------------------------------------------------------------------

    EncodedTeamBuild TeamBuildToEncoded(const TeamHeroBuild& tbuild)
    {
        const uint32_t magic = GetEncodedMagic();

        BitWriter bw;
        bw.write(magic, 4);
        auto build_count_pos = bw.pos;
        bw.write(0, 4);

        uint8_t written = 0;
        for (const auto& build : tbuild.builds) {
            GW::SkillbarMgr::SkillTemplate tmpl{};
            if (build.code.empty() || !GW::SkillbarMgr::DecodeSkillTemplate(tmpl, build.code.c_str())) 
                continue;
            WriteHero(bw, tmpl, build.hero_id);
            written++;
        }
        auto prev_pos = bw.pos;
        bw.pos = build_count_pos;
        bw.write(written, 4);
        bw.pos = prev_pos;

        return BytesToEncoded(bw.buf, magic);
    }

    bool EncodedToTeamBuild(const EncodedTeamBuild& encoded, TeamHeroBuild& out)
    {
        const uint32_t magic = DetectMagic(encoded);

        const auto bytes = EncodedToBytes(encoded, magic);
        if (bytes.empty()) 
            return false;

        BitReader br(bytes.data(), static_cast<int>(bytes.size()));
        if (br.read(4) != magic) 
            return false;

        const uint32_t count = br.read(4);
        out.builds.clear();

        for (uint32_t h = 0; h < count; h++) {
            GW::SkillbarMgr::SkillTemplate tmpl{};
            GW::Constants::HeroID hero_id{};
            if (!ReadHero(br, tmpl, hero_id)) 
                return false;

            Build build{};
            build.hero_id = hero_id;
            char code_buf[64]{};
            if (!GW::SkillbarMgr::EncodeSkillTemplate(tmpl, code_buf, _countof(code_buf))) 
                return false;
            build.code = code_buf;
            build.name = build.GetFallbackBuildName();
            out.builds.push_back(std::move(build));
        }
        if (br.ok) out.ui_id = TextUtils::WStringToString(encoded);
        out.has_hero_slots = std::ranges::any_of(out.builds, [](const Build& b) {
            return b.hero_id != GW::Constants::HeroID::NoHero;
        });
        return br.ok;
    }

    bool IsEncodedTeamBuild(const EncodedTeamBuild& encoded)
    {
        if (encoded.empty()) return false;
        const wchar_t wc = encoded[0];
        for (uint32_t magic : {kMagicWestern, kMagicAsian}) {
            const auto* ranges = magic == kMagicAsian ? kRangesAsian : kRangesWestern;
            const int count = magic == kMagicAsian ? static_cast<int>(std::size(kRangesAsian)) : static_cast<int>(std::size(kRangesWestern));
            const int val = WcharToValT(wc, ranges, count);
            if (val >= 0 && (static_cast<uint32_t>(val) & 0xF) == magic) return true;
        }
        return false;
    }

    // ---------------------------------------------------------------------------
    // DaybreakTeamBuild
    // ---------------------------------------------------------------------------

    DaybreakTeamBuild TeamBuildToDaybreak(const TeamHeroBuild& tbuild)
    {
        BitWriter bw;

        // Party loadout header: header=15 (4 bits) + type=1 (4 bits) = first byte 0x1F
        bw.write(15u, 4);
        bw.write(1u, 4);
        bw.write(static_cast<uint32_t>(tbuild.builds.size()), 4);

        for (const auto& build : tbuild.builds) {
            GW::SkillbarMgr::SkillTemplate tmpl{};
            if (!GW::SkillbarMgr::DecodeSkillTemplate(tmpl, build.code.c_str())) return {};

            bw.write(static_cast<uint32_t>(build.hero_id), 8);

            const auto max_prof = std::max(static_cast<uint32_t>(tmpl.primary), static_cast<uint32_t>(tmpl.secondary));
            const uint32_t prof_code = max_prof > 15 ? 1u : 0u;
            const int p = static_cast<int>(prof_code) * 2 + 4;
            bw.write(prof_code, 2);
            bw.write(static_cast<uint32_t>(tmpl.primary), p);
            bw.write(static_cast<uint32_t>(tmpl.secondary), p);
            bw.write(tmpl.attributes_count, 4);

            uint32_t attr_code = 0;
            for (uint32_t i = 0; i < tmpl.attributes_count && i < 12; i++) {
                if (static_cast<uint32_t>(tmpl.attribute_ids[i]) > 15) {
                    attr_code = 2;
                    break;
                }
            }
            const int a = static_cast<int>(attr_code) + 4;
            bw.write(attr_code, 4);
            for (uint32_t i = 0; i < tmpl.attributes_count && i < 12; i++) {
                bw.write(static_cast<uint32_t>(tmpl.attribute_ids[i]), a);
                bw.write(tmpl.attribute_values[i], 4);
            }

            uint32_t skill_code = 0;
            for (int i = 0; i < 8; i++) {
                if (static_cast<uint32_t>(tmpl.skills[i]) > 255) {
                    skill_code = 1;
                    break;
                }
            }
            const int s = static_cast<int>(skill_code) + 8;
            bw.write(skill_code, 4);
            for (int i = 0; i < 8; i++)
                bw.write(static_cast<uint32_t>(tmpl.skills[i]), s);
        }

        return BytesToDaybreak(bw.buf);
    }

    bool DaybreakToTeamBuild(const DaybreakTeamBuild& daybreak, TeamHeroBuild& out)
    {
        const auto bytes = DaybreakToBytes(daybreak);
        if (bytes.empty()) return false;

        BitReader br(bytes.data(), static_cast<int>(bytes.size()));
        if (br.read(4) != 15u) return false; // header
        if (br.read(4) != 1u) return false;  // type = party loadout

        const uint32_t count = br.read(4);
        out.builds.clear();

        for (uint32_t h = 0; h < count; h++) {
            GW::SkillbarMgr::SkillTemplate tmpl{};

            const uint32_t hero_id = br.read(8);
            const uint32_t prof_code = br.read(2);
            const int p = static_cast<int>(prof_code) * 2 + 4;
            tmpl.primary = static_cast<Profession>(br.read(p));
            tmpl.secondary = static_cast<Profession>(br.read(p));
            tmpl.attributes_count = br.read(4);

            const uint32_t attr_code = br.read(4);
            const int a = static_cast<int>(attr_code) + 4;
            for (uint32_t i = 0; i < tmpl.attributes_count && i < 12; i++) {
                tmpl.attribute_ids[i] = static_cast<Attribute>(br.read(a));
                tmpl.attribute_values[i] = br.read(4);
            }

            const uint32_t skill_code = br.read(4);
            const int s = static_cast<int>(skill_code) + 8;
            for (int i = 0; i < 8; i++)
                tmpl.skills[i] = static_cast<SkillID>(br.read(s));

            if (!br.ok) return false;

            Build build{};
            build.hero_id = static_cast<GW::Constants::HeroID>(hero_id);
            char code_buf[64]{};
            if (!GW::SkillbarMgr::EncodeSkillTemplate(tmpl, code_buf, _countof(code_buf))) return false;
            build.code = code_buf;
            out.builds.push_back(std::move(build));
        }
        return br.ok;
    }

    bool IsDaybreakTeamBuild(const DaybreakTeamBuild& daybreak)
    {
        if (daybreak.empty()) return false;
        const auto bytes = DaybreakToBytes(daybreak.substr(0, 6));
        return !bytes.empty() && bytes[0] == kDaybreakMagicByte;
    }

    // ---------------------------------------------------------------------------
    // Chat helpers
    // ---------------------------------------------------------------------------

    std::wstring ToChatMessage(const TeamHeroBuild& tbuild)
    {
        const auto encoded = TeamBuildToEncoded(tbuild);
        if (encoded.empty()) return {};

        // [label;encoded] must fit within 120 wchars
        // 3 = '[' + ';' + ']'
        constexpr size_t kMaxLen = 119;
        constexpr size_t kOverhead = 3;
        const size_t max_label = kMaxLen > encoded.size() + kOverhead ? kMaxLen - encoded.size() - kOverhead : 0;

        if (max_label == 0) return {}; // encoded alone is too long

        std::wstring name = L"Teambuild";
        if (name.size() > max_label) name.resize(max_label);

        return std::format(L"[{};{}]", name, encoded);
    }

} // namespace TeamBuildEncoder
