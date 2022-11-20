#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/Resources.h>
#include <Modules/HallOfMonumentsModule.h>
#include <CurlWrapper.h>


namespace {
    constexpr char _Base64ToValue[128] = {
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // [0,   16)
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // [16,  32)
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, // [32,  48)
           52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, // [48,  64)
           -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, // [64,  80)
           15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, // [80,  96)
           -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // [96,  112)
           41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // [112, 128)
    };
    const unsigned char _Base64Table[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int _WriteBits(int val, char* buff, int count = 6) {
        for (int i = 0; i < count; i++) {
            buff[i] = ((val >> i) & 1);
        }
        return count;
    }

    int _ReadBits(char** str, int n) {
        int val = 0;
        char* s = *str;
        for (int i = 0; i < n; i++)
            val |= (*s++ << i);
        *str = s;
        return val;
    }

    int Base64ToBitString(const char* in, char* out, int out_len) {
        int in_len = (int)strlen(in);
        ASSERT((in_len * 6) < out_len);

        int bitStrLen = 0;
        for (int i = 0; i < in_len; i++) {
            int numeric_value = _Base64ToValue[in[i]];
            if (numeric_value == -1) {
                Log::Error("Unvalid base64 character '%c' in string '%s'\n", in[i], in);
                return -1;
            }
            bitStrLen += _WriteBits(numeric_value, &out[bitStrLen], 6);
        }
        return bitStrLen;
    }

}
bool HallOfMonumentsModule::DecodeHomCode(const char* in, HallOfMonumentsAchievements* out) {
    ASSERT(strlen(in) < _countof(out->hom_code));
    strcpy(out->hom_code, in);
    return DecodeHomCode(out);

}
bool HallOfMonumentsModule::DecodeHomCode(HallOfMonumentsAchievements* out) {
    const int bufSize = 1024;
    char bitStr[bufSize]; // @Cleanup: Confirm that the buffer is alway big enough.
    const int bitStrLen = Base64ToBitString(out->hom_code, bitStr, bufSize);
    if (bitStrLen == -1) {
        Log::Error("DecodeHomCode: Failed to Base64ToBitString");
        return false;
    }
    // Resilience
    char* it = bitStr;
    out->resilience_tally = 0;
    memset(out->resilience_points, 0, sizeof(out->resilience_points));
    for (size_t i = 0; i < _countof(out->resilience_detail); i++) {
        out->resilience_detail[i] = (bool)_ReadBits(&it, 1);
        if (!out->resilience_detail[i])
            continue;
        out->resilience_tally++;
        out->resilience_points[(size_t)ResiliencePoints::AnyArmorStatue] = 1;
        if (out->resilience_tally == 3)
            out->resilience_points[(size_t)ResiliencePoints::ThreeArmorStatues] = 1;
        if (out->resilience_tally == 5)
            out->resilience_points[(size_t)ResiliencePoints::FiveArmorStatues] = 2;
        if (out->resilience_tally == 7)
            out->resilience_points[(size_t)ResiliencePoints::SevenArmorStatues] = 1;

        switch ((ResilienceDetail)i) {
        case ResilienceDetail::EliteKurzickArmor:
        case ResilienceDetail::EliteLuxonArmor:
            out->resilience_points[(size_t)ResiliencePoints::KurzickOrLuxonArmorStatue] = 1;
            break;
        case ResilienceDetail::VabbianArmor:
            out->resilience_points[(size_t)ResiliencePoints::VabbianArmorStatue] = 1;
            break;
        case ResilienceDetail::ObsidianArmor:
            out->resilience_points[(size_t)ResiliencePoints::ObsidianArmorStatue] = 1;
            break;
        }
    }
    out->resilience_points_total = 0;
    for (size_t i = 0; i < _countof(out->resilience_points); i++) {
        out->resilience_points_total += out->resilience_points[i];
    }
    // Fellowship
    it = &bitStr[32];
    out->fellowship_tally = 0;
    memset(out->fellowship_points, 0, sizeof(out->fellowship_points));
    for (size_t i = 0; i < _countof(out->fellowship_detail); i++) {
        out->fellowship_detail[i] = (bool)_ReadBits(&it, 1);
        if (!out->fellowship_detail[i])
            continue;
        out->fellowship_tally++;
        if (out->fellowship_tally == 5)
            out->fellowship_points[(size_t)FellowshipPoints::FiveCompanionStatues] = 2;
        if (out->fellowship_tally == 10)
            out->fellowship_points[(size_t)FellowshipPoints::TenCompanionStatues] = 1;
        if (out->fellowship_tally == 20)
            out->fellowship_points[(size_t)FellowshipPoints::TwentyCompanionStatues] = 1;
        if (out->fellowship_tally == 30)
            out->fellowship_points[(size_t)FellowshipPoints::ThirtyCompanionStatues] = 1;
        switch ((FellowshipDetail)i) {
        case FellowshipDetail::AnimalCompanion:
            out->fellowship_points[(size_t)FellowshipPoints::AnyPetStatue] = 1;
            break;
        case FellowshipDetail::BlackMoa:
        case FellowshipDetail::BlackWidowSpider:
        case FellowshipDetail::ImperialPhoenix:
            out->fellowship_points[(size_t)FellowshipPoints::AnyPetStatue] = 1;
            out->fellowship_points[(size_t)FellowshipPoints::AnyRarePetStatue] = 1;
            break;
        default:
            out->fellowship_points[(size_t)FellowshipPoints::AnyHeroStatue] = 1;
            break;
        }
    }
    out->fellowship_points_total = 0;
    for (size_t i = 0; i < _countof(out->fellowship_points); i++) {
        out->fellowship_points_total += out->fellowship_points[i];
    }

    // Honor
    it = &bitStr[64];
    out->honor_tally = 0;
    memset(out->honor_points, 0, sizeof(out->honor_points));
    out->honor_points[(size_t)HonorPoints::AccountsLinked] = 3;
    for (size_t i = 0; i < _countof(out->honor_detail); i++) {
        out->honor_detail[i] = (bool)_ReadBits(&it, 1);
        if (!out->honor_detail[i])
            continue;
        out->honor_tally++;
        if (out->honor_tally == 1)
            out->honor_points[(size_t)HonorPoints::AnyStatue] = 2;
        if (out->honor_tally == 5)
            out->honor_points[(size_t)HonorPoints::FiveStatues] = 3;
        if (out->honor_tally == 10)
            out->honor_points[(size_t)HonorPoints::TenStatues] = 1;
        if (out->honor_tally == 15)
            out->honor_points[(size_t)HonorPoints::FifteenStatues] = 1;
        if (out->honor_tally == 20)
            out->honor_points[(size_t)HonorPoints::TwentyStatues] = 1;
        if (out->honor_tally == 25)
            out->honor_points[(size_t)HonorPoints::TwentyFiveStatues] = 1;
        if (out->honor_tally == 30)
            out->honor_points[(size_t)HonorPoints::ThirtyStatues] = 1;
        if (out->honor_tally == 35)
            out->honor_points[(size_t)HonorPoints::ThirtyFiveStatues] = 1;
        if (out->honor_tally == 40)
            out->honor_points[(size_t)HonorPoints::FourtyStatues] = 1;
        switch ((HonorDetail)i) {
        case HonorDetail::EternalChampion:
        case HonorDetail::EternalCodexDisciple:
        case HonorDetail::EternalCommander:
        case HonorDetail::EternalGladiator:
        case HonorDetail::EternalHero:
        case HonorDetail::EternalZaishenSupporter:
            out->honor_points[(size_t)HonorPoints::OnePvpStatue] = 3;
            break;
        }
    }
    out->honor_points_total = 0;
    for (size_t i = 0; i < _countof(out->honor_points); i++) {
        out->honor_points_total += out->honor_points[i];
    }

    // Valor
    it = &bitStr[128];
    out->valor_tally = 0;
    memset(out->valor_points, 0, sizeof(out->valor_points));
    for (size_t i = 0; i < _countof(out->valor_detail); i++) {
        out->valor_detail[i] = (bool)_ReadBits(&it, 1);
        if (!out->valor_detail[i])
            continue;
        out->valor_tally++;
        if (out->valor_tally == 1)
            out->valor_points[(size_t)ValorPoints::AnyWeaponStatue] = 1;
        if (out->valor_tally == 5)
            out->valor_points[(size_t)ValorPoints::FiveWeaponStatues] = 1;
        if (out->valor_tally == 11)
            out->valor_points[(size_t)ValorPoints::ElevenWeaponStatues] = 2;
        if (out->valor_tally == 15)
            out->valor_points[(size_t)ValorPoints::FifteenWeaponStatues] = 1;

        if (i >= (size_t)ValorDetail::DestroyerAxe && i <= (size_t)ValorDetail::DestroyerSword)
            out->valor_points[(size_t)ValorPoints::DestroyerWeaponStatue] = 1;
        else if (i >= (size_t)ValorDetail::TormentedAxe && i <= (size_t)ValorDetail::TormentedSword)
            out->valor_points[(size_t)ValorPoints::TormentedWeaponStatue] = 1;
        else if (i >= (size_t)ValorDetail::OppressorAxe && i <= (size_t)ValorDetail::OppressorSword)
            out->valor_points[(size_t)ValorPoints::OppressorWeaponStatue] = 1;
    }
    out->valor_points_total = 0;
    for (size_t i = 0; i < _countof(out->valor_points); i++) {
        out->valor_points_total += out->valor_points[i];
    }

    // Devotion
    it = &bitStr[192];
    for (size_t i = 0; i < _countof(out->devotion_detail); i++) {
        out->devotion_detail[i] = (uint32_t)_ReadBits(&it, 7);
        out->devotion_tally += out->devotion_detail[i];
        if (!out->devotion_detail[i])
            continue;
        out->devotion_points[(size_t)DevotionPoints::AnyMiniatureStatue] = 1;
        if (out->devotion_tally >= 20)
            out->devotion_points[(size_t)DevotionPoints::TwentyMiniatureStatues] = 2;
        if (out->devotion_tally >= 30)
            out->devotion_points[(size_t)DevotionPoints::ThirtyMiniatureStatues] = 1;
        if (out->devotion_tally >= 40)
            out->devotion_points[(size_t)DevotionPoints::FourtyMiniatureStatues] = 1;
        if (out->devotion_tally >= 50)
            out->devotion_points[(size_t)DevotionPoints::FiftyMiniatureStatues] = 1;

        switch ((DevotionDetail)i) {
        case DevotionDetail::Rare:
            out->devotion_points[(size_t)DevotionPoints::RareMiniatureStatue] = 1;
            break;
        case DevotionDetail::Unique:
            out->devotion_points[(size_t)DevotionPoints::UniqueMiniatureStatue] = 1;
            break;
        }
    }
    out->devotion_points_total = 0;
    for (size_t i = 0; i < _countof(out->devotion_points); i++) {
        out->devotion_points_total += out->devotion_points[i];
    }
    return true;
}
void HallOfMonumentsModule::AsyncGetAccountAchievements(const std::wstring& character_name, HallOfMonumentsAchievements* out, OnAchievementsLoadedCallback callback)
{
    out->state = HallOfMonumentsAchievements::State::Loading;
    std::string character_name_s = GuiUtils::WStringToString(character_name);
    for (size_t x = 0; x < character_name_s.length(); x++) {
        if (x == 0)
            character_name_s[x] = (char)toupper(character_name_s[x]);
        else if (character_name_s[x - 1] == ' ')
            character_name_s[x] = (char)toupper(character_name_s[x]);
    }
    out->character_name = character_name;

    std::string char_name_escaped;
    EscapeUrl(char_name_escaped, character_name_s.c_str());
    const auto url_str = std::format("https://hom.guildwars2.com/character/{}", char_name_escaped);

    Resources::Instance().Download(url_str, [out, callback](bool success, const std::string& response) {
        if (!success) {
            Log::Log("Failed to load account hom code %s\n%s", out->character_name.c_str(), response.c_str());
            out->state = HallOfMonumentsAchievements::State::Error;
            if (callback) callback(out);
            return;
        }
        std::regex json_regex("legacy_bits\":\"([^\"]+)");
        std::smatch m;
        if (!std::regex_search(response, m, json_regex)) {
            Log::Log("Failed to find regex code from %s", response.c_str());
            out->state = HallOfMonumentsAchievements::State::Error;
            if (callback) callback(out);
            return;
        }
        std::string hom_code = m[1].str();
        if (!Instance().DecodeHomCode(hom_code.c_str(), out)) {
            Log::Log("Failed to DecodeHomCode from %s", m[1].str().c_str());
            out->state = HallOfMonumentsAchievements::State::Error;
            if (callback) callback(out);
            return;
        }
        out->state = HallOfMonumentsAchievements::State::Done;
        if (callback) callback(out);
        });
}
void HallOfMonumentsAchievements::OpenInBrowser() {
    const auto url = std::format("https://hom.guildwars2.com/en/#details={}&page=main", hom_code);
    GW::GameThread::Enqueue([url] {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)url.c_str());
        });
}
