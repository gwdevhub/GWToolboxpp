#include "TeamBuildEncoder.h"

#include <algorithm>
#include <cstring>
#include <format>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "TextUtils.h"
#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>

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
    // 9 bits per wchar across 3 confirmed GW-safe Unicode ranges:
    //   val 0x000-0x05F → U+0020-U+007F  (96 values,  Basic Latin)
    //   val 0x060-0x0DE → U+00A1-U+017F  (223 values, Latin-1 Supplement + Extended-A)
    //   val 0x0DF-0x1FF → U+0391-U+045F  (193 values, Greek + Cyrillic)
    // Total: 512 values = exactly 2^9
    // ---------------------------------------------------------------------------

    // 9 bits per wchar across 6 GW-safe Unicode sub-ranges (512 values exactly).
    // Excludes [=U+005B, ]=U+005D, ;=U+003B to allow safe use inside [label;encoded] chat links.
    constexpr struct {
        uint16_t base, size;
    } kRanges[] = {
        {0x0020, 27},  // U+0020-U+003A  (space .. colon)
        {0x003C, 31},  // U+003C-U+005A  (< = > ? @ A-Z)
        {0x005C, 1},   // U+005C         (backslash)
        {0x005E, 33},  // U+005E-U+007E  (^ _ ` a-z { | } ~)
        {0x00A1, 223}, // U+00A1-U+017F  (Latin-1 Supplement + Extended-A)
        {0x0391, 197}, // U+0391-U+0455  (Greek + Cyrillic)
    };
    constexpr uint32_t kEncodedMagic = 0xF;

    static wchar_t ValToWchar(uint16_t val)
    {
        for (const auto& r : kRanges) {
            if (val < r.size) return static_cast<wchar_t>(r.base + val);
            val -= r.size;
        }
        return L'?';
    }

    static int WcharToVal(wchar_t wc)
    {
        uint16_t offset = 0;
        for (const auto& r : kRanges) {
            if (wc >= r.base && wc < r.base + r.size) return offset + (wc - r.base);
            offset += r.size;
        }
        return -1;
    }

    static EncodedTeamBuild BytesToEncoded(const std::vector<uint8_t>& bytes)
    {
        EncodedTeamBuild result;
        int bit = 0;
        const int total_bits = static_cast<int>(bytes.size()) * 8;
        while (bit < total_bits) {
            uint16_t val = 0;
            for (int i = 0; i < 9 && bit < total_bits; i++, bit++) {
                if ((bytes[bit / 8] >> (bit % 8)) & 1u) val |= static_cast<uint16_t>(1u << i);
            }
            result += ValToWchar(val);
        }
        return result;
    }

    static std::vector<uint8_t> EncodedToBytes(const EncodedTeamBuild& encoded)
    {
        std::vector<uint8_t> bytes;
        int bit = 0;
        for (const wchar_t wc : encoded) {
            const int val = WcharToVal(wc);
            if (val < 0) return {};
            for (int i = 0; i < 9; i++, bit++) {
                if (bit / 8 >= static_cast<int>(bytes.size())) bytes.push_back(0);
                if ((val >> i) & 1u) bytes[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
            }
        }
        return bytes;
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



    const std::array<Attribute, 5> kProfessionAttributes[11] = {
        // 0: None
        {Attribute::None, Attribute::None, Attribute::None, Attribute::None, Attribute::None},
        // 1: Warrior
        {Attribute::Strength, Attribute::AxeMastery, Attribute::HammerMastery, Attribute::Swordsmanship, Attribute::Tactics},
        // 2: Ranger
        {Attribute::BeastMastery, Attribute::Expertise, Attribute::Marksmanship, Attribute::WildernessSurvival, Attribute::None},
        // 3: Monk
        {Attribute::DivineFavor, Attribute::HealingPrayers, Attribute::ProtectionPrayers, Attribute::SmitingPrayers, Attribute::None},
        // 4: Necromancer
        {Attribute::BloodMagic, Attribute::Curses, Attribute::DeathMagic, Attribute::SoulReaping, Attribute::None},
        // 5: Mesmer
        {Attribute::DominationMagic, Attribute::FastCasting, Attribute::IllusionMagic, Attribute::InspirationMagic, Attribute::None},
        // 6: Elementalist
        {Attribute::AirMagic, Attribute::EarthMagic, Attribute::EnergyStorage, Attribute::FireMagic, Attribute::WaterMagic},
        // 7: Assassin
        {Attribute::CriticalStrikes, Attribute::DaggerMastery, Attribute::DeadlyArts, Attribute::ShadowArts, Attribute::None},
        // 8: Ritualist
        {Attribute::ChannelingMagic, Attribute::Communing, Attribute::RestorationMagic, Attribute::SpawningPower, Attribute::None},
        // 9: Paragon
        {Attribute::Command, Attribute::Leadership, Attribute::Motivation, Attribute::SpearMastery, Attribute::None},
        // 10: Dervish
        {Attribute::EarthPrayers, Attribute::Mysticism, Attribute::ScytheMastery, Attribute::WindPrayers, Attribute::None},
    };

    int BitsForAttrValue(int remaining_points)
    {
        if (remaining_points >= 28) return 4;
        if (remaining_points >= 6) return 3;
        if (remaining_points >= 1) return 2;
        return 0;
    }

    // ---------------------------------------------------------------------------
    // Skill list
    // ---------------------------------------------------------------------------

    std::vector<SkillID> GetAccessibleSkills(Profession primary, Profession secondary)
    {
        std::vector<SkillID> skills;
        for (size_t i = 1; i < static_cast<size_t>(SkillID::Count); i++) {
            const auto skill_id = static_cast<SkillID>(i);
            const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill || !skill->IsPlayable() || skill->IsPvP()) continue;
            if (skill->IsPvE() && skill->title < (uint16_t)GW::Constants::TitleID::Codex) {
                // PvE title skills
                skills.push_back(skill_id);
                continue;
            }
            if (!skill->IsPvE() && !IsSkillEquippable(skill_id))
                continue;
            const auto prof = static_cast<Profession>(skill->profession);
            if (prof == primary || prof == secondary) 
                skills.push_back(skill_id);
        }
        // NB: Max skills per profession atm is 389!
        return skills;
    }

    // ---------------------------------------------------------------------------
    // Shared hero write/read for EncodedTeamBuild
    // ---------------------------------------------------------------------------

    static void WriteHero(BitWriter& bw, const GW::SkillbarMgr::SkillTemplate& tmpl, GW::Constants::HeroID hero_id)
    {
        const auto primary = static_cast<Profession>(tmpl.primary);
        const auto secondary = static_cast<Profession>(tmpl.secondary);

        bw.write(static_cast<uint32_t>(hero_id), 6);
        bw.write(static_cast<uint32_t>(primary), 4);
        bw.write(static_cast<uint32_t>(secondary), 4);

        int total_points = 0;
        for (uint32_t i = 0; i < tmpl.attributes_count; i++)
            total_points += kAttrCost[tmpl.attribute_values[i]];

        bw.write(static_cast<uint32_t>(total_points), 8);
        int remaining = total_points;
        for (int prof_idx = 0; prof_idx < 2; prof_idx++) {
            const auto prof = prof_idx == 0 ? primary : secondary;
            if (prof == Profession::None) continue;
            for (const auto attr : kProfessionAttributes[static_cast<int>(prof)]) {
                if (attr == Attribute::None) break;
                const int bits = BitsForAttrValue(remaining);
                if (bits == 0) break;
                uint32_t val = 0;
                for (uint32_t i = 0; i < tmpl.attributes_count; i++) {
                    if (tmpl.attribute_ids[i] == attr) {
                        val = tmpl.attribute_values[i];
                        break;
                    }
                }
                bw.write(val, bits);
                remaining -= kAttrCost[val];
            }
        }

        const auto accessible = GetAccessibleSkills(primary, secondary);
        for (int i = 0; i < 8; i++) {
            const auto skill_id = tmpl.skills[i];
            if (skill_id == SkillID::No_Skill) {
                bw.write(0, 9);
                continue;
            }
            const auto it = std::find(accessible.begin(), accessible.end(), skill_id);
            bw.write(it != accessible.end() ? static_cast<uint32_t>(it - accessible.begin()) + 1 : 0u, 9);
        }
    }

    static bool ReadHero(BitReader& br, GW::SkillbarMgr::SkillTemplate& tmpl, GW::Constants::HeroID& hero_id)
    {
        hero_id = static_cast<GW::Constants::HeroID>(br.read(6));
        tmpl.primary = static_cast<Profession>(br.read(4));
        tmpl.secondary = static_cast<Profession>(br.read(4));

        const auto primary = tmpl.primary;
        const auto secondary = tmpl.secondary;

        const int total_points = static_cast<int>(br.read(8));
        int remaining = total_points;
        uint32_t attr_idx = 0;
        for (int prof_idx = 0; prof_idx < 2; prof_idx++) {
            const auto prof = prof_idx == 0 ? primary : secondary;
            if (prof == Profession::None) continue;
            for (const auto attr : kProfessionAttributes[static_cast<int>(prof)]) {
                if (attr == Attribute::None) break;
                const int bits = BitsForAttrValue(remaining);
                if (bits == 0) break;
                const uint32_t val = br.read(bits);
                if (val > 0) {
                    tmpl.attribute_ids[attr_idx] = attr;
                    tmpl.attribute_values[attr_idx] = val;
                    attr_idx++;
                    remaining -= kAttrCost[val];
                }
            }
        }
        tmpl.attributes_count = attr_idx;

        const auto accessible = GetAccessibleSkills(primary, secondary);
        for (int i = 0; i < 8; i++) {
            const uint32_t idx = br.read(9);
            tmpl.skills[i] = (idx == 0 || idx > accessible.size()) ? SkillID::No_Skill : accessible[idx - 1];
        }
        return br.ok;
    }

    // ---------------------------------------------------------------------------
    // EncodedTeamBuild
    // ---------------------------------------------------------------------------

    EncodedTeamBuild TeamBuildToEncoded(const TeamHeroBuild& tbuild)
    {
        BitWriter bw;
        bw.write(kEncodedMagic, 4);
        bw.write(static_cast<uint32_t>(tbuild.builds.size()), 4);

        for (const auto& build : tbuild.builds) {
            GW::SkillbarMgr::SkillTemplate tmpl{};
            if (!build.code.empty() &&  !GW::SkillbarMgr::DecodeSkillTemplate(tmpl, build.code.c_str())) 
                tmpl = {};
            WriteHero(bw, tmpl, build.hero_id);
        }

        return BytesToEncoded(bw.buf);
    }

    bool EncodedToTeamBuild(const EncodedTeamBuild& encoded, TeamHeroBuild& out)
    {
        const auto bytes = EncodedToBytes(encoded);
        if (bytes.empty()) return false;

        BitReader br(bytes.data(), static_cast<int>(bytes.size()));
        if (br.read(4) != kEncodedMagic) return false;

        const uint32_t count = br.read(4);
        out.builds.clear();

        for (uint32_t h = 0; h < count; h++) {
            GW::SkillbarMgr::SkillTemplate tmpl{};
            GW::Constants::HeroID hero_id{};
            if (!ReadHero(br, tmpl, hero_id)) return false;

            Build build{};
            build.hero_id = hero_id;
            char code_buf[64]{};
            if (!GW::SkillbarMgr::EncodeSkillTemplate(tmpl, code_buf, _countof(code_buf))) return false;
            build.code = code_buf;
            out.builds.push_back(std::move(build));
        }
        if (br.ok) out.ui_id = TextUtils::WStringToString(encoded);
        return br.ok;
    }

    bool IsEncodedTeamBuild(const EncodedTeamBuild& encoded)
    {
        if (encoded.empty()) return false;
        const int val = WcharToVal(encoded[0]);
        return val >= 0 && (static_cast<uint32_t>(val) & 0xF) == kEncodedMagic;
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
        // Decode first 6 chars (36 bits) to get first byte and check magic
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
