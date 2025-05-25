#include "NPCVoiceModule.h"
#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/Utilities/Scanner.h>

#include <Logger.h>
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include <Modules/AudioSettings.h>

#include <Defines.h>
#include <ImGuiAddons.h> // Add this include
#include <algorithm>
#include <curl/curl.h>
#include <sstream>
#include <thread>

namespace {

    const char* voice_id_human_male = "2EiwWnXFnvU5JabPnv8n";
    const char* voice_id_human_female = "EXAVITQu4vr4xnSDxMaL";


        // TTS Provider settings
    TTSProvider current_tts_provider = TTSProvider::ElevenLabs;

    // API Keys
    std::string elevenlabs_api_key = "";
    std::string openai_api_key = "";

    // UI buffers for API keys
    char elevenlabs_api_key_buffer[256] = {0};
    char openai_api_key_buffer[256] = {0};
    namespace GWRace {
        constexpr uint8_t HUMAN = 0;
        constexpr uint8_t CHARR = 1;
        constexpr uint8_t NORN = 2;
        constexpr uint8_t ASURA = 3;
        constexpr uint8_t TENGU = 4;
        constexpr uint8_t DWARF = 5;
        constexpr uint8_t CENTAUR = 5;
    } // namespace GWRace

    enum class TraderType : uint8_t { Merchant, RuneTrader, ArmorCrafter, WeaponCustomizer, MaterialTrader, RareMaterialTrader, DyeTrader, OtherItemCrafter };

    std::map<std::tuple<GW::Region, TraderType>, std::wstring> merchant_greetings;

        // Simple log system - store last 5 messages
    std::deque<std::string> voice_log_messages;
    const size_t MAX_LOG_MESSAGES = 5;

    // Replace VoiceLog calls with this function
    void VoiceLog(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Add to our internal log
        voice_log_messages.push_front(std::string(buffer));

        // Keep only last 5 messages
        while (voice_log_messages.size() > MAX_LOG_MESSAGES) {
            voice_log_messages.pop_back();
        }

        // Still log to main log system
        Log::Log("%s", buffer);
    }

        // Utility function to convert Language enum to ElevenLabs language code
    std::string LanguageToAbbreviation(GW::Constants::Language language)
    {
        switch (language) {
            case GW::Constants::Language::English:
                return "en";
            case GW::Constants::Language::Korean:
                return "ko";
            case GW::Constants::Language::French:
                return "fr";
            case GW::Constants::Language::German:
                return "de";
            case GW::Constants::Language::Italian:
                return "it";
            case GW::Constants::Language::Spanish:
                return "es";
            case GW::Constants::Language::TraditionalChinese:
                return "zh"; // or "zh-tw" for Traditional Chinese specifically
            case GW::Constants::Language::Japanese:
                return "ja";
            case GW::Constants::Language::Polish:
                return "pl";
            case GW::Constants::Language::Russian:
                return "ru";
            case GW::Constants::Language::BorkBorkBork:
                return "sv"; // Swedish (assuming this is the Swedish Chef reference)
            case GW::Constants::Language::Unknown:
            default:
                return "en"; // Default to English
        }
    }

    const TraderType GetTraderType(const wchar_t* name)
    {
        std::wstring trader_name(name ? name : L"");
        std::transform(trader_name.begin(), trader_name.end(), trader_name.begin(), ::tolower);
        if (trader_name.find(L"merchant") != std::wstring::npos) return TraderType::Merchant;
        if (trader_name.find(L"rune") != std::wstring::npos) return TraderType::RuneTrader;
        if (trader_name.find(L"armor") != std::wstring::npos) return TraderType::ArmorCrafter;
        if (trader_name.find(L"weapon") != std::wstring::npos) return TraderType::WeaponCustomizer;
        if (trader_name.find(L"rare mat") != std::wstring::npos) return TraderType::MaterialTrader;
        if (trader_name.find(L"material") != std::wstring::npos) return TraderType::RareMaterialTrader;
        if (trader_name.find(L"dye") != std::wstring::npos) return TraderType::DyeTrader;
        return TraderType::OtherItemCrafter;
    }


    const bool GenderFromModelFileId(const uint32_t model_file_id, uint32_t* gender) {
        switch (model_file_id) {
            case 0x3c82:  // male caster
            case 0x1c604: // male warrior
            case 0x1C603: // male npc
            case 0x1F812: // male centaur
            case 0x22839:
            case 0x17728:
            case 0x31CF4: // male derv
            case 0x31BBC: // male para
            case 0x3EB1: // male henchman, istan
            case 0x228EE: // Flaming scepter mage
            case 0x3FF9: // Canthan mesmer
            case 0x27692: // Canthan sin
            case 0x2786A: // Canthan rit
            case 0x2CCF0: // Ludo
            case 0x2CCF1: // Instructor ng
            case 0x25F90: // high priest zhang
            case 0x44264: // istani male
            case 0x1F816: // male dwarf
            case 0x228EF: // ascalonian warrior/ vanguard
            case 0x228ED: // merc registrar
            case 0x227E5:
                return *gender = 1, true;
            case 0x3F8F: // female war
            case 0x3F8B:
            case 0x4426B: // female npc
            case 0x3212:  // female warrior
            case 0x1c601: // female caster
            case 0x227FF:
            case 0x3F81:
            case 0x3F82: // Istani female henchman
            case 0x3D67: // Factions hench
            case 0x27872: // Rit hench
            case 0x27699: // Assassin hench
            case 0x31D00: // istani female
            case 0x22940:// white mantle
            case 0x22906:
            case 0x227E6:
                return *gender = 0, true;
        }
        return false;
    }
    const bool RaceFromModelFileId(const uint32_t model_file_id, uint8_t* race)
    {
        switch (model_file_id) {
            case 0x3c82:  // male caster
            case 0x1c604: // male warrior
            case 0x1C603: // male npc
            case 0x1F812: // male centaur
            case 0x22839:
            case 0x17728:
            case 0x31CF4: // male derv
            case 0x31BBC: // male para
            case 0x3EB1:  // male henchman, istan
            case 0x3F8F:  // female war
            case 0x3F8B:
            case 0x4426B: // female npc
            case 0x3212:  // female warrior
            case 0x1c601: // female caster
            case 0x227FF:
            case 0x3F82: // Istani female henchman
            case 0x228EE: // Flaming scepter mage
            case 0x3D67:
            case 0x27872: // Rit hench
            case 0x27699: // Assassin hench
            case 0x3FF9:  // Canthan mesmer
            case 0x27692: // Canthan sin
            case 0x2786A: // Canthan rit
            case 0x31D00: // istani female
            case 0x44264: // istani male
            case 0x228EF:
            case 0x22940:
            case 0x228ED: // merc registrar
            case 0x3F81:
            case 0x22906:
            case 0x227E5:
            case 0x227E6:
                return *race = GWRace::HUMAN, true;
            case 0x1F816:
                return *race = GWRace::DWARF, true;
        }
        return false;
    }

    const bool GenderFromAgent(GW::AgentLiving* agent, uint32_t* gender) {
        if (!(agent && agent->GetIsLivingType() && gender)) 
            return false;
        if (agent->IsNPC()) {
            const auto npc = GW::Agents::GetNPCByID(agent->player_number);
            if (!npc) return false;
            return GenderFromModelFileId(npc->model_file_id, gender);
        }
        return false;
    }
    const bool RaceFromAgent(GW::AgentLiving* agent, uint8_t* race) {
        if (!(agent && agent->GetIsLivingType() && race)) return false;
        if (agent->IsNPC()) {
            const auto npc = GW::Agents::GetNPCByID(agent->player_number);
            if (!npc) return false;
            return RaceFromModelFileId(npc->model_file_id, race);
        }
        return false;
    }


    struct VoiceProfile {
        std::string voice_id;
        float stability = 0.5f;
        float similarity = 0.5f;
        float style = 0.5f;
        float speaking_rate = 1.0f;
        std::string accent_modifier = "";

        VoiceProfile() = default;
        VoiceProfile(const std::string& id, float stab, float sim, float st, float rate = 1.0f, const std::string& accent = "") : voice_id(id), stability(stab), similarity(sim), style(st), speaking_rate(rate), accent_modifier(accent) {}
    };

    // Voice mapping system (unchanged)
    std::map<std::tuple<uint8_t, uint8_t, GW::Region>, VoiceProfile> voice_matrix;
    std::map<uint32_t, VoiceProfile> special_npc_voices;
    uint32_t npc_ids_to_ignore[] = {
        1991 // Durmand
    };

    // Cost optimization settings
    bool only_use_first_dialog = true;
    bool rate_limiting_enabled = true;
    float min_delay_between_calls = 2.0f;
    std::chrono::steady_clock::time_point last_api_call;

    uint32_t last_dialog_agent_id = 0;

    size_t max_text_length = 256;
    VoiceProfile default_voice_profile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f, "");

    std::chrono::steady_clock::time_point audio_start_time;
    float current_audio_duration = 0.0f; // Duration in seconds
    bool audio_is_playing = false;
    bool generating_voice = false;

    GW::Vec3f GetPlayerVec3f() {
        const auto player = GW::Agents::GetControlledCharacter();
        return player ? GW::Vec3f(player->pos.x, player->pos.y, player->z) : GW::Vec3f();
    }
    GW::GamePos GetPlayerPosition()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        return player ? player->pos : GW::GamePos();
    }

    // Function to estimate audio duration from file size (rough approximation)
    float EstimateAudioDuration(const std::filesystem::path& audio_file)
    {
        try {
            auto file_size = std::filesystem::file_size(audio_file);
            // Rough estimation: WAV files are typically ~16KB per second at 16-bit, 22kHz
            // This is approximate - actual duration depends on sample rate, bit depth, etc.
            float estimated_duration = static_cast<float>(file_size) / 16000.0f; // bytes per second
            return std::max(1.0f, estimated_duration) - .5f;                           // Minimum 1 second
        } catch (const std::exception&) {
            return 2.0f; // Default fallback duration
        }
    }

    // Function to check if current audio is still playing
    bool IsAudioStillPlaying()
    {
        if (!audio_is_playing) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - audio_start_time).count();

        if (elapsed >= current_audio_duration) {
            audio_is_playing = false;
            return false;
        }

        return true;
    }

    // Enhanced audio playback function
    void PlayAudioFile(const std::filesystem::path& file_path)
    {
        // Don't play more than one dialog track at once
        if (IsAudioStillPlaying()) {
            return;
        }

        // Estimate duration of the new audio file
        current_audio_duration = EstimateAudioDuration(file_path);

        // Play the new audio
        const auto pos = GetPlayerVec3f();
        // 0x1400 means "this audio file in positional", so it will play in 3D space relative to the position given
        // 0x4 means "this is a dialog audio file"
        AudioSettings::PlaySound(file_path.wstring().c_str(), &pos, 0x1400 | 0x4);

        // Mark as playing and record start time
        audio_is_playing = true;
        audio_start_time = std::chrono::steady_clock::now();

        VoiceLog("Playing audio file: %s (estimated duration: %.1fs)", file_path.filename().string().c_str(), current_audio_duration);
    }

    // Extract first sentence from text
    std::wstring ExtractFirstSentence(const std::wstring& text)
    {
        if (text.empty()) return L"";

        std::wstring trimmed = text;

        // Remove leading/trailing whitespace
        size_t start = trimmed.find_first_not_of(L" \t\n\r");
        if (start == std::wstring::npos) return L"";
        trimmed = trimmed.substr(start);

        // Find sentence ending punctuation
        std::vector<wchar_t> sentence_endings = {L'.', L'!', L'?', L';', L':'};
        size_t min_pos = std::wstring::npos;

        for (wchar_t ending : sentence_endings) {
            size_t pos = trimmed.find(ending);
            if (pos == std::wstring::npos) continue;
            // Make sure it's not an abbreviation or decimal
            bool is_sentence_end = true;

            // Check for common abbreviations (Mr., Dr., etc.)
            if (ending == L'.' && pos > 0) {
                // Look for patterns like "Mr." or single letters
                if (pos >= 2) {
                    std::wstring before = trimmed.substr(pos - 2, 2);
                    if (before == L"Mr" || before == L"Dr" || before == L"Ms" || before == L"St" || before == L"Mt" || before == L"vs") {
                        is_sentence_end = false;
                    }
                }

                // Check if it's a single letter abbreviation
                if (pos > 0 && pos < trimmed.length() - 1) {
                    if (trimmed[pos - 1] != L' ' && trimmed[pos + 1] == L' ' && pos >= 1 && iswupper(trimmed[pos - 1])) {
                        is_sentence_end = false;
                    }
                }

                // Check for numbers (decimals)
                if (pos > 0 && pos < trimmed.length() - 1) {
                    if (iswdigit(trimmed[pos - 1]) && iswdigit(trimmed[pos + 1])) {
                        is_sentence_end = false;
                    }
                }
            }

            if (is_sentence_end && (min_pos == std::wstring::npos || pos < min_pos)) {
                min_pos = pos;
            }
        }

        // If no sentence ending found, use the whole text up to max length
        if (min_pos == std::wstring::npos) {
            if (trimmed.length() > max_text_length) {
                trimmed = trimmed.substr(0, max_text_length);
                // Try to break at word boundary
                auto last_space = trimmed.find_last_of(L' ');
                if (last_space != std::wstring::npos && last_space > max_text_length * 0.7) {
                    trimmed = trimmed.substr(0, last_space);
                }
                trimmed += L"...";
            }
            return trimmed;
        }

        // Extract first sentence including the punctuation
        std::wstring first_sentence = trimmed.substr(0, min_pos + 1);

        // Remove trailing whitespace
        size_t end = first_sentence.find_last_not_of(L" \t\n\r");
        if (end != std::wstring::npos) {
            first_sentence = first_sentence.substr(0, end + 1);
        }

        return first_sentence;
    }

    // Sanitise the encoded dialog to be more generic; avoids extra costs in decoding the current player name and numeric args
    std::wstring PreprocessEncodedTextForTTS(const std::wstring& text)
    {
        // replace player name
        auto result = TextUtils::ctre_regex_replace<L"\x0ba9\x0107[^\x0001]+\x0001", L"\x0ba9\x0107" "Chosen\x0001">(text);
        // replace numeric args
        result = TextUtils::ctre_regex_replace<L"[\x0101\x102\x103\x104].", L"">(result);
        return result;
    }

    // Cost optimization functions
    std::wstring PreprocessTextForTTS(const std::wstring& text)
    {
        std::wstring processed = text;

        processed = TextUtils::ctre_regex_replace<L"<a=[^<]+[^>]+>", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"<br>|<brx>|<p>", L". ">(processed);
        processed = TextUtils::StripTags(processed);

        // First, extract only the first sentence if enabled
        processed = ExtractFirstSentence(processed);

        if (processed.empty()) return L"";

        // Remove excessive punctuation that doesn't affect speech
        processed = TextUtils::ctre_regex_replace<L"[.]{2,}", L".">(processed);
        processed = TextUtils::ctre_regex_replace<L"[!]{2,}", L"!">(processed);
        processed = TextUtils::ctre_regex_replace<L"[?]{2,}", L"?">(processed);

        // Remove parenthetical asides
        processed = TextUtils::ctre_regex_replace<L"\\([^)]*\\)", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"\\[[^\\]]*\\]", L"">(processed);
        // Normalize whitespace
        processed = TextUtils::ctre_regex_replace<L"\\s+", L" ">(processed);

        // Final length check (should rarely be needed now with first sentence extraction)
        if (processed.length() > max_text_length) {
            processed = processed.substr(0, max_text_length);
            auto last_space = processed.find_last_of(L' ');
            if (last_space != std::wstring::npos && last_space > max_text_length * 0.8) {
                processed = processed.substr(0, last_space) + L"...";
            }
        }

        return processed;
    }

    bool CanMakeAPICall()
    {
        if (!rate_limiting_enabled) return true;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_api_call).count();
        return elapsed >= min_delay_between_calls;
    }

    GW::Region GetRegionFromMapID(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        return map_info->region;
    }

    VoiceProfile* GetVoiceProfile(GW::AgentLiving* agent, GW::Constants::MapID map_id)
    {
        if (!agent) return nullptr;
        const wchar_t* name = GW::Agents::GetAgentEncName(agent);
        if (!(name && *name && agent->GetIsLivingType() && agent->IsNPC())) return nullptr;

        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost 
            && agent->allegiance != GW::Constants::Allegiance::Npc_Minipet) {
            // Only process NPCs when in an explorable area; not players, enemies or pets
            return nullptr;
        }

        // Check for special named NPC first
        auto special_it = special_npc_voices.find(agent->player_number);
        if (special_it != special_npc_voices.end()) {
            return &special_it->second;
        }

        uint8_t race = 0;
        uint32_t gender = 0;
        if (!GenderFromAgent(agent, &gender)) return nullptr;
        if (!RaceFromAgent(agent, &race)) return nullptr;

        // Determine region from map
        const auto region = GetRegionFromMapID(map_id);

        // Try exact match first (sex, race, region)
        auto exact_key = std::make_tuple(gender, race, region);
        auto exact_it = voice_matrix.find(exact_key);
        if (exact_it != voice_matrix.end()) {
            return &exact_it->second;
        }

        // Try race-agnostic match (sex, race, "any")
        auto race_key = std::make_tuple(gender, race, GW::Region::Region_DevRegion);
        auto by_race = voice_matrix.find(race_key);
        if (by_race != voice_matrix.end()) {
            return &by_race->second;
        }

        auto sex_key = std::make_tuple(gender, GWRace::HUMAN, GW::Region::Region_DevRegion);
        auto by_gender = voice_matrix.find(sex_key);
        if (by_gender != voice_matrix.end()) {
            return &by_gender->second;
        }

        // Ultimate fallback
        return &default_voice_profile;
    }

    VoiceProfile* GetVoiceProfile(uint32_t agent_id, GW::Constants::MapID map_id)
    {
        return GetVoiceProfile(static_cast<GW::AgentLiving*> (GW::Agents::GetAgentByID(agent_id)), map_id);
    }

    GW::HookEntry UIMessage_HookEntry;
    GW::HookEntry PreUIMessage_HookEntry;
    void GenerateVoiceAPI(const std::wstring& _message, const VoiceProfile* profile, GW::Constants::Language language);

    void GenerateVoiceFromDecodedString(const wchar_t* decoded_str, const VoiceProfile* profile, GW::Constants::Language language)
    {
        if (!(decoded_str && *decoded_str && profile)) return;
        const auto pre_decoded = PreprocessTextForTTS(decoded_str);
        if (pre_decoded.empty()) return;
        GenerateVoiceAPI(pre_decoded, profile, language);
    }
    void GetNPCName(uint32_t agent_id, GW::UI::DecodeStr_Callback callback, void* param = nullptr) {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        const auto name = agent ? GW::Agents::GetAgentEncName(agent) : nullptr;
        if (!(name && *name)) {
            callback(param, L"");
            return;
        }
        GW::UI::AsyncDecodeStr(name, callback, param, GW::Constants::Language::English);
    }

    void GenerateVoiceFromEncodedString(const wchar_t* encoded_str, const VoiceProfile* profile)
    {
        if (!(encoded_str && *encoded_str && profile)) return;
        const auto pre_decoded = PreprocessEncodedTextForTTS(encoded_str);
        GW::UI::AsyncDecodeStr(
            pre_decoded.c_str(),
            [](void* param, const wchar_t* s) {
                GenerateVoiceFromDecodedString(s, (VoiceProfile*)param, GW::UI::GetTextLanguage());
            },
            (void*)profile, GW::UI::GetTextLanguage()
        );
    }

    bool IsEotnRegion() {
        // Check if the current map is EotN
        const auto map_info = GW::Map::GetMapInfo();
        switch (map_info->region) {
            case GW::Region::Region_TarnishedCoast:
            case GW::Region::Region_DepthsOfTyria:
            case GW::Region::Region_FarShiverpeaks:
            case GW::Region::Region_CharrHomelands:
                return true;
        }
        return false;
    }

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (msgid) {
            case GW::UI::UIMessage::kDialogBody: {
                const auto packet = (GW::UI::DialogBodyInfo*)wParam;
                const auto was_dialog_already_open = GW::UI::GetFrameByLabel(L"NPCInteract") && packet->agent_id == last_dialog_agent_id;
                last_dialog_agent_id = packet->agent_id;
                if (only_use_first_dialog && was_dialog_already_open) {
                    return; // Skip if already open and only using first dialog
                }
                GenerateVoiceFromEncodedString(packet->message_enc, GetVoiceProfile(packet->agent_id, GW::Map::GetMapID()));
            } break;
        }
    }
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (msgid) {
            case GW::UI::UIMessage::kAgentSpeechBubble: {
                // Some NPCs don't have a dialog window, but they can still have speech bubbles
                const auto packet = (GW::UI::UIPacket::kAgentSpeechBubble*)wParam;
                const auto agent = GW::Agents::GetAgentByID(packet->agent_id);
                if (GW::GetDistance(agent->pos, GetPlayerPosition()) > GW::Constants::Range::Adjacent) {
                    return; // Ignore distant NPCs
                }
                GenerateVoiceFromEncodedString(packet->message, GetVoiceProfile(packet->agent_id, GW::Map::GetMapID()));
            } break;
            case GW::UI::UIMessage::kVendorWindow: {
                const auto packet = (GW::UI::UIPacket::kVendorWindow*)wParam;
                if (IsEotnRegion()) 
                    return; // EotN vendors already have greetings
                switch (packet->transaction_type) {
                    case GW::Merchant::TransactionType::CollectorBuy: {
                        // Find and use the collector's dialog context
                        auto collector_dialog = GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Vendor"), 0, 0, 2);
                        const auto context = (wchar_t**)GW::UI::GetFrameContext(collector_dialog);
                        if (context && *context) GenerateVoiceFromEncodedString(*context, GetVoiceProfile(packet->unk, GW::Map::GetMapID()));
                    } break;
                    case GW::Merchant::TransactionType::MerchantBuy:
                    case GW::Merchant::TransactionType::CrafterBuy:
                    case GW::Merchant::TransactionType::WeaponsmithCustomize:
                    case GW::Merchant::TransactionType::TraderBuy: {
                        if (GW::UI::GetTextLanguage() != GW::Constants::Language::English) return; // We only support English trader quotes for now
                        // For traders, find out the name of the trader and determine which greeting to use
                        const auto profile = GetVoiceProfile(packet->unk, GW::Map::GetMapID());
                        GetNPCName(
                            packet->unk,
                            [](void* param, const wchar_t* s) {
                                const auto region = GetRegionFromMapID(GW::Map::GetMapID());
                                auto key = std::make_tuple(region, GetTraderType(s));
                                auto it = merchant_greetings.find(key);
                                if (it != merchant_greetings.end() && !it->second.empty()) {
                                    GenerateVoiceFromDecodedString(it->second.c_str(), (VoiceProfile*)param,GW::UI::GetTextLanguage());
                                }
                            },
                            (void*)profile
                        );

                    } break;
                }
            } break;
        }
    }

// Fixed cache key generation that works with Korean/Japanese/Chinese text
    // Fixed cache key generation that works with Korean/Japanese/Chinese text
    std::string GenerateOptimizedCacheKey(const VoiceProfile* p, const std::wstring& text, GW::Constants::Language)
    {
        // Just hash the text - works with ANY language
        auto text_hash = std::hash<std::wstring>{}(text);
        return std::format("{}_{:x}_{}.mp3", p->voice_id, text_hash,text.size());
    }

    size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
    {
        size_t total_size = size * nmemb;
        userp->append((char*)contents, total_size);
        return total_size;
    }

    // Add OpenAI TTS function
    std::string GenerateVoiceOpenAI(const std::wstring& text, const VoiceProfile* profile, GW::Constants::Language language)
    {
        std::string audio_data;
        if (!openai_api_key.length()) {
            VoiceLog("No OpenAI API Key");
            return audio_data;
        }
        CURL* curl = curl_easy_init();

        if (!curl) {
            VoiceLog("Failed to initialize CURL for OpenAI");
            return audio_data;
        }

        try {
            std::string url = "https://api.openai.com/v1/audio/speech";

            nlohmann::json request_body;
            request_body["model"] = "gpt-4o-mini-tts"; // Use cheaper tts-1 model, or tts-1-hd for higher quality
            request_body["input"] = TextUtils::WStringToString(text);

            // Map gender to OpenAI voices
            std::string voice_name;
            if (profile->voice_id == voice_id_human_female) {
                voice_name = "nova"; // Female voice
            }
            else {
                voice_name = "onyx"; // Male voice
            }
            request_body["voice"] = voice_name;
            request_body["response_format"] = "mp3";
            request_body["speed"] = profile->speaking_rate;
            request_body["language"] = LanguageToAbbreviation(language);

            // Set up headers
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + openai_api_key).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            // Configure CURL
            const auto request_str = request_body.dump();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Check response code
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (res != CURLE_OK) {
                VoiceLog("CURL request failed for OpenAI: %s", curl_easy_strerror(res));
                audio_data.clear();
            }
            else if (response_code != 200) {
                VoiceLog("OpenAI API returned error code: %ld\n%s", response_code, audio_data.c_str());
                audio_data.clear();
            }
            else {
                VoiceLog("OpenAI voice generation successful, received %zu bytes", audio_data.size());
            }

            // Cleanup
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

        } catch (const std::exception& e) {
            VoiceLog("Exception in OpenAI voice generation: %s", e.what());
            curl_easy_cleanup(curl);
            audio_data.clear();
        }
        return audio_data;
    }

    // Modified GenerateVoice function with cheaper model option
    std::string GenerateVoiceElevenLabs(const std::wstring& text, const VoiceProfile* profile, GW::Constants::Language language)
    {
        std::string audio_data;
        if (!elevenlabs_api_key.length()) {
            Log::Warning("No ElevenLabs API Key");
            return audio_data;
        }
        CURL* curl = curl_easy_init();

        if (!curl) {
            VoiceLog("Failed to initialize CURL");
            return audio_data;
        }

        try {
            std::string url = "https://api.elevenlabs.io/v1/text-to-speech/" + profile->voice_id;

            nlohmann::json request_body;
            nlohmann::json voice_settings;

            request_body["text"] = TextUtils::WStringToString(text);

            request_body["model_id"] = "eleven_flash_v2_5"; // Cheaper option

            voice_settings["stability"] = profile->stability;
            voice_settings["similarity_boost"] = profile->similarity;
            voice_settings["style"] = profile->style;
            voice_settings["use_speaker_boost"] = false; // Disable to save costs

            request_body["voice_settings"] = voice_settings;

            request_body["language"] = LanguageToAbbreviation(language);

            // Set up headers
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("xi-api-key: " + elevenlabs_api_key).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Accept: audio/mpeg");

            // Configure CURL
            const auto request_str = request_body.dump();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Check response code
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (res != CURLE_OK) {
                VoiceLog("CURL request failed: %s", curl_easy_strerror(res));
                audio_data.clear();
            }
            else if (response_code != 200) {
                VoiceLog("ElevenLabs API returned error code: %ld\n%s", response_code,audio_data.c_str());
                audio_data.clear();
            }
            else {
                VoiceLog("Voice generation successful, received %zu bytes", audio_data.size());
            }

            // Cleanup
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

        } catch (const std::exception& e) {
            VoiceLog("Exception in voice generation: %s", e.what());
            curl_easy_cleanup(curl);
            audio_data.clear();
        }

        return audio_data;
    }

    std::string GenerateVoice(const std::wstring& text, const VoiceProfile* profile, GW::Constants::Language language)
    {
        switch (current_tts_provider) {
            case TTSProvider::OpenAI:
                return GenerateVoiceOpenAI(text, profile, language);
            case TTSProvider::ElevenLabs:
            default:
                return GenerateVoiceElevenLabs(text, profile, language);
        }
    }


    void GenerateVoiceAPI(const std::wstring& _message, const VoiceProfile* profile, GW::Constants::Language language)
    {
        if (generating_voice) return;
        generating_voice = true;

        Resources::EnqueueWorkerTask([message = _message, profile, language]() {
            const auto cache_key = GenerateOptimizedCacheKey(profile, message,language);
            const auto path = Resources::GetPath("NPCVoiceCache") / LanguageToAbbreviation(language) / cache_key;

            // Check cache first
            if (std::filesystem::exists(path)) {
                PlayAudioFile(path);
                return generating_voice = false;
            }

            auto audio_data = GenerateVoice(message, profile, language);
            if (!audio_data.size()) {
                VoiceLog("Failed to generate voice data");
                return generating_voice = false;
            }

            // Update rate limiting
            last_api_call = std::chrono::steady_clock::now();

            // Save to cache
            Resources::EnsureFolderExists(path.parent_path());
            FILE* fp = fopen(path.string().c_str(), "wb");
            if (!fp) {
                VoiceLog("Failed to open file for writing: %s", path.string().c_str());
                return generating_voice = false;
            }
            const auto written = fwrite(audio_data.data(), sizeof(audio_data[0]), audio_data.size(), fp);
            fclose(fp);
            if (written < 1) {
                VoiceLog("Failed to write audio data to file: %s", path.string().c_str());
                return generating_voice = false;
            }

            PlayAudioFile(path);
            return generating_voice = false;
        });
    }
} // namespace

void NPCVoiceModule::Initialize()
{
    ToolboxModule::Initialize();

    voice_matrix.clear();
    special_npc_voices.clear();

    // Human voices by region and sex
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Ascalon}] = VoiceProfile(voice_id_human_male, 0.7f, 0.6f, 0.5f, 0.95f, "gruff");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Ascalon}] = VoiceProfile(voice_id_human_female, 0.6f, 0.7f, 0.6f, 1.0f, "determined");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Kryta}] = VoiceProfile(voice_id_human_male, 0.5f, 0.7f, 0.4f, 1.0f, "refined");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Kryta}] = VoiceProfile(voice_id_human_female, 0.4f, 0.8f, 0.5f, 1.05f, "noble");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Maguuma}] = VoiceProfile(voice_id_human_male, 0.6f, 0.5f, 0.6f, 0.90f, "tribal");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Maguuma}] = VoiceProfile(voice_id_human_female, 0.5f, 0.6f, 0.7f, 0.95f, "wild");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_CrystalDesert}] = VoiceProfile(voice_id_human_male, 0.7f, 0.4f, 0.3f, 0.85f, "mystical");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_CrystalDesert}] = VoiceProfile(voice_id_human_female, 0.6f, 0.5f, 0.4f, 0.90f, "ancient");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_NorthernShiverpeaks}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.4f, 0.80f, "mountain");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_NorthernShiverpeaks}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.5f, 0.85f, "hardy");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_FarShiverpeaks}] = VoiceProfile(voice_id_human_male, 0.9f, 0.2f, 0.3f, 0.75f, "isolated");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_FarShiverpeaks}] = VoiceProfile(voice_id_human_female, 0.8f, 0.3f, 0.4f, 0.80f, "distant");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_ShingJea}] = VoiceProfile(voice_id_human_male, 0.3f, 0.7f, 0.2f, 1.0f, "canthan_scholarly");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_ShingJea}] = VoiceProfile(voice_id_human_female, 0.2f, 0.8f, 0.3f, 1.05f, "canthan_gentle");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Kaineng}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 1.0f, "canthan_urban");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Kaineng}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 1.0f, "canthan_refined");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Kurzick}] = VoiceProfile(voice_id_human_male, 0.5f, 0.5f, 0.4f, 0.95f, "canthan_forest");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Kurzick}] = VoiceProfile(voice_id_human_female, 0.4f, 0.6f, 0.5f, 1.0f, "canthan_mystic");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Luxon}] = VoiceProfile(voice_id_human_male, 0.5f, 0.6f, 0.4f, 0.95f, "canthan_sea");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Luxon}] = VoiceProfile(voice_id_human_female, 0.4f, 0.7f, 0.5f, 1.0f, "canthan_tide");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Istan}] = VoiceProfile(voice_id_human_male, 0.4f, 0.7f, 0.5f, 1.0f, "elonian_island");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Istan}] = VoiceProfile(voice_id_human_female, 0.3f, 0.8f, 0.6f, 1.05f, "elonian_tropical");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Kourna}] = VoiceProfile(voice_id_human_male, 0.7f, 0.5f, 0.4f, 0.90f, "elonian_warrior");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Kourna}] = VoiceProfile(voice_id_human_female, 0.6f, 0.6f, 0.5f, 0.95f, "elonian_strong");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Vaabi}] = VoiceProfile(voice_id_human_male, 0.3f, 0.8f, 0.6f, 1.1f, "elonian_royal");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Vaabi}] = VoiceProfile(voice_id_human_female, 0.2f, 0.9f, 0.7f, 1.15f, "elonian_luxurious");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_Desolation}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.2f, 0.85f, "elonian_cursed");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_Desolation}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.3f, 0.90f, "elonian_haunted");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_TarnishedCoast}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 1.05f, "intellectual");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_TarnishedCoast}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 1.10f, "scholarly");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_DepthsOfTyria}] = VoiceProfile(voice_id_human_male, 0.6f, 0.4f, 0.3f, 0.85f, "underground");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_DepthsOfTyria}] = VoiceProfile(voice_id_human_female, 0.5f, 0.5f, 0.4f, 0.90f, "deep");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_CharrHomelands}] = VoiceProfile(voice_id_human_male, 0.9f, 0.2f, 0.2f, 0.80f, "prisoner");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_CharrHomelands}] = VoiceProfile(voice_id_human_female, 0.8f, 0.3f, 0.3f, 0.85f, "captive");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_HeroesAscent}] = VoiceProfile(voice_id_human_male, 0.6f, 0.7f, 0.8f, 1.0f, "legendary");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_HeroesAscent}] = VoiceProfile(voice_id_human_female, 0.5f, 0.8f, 0.9f, 1.05f, "heroic");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_FissureOfWoe}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 0.90f, "eternal");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_FissureOfWoe}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 0.95f, "otherworldly");
    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_DomainOfAnguish}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.2f, 0.85f, "tormented");
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_DomainOfAnguish}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.3f, 0.90f, "anguished");

    voice_matrix[{1, GWRace::CHARR, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.8f, 0.4f, 0.3f, 0.85f, "growling");
    voice_matrix[{0, GWRace::CHARR, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.7f, 0.5f, 0.4f, 0.90f, "fierce");

    voice_matrix[{1, GWRace::DWARF, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.9f, 0.3f, 0.4f, 0.80f, "gravelly");
    voice_matrix[{0, GWRace::DWARF, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.8f, 0.4f, 0.5f, 0.85f, "robust");

    voice_matrix[{1, GWRace::HUMAN, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f);
    voice_matrix[{0, GWRace::HUMAN, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.5f, 0.5f, 0.5f, 1.0f);

    // Special named NPCs
    //special_npc_voices[3492] = VoiceProfile("oWAxZDx7w5VEj9dCyTzz", 0.6f, 0.8f, 0.7f); // Devona

    // Replace the merchant_greetings initialization in Initialize() with this:

    // Initialize merchant greetings by region and trader type
    merchant_greetings.clear();

    // ASCALON - War-torn, survival focused
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::Merchant}] = L"Welcome, traveler, times are hard but I still have goods to trade.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::ArmorCrafter}] = L"Need armor that can withstand Charr claws?";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::WeaponCustomizer}] = L"Looking for a weapon that can pierce Charr hide?";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RuneTrader}] = L"Ancient runes, salvaged from the ruins of our kingdom.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::MaterialTrader}] = L"Materials salvaged from the ruins, at fair prices.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RareMaterialTrader}] = L"Rare materials, recovered from the Searing's aftermath.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::DyeTrader}] = L"Colors to brighten these dark times.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::OtherItemCrafter}] = L"Need something crafted by skills that survived the Searing?";

    // PRE-SEARING ASCALON - Peaceful, idyllic
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::Merchant}] = L"Good day, welcome to beautiful Ascalon!";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::ArmorCrafter}] = L"The finest armor in all of prosperous Ascalon.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::WeaponCustomizer}] = L"Welcome, these weapons represent centuries of Ascalonian tradition.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RuneTrader}] = L"Ancient runes from our kingdom's golden age.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::MaterialTrader}] = L"The finest materials from across our prosperous kingdom.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RareMaterialTrader}] = L"Rare treasures from the abundant lands of Ascalon.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::DyeTrader}] = L"Vibrant colors from our peaceful realm.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::OtherItemCrafter}] = L"Greetings, I offer the finest craftsmanship in all of Ascalon.";

    // KRYTA - Noble, refined
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::Merchant}] = L"Good day to you, welcome to my establishment!";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::ArmorCrafter}] = L"Protection fit for nobles and heroes alike.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::WeaponCustomizer}] = L"Seeking a weapon of distinction?";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RuneTrader}] = L"Ah, a seeker of magical enhancement with excellent taste.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::MaterialTrader}] = L"Fine materials from across the kingdom, at your service.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RareMaterialTrader}] = L"Rare treasures from the royal vaults of Kryta.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::DyeTrader}] = L"Royal colors for distinguished customers.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::OtherItemCrafter}] = L"Greetings, I offer the finest crafting services in Kryta.";

    // MAGUUMA JUNGLE - Wild, dangerous
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::Merchant}] = L"Welcome, brave soul, few venture this deep into the jungle.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::ArmorCrafter}] = L"Armor tested against the jungle's deadly creatures.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::WeaponCustomizer}] = L"Weapons tested against the beasts of the deep jungle.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RuneTrader}] = L"Ancient magic, whispered by the jungle spirits.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::MaterialTrader}] = L"Exotic materials from the heart of the Maguuma.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RareMaterialTrader}] = L"Rare jungle treasures, if you have the courage.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::DyeTrader}] = L"Wild colors from nature's most dangerous realm.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials blessed by the jungle spirits.";

    // CRYSTAL DESERT - Harsh, mystical
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::Merchant}] = L"Welcome, desert wanderer, supplies are precious here.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::ArmorCrafter}] = L"Protection forged in the crystal caves.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::WeaponCustomizer}] = L"Weapons forged in the crystal caves, sharp as dragon glass.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RuneTrader}] = L"Ancient power, crystallized in the desert sands.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::MaterialTrader}] = L"Rare crystals and desert treasures await the worthy.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RareMaterialTrader}] = L"Mystical crystals from the heart of the desert.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::DyeTrader}] = L"Colors as brilliant as crystal formations.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::OtherItemCrafter}] = L"Greetings, I work with crystal and sand-blessed materials.";

    // NORTHERN SHIVERPEAKS - Cold, dwarven
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::Merchant}] = L"Welcome to the peaks, warm yourself and browse my wares.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::ArmorCrafter}] = L"Dwarven protection, built to last centuries!";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::WeaponCustomizer}] = L"Weapons forged in the ancient dwarven tradition.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RuneTrader}] = L"Ancient dwarven runes, carved in stone and memory.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::MaterialTrader}] = L"Mountain stone and dwarven steel, the finest materials.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RareMaterialTrader}] = L"Precious gems from the deepest mountain veins.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::DyeTrader}] = L"Mountain colors, bold as dwarven courage.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::OtherItemCrafter}] = L"Greetings, dwarven craftsmanship built to last centuries!";

    // FAR SHIVERPEAKS - Remote, harsh
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::Merchant}] = L"By the forge, another traveler reaches these distant peaks!";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::ArmorCrafter}] = L"Armor tempered by the harshest cold and winds.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::WeaponCustomizer}] = L"Weapons tempered by the coldest winds and hardest stone.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RuneTrader}] = L"Forgotten runes from the edge of the world.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::MaterialTrader}] = L"Materials from the edge of the world, rare beyond measure.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RareMaterialTrader}] = L"Treasures from where the world ends.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::DyeTrader}] = L"Colors from the world's frozen edge.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::OtherItemCrafter}] = L"Welcome, hardy soul, few seek crafting in these remote lands.";

    // CANTHA - SHING JEA - Peaceful, academic
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::Merchant}] = L"Honorable student, welcome to this place of learning.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::ArmorCrafter}] = L"Protection crafted with the wisdom of ancient masters.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::WeaponCustomizer}] = L"Weapons forged in the sacred traditions of Shing Jea.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RuneTrader}] = L"Ancient symbols blessed by monastery wisdom.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::MaterialTrader}] = L"Materials blessed by the monastery's ancient wisdom.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RareMaterialTrader}] = L"Rare treasures from the sacred archives.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::DyeTrader}] = L"Colors harmonious as monastery gardens.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::OtherItemCrafter}] = L"Greetings, I craft with the wisdom of ancient masters.";

    // CANTHA - KAINENG CITY - Urban, bustling
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::Merchant}] = L"Welcome to Kaineng City, the finest goods from across the empire!";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::ArmorCrafter}] = L"City armor, where tradition meets innovation.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::WeaponCustomizer}] = L"Weapons crafted in the heart of the empire's greatest city.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RuneTrader}] = L"Imperial runes from the empire's vast collection.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::MaterialTrader}] = L"Urban treasures and exotic materials from every corner of Cantha.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RareMaterialTrader}] = L"Rare goods from the empire's endless markets.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::DyeTrader}] = L"Imperial colors from the city of endless hues.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::OtherItemCrafter}] = L"Greetings, city craftsmanship meets ancient tradition.";

    // CANTHA - KURZICK LANDS - Forest, gothic
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::Merchant}] = L"Welcome to our sacred forests, nature's bounty awaits.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::ArmorCrafter}] = L"Protection blessed by the forest spirits.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::WeaponCustomizer}] = L"Weapons blessed by the ancient spirits of the wood.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RuneTrader}] = L"Forest runes, whispered by ancient trees.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::MaterialTrader}] = L"Materials harvested with respect for the eternal forest.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RareMaterialTrader}] = L"Sacred forest treasures, given freely by nature.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::DyeTrader}] = L"Forest colors, as eternal as the trees.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::OtherItemCrafter}] = L"Greetings, friend of the forest, I craft with nature's blessing.";

    // CANTHA - LUXON WATERS - Maritime, jade-themed
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::Merchant}] = L"Welcome, sea-friend, the jade winds bring good fortune.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::ArmorCrafter}] = L"Protection blessed by the jade sea's power.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::WeaponCustomizer}] = L"Weapons forged with the strength of the eternal tides.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RuneTrader}] = L"Sea runes, carved by tidal forces.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::MaterialTrader}] = L"Treasures from the jade sea, brought by favorable winds.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RareMaterialTrader}] = L"Rare jade gifts from the eternal sea.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::DyeTrader}] = L"Sea colors, shifting like jade waters.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials blessed by the jade sea.";

    // ELONA - ISTAN - Island paradise
    merchant_greetings[{GW::Region::Region_Istan, TraderType::Merchant}] = L"Ahlan wa sahlan, welcome to the jewel of Elona!";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::ArmorCrafter}] = L"Island protection, light as sea breeze.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::WeaponCustomizer}] = L"Weapons blessed by the island winds and morning sun.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RuneTrader}] = L"Island runes, blessed by ocean spirits.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::MaterialTrader}] = L"Exotic materials from the trade winds of Istan.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RareMaterialTrader}] = L"Precious island treasures from hidden coves.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::DyeTrader}] = L"Tropical colors, bright as paradise.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::OtherItemCrafter}] = L"Greetings, honored traveler, island craftsmanship at its finest.";

    // ELONA - KOURNA - Harsh mainland
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::Merchant}] = L"Welcome, desert warrior, Kourna's goods serve the strong.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::ArmorCrafter}] = L"Armor tested against centaurs and corsairs.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::WeaponCustomizer}] = L"Weapons tested against the centaurs and corsairs.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RuneTrader}] = L"Battle runes, forged in conflict.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::MaterialTrader}] = L"Materials hardened by sun and survival, proven in battle.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RareMaterialTrader}] = L"War spoils from the harsh mainland.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::DyeTrader}] = L"Desert colors, bold as warrior courage.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::OtherItemCrafter}] = L"Greetings, I forge in the harsh fires of the mainland.";

    // ELONA - VABBI - Wealthy, luxurious
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::Merchant}] = L"Welcome to Vabbi, land of princes and prosperity!";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::ArmorCrafter}] = L"Armor fit for princes, adorned with precious gems.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::WeaponCustomizer}] = L"Weapons fit for princes, adorned with precious gems.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RuneTrader}] = L"Royal runes from the prince's collections.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::MaterialTrader}] = L"The finest materials gold can buy, from Vabbi's treasuries.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RareMaterialTrader}] = L"Princely treasures from royal vaults.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::DyeTrader}] = L"Royal colors, luxurious as palace silks.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::OtherItemCrafter}] = L"Greetings, only the most exquisite craftsmanship in Vabbi.";

    // ELONA - DESOLATION - Cursed, dangerous
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::Merchant}] = L"Welcome, brave soul, few dare trade in these cursed lands.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::ArmorCrafter}] = L"Protection against demons and cursed winds.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::WeaponCustomizer}] = L"Weapons forged to face demons and the undead hordes.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RuneTrader}] = L"Cursed runes, powerful but dangerous.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::MaterialTrader}] = L"Materials touched by darkness, powerful but dangerous.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RareMaterialTrader}] = L"Forbidden treasures from the cursed realm.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::DyeTrader}] = L"Dark colors from the realm of shadows.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::OtherItemCrafter}] = L"Greetings, I work despite the sulfurous winds and darkness.";

    // CHARR HOMELANDS - Enemy territory, rare
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::Merchant}] = L"You dare trade in Charr lands, your courage impresses me.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::ArmorCrafter}] = L"Armor stolen from Charr forges.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::WeaponCustomizer}] = L"Weapons forged in the furnaces of our enemies.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RuneTrader}] = L"Charr runes, taken from their shamans.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::MaterialTrader}] = L"Materials stolen from the very heart of Charr power.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RareMaterialTrader}] = L"War trophies from the enemy homeland.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::DyeTrader}] = L"Colors of war from enemy territory.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::OtherItemCrafter}] = L"By the flame, a human seeks crafting in Charr territory!";

    // UNDERWORLD/SPECIAL AREAS - Mystical, otherworldly
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::Merchant}] = L"Welcome, eternal warrior, even here commerce finds a way.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::ArmorCrafter}] = L"Protection forged in eternal fires.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::WeaponCustomizer}] = L"Weapons forged in the fires of the eternal realm.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RuneTrader}] = L"Eternal runes from beyond mortal understanding.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::MaterialTrader}] = L"Materials from beyond the mortal realm, precious beyond gold.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RareMaterialTrader}] = L"Treasures from the realm of eternal flame.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::DyeTrader}] = L"Colors from the eternal realm.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials touched by eternity.";

    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::Merchant}] = L"Welcome to this realm of shadows, trade persists even here.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::ArmorCrafter}] = L"Protection forged in anguish and shadow.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::WeaponCustomizer}] = L"Weapons forged in anguish, tempered by eternal suffering.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RuneTrader}] = L"Runes of torment from the shadow realm.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::MaterialTrader}] = L"Materials born of anguish, powerful beyond mortal understanding.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RareMaterialTrader}] = L"Treasures from the realm of eternal torment.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::DyeTrader}] = L"Shadow colors from the realm of anguish.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::OtherItemCrafter}] = L"Greetings, shadow-walker, I craft with the essence of torment.";

    const GW::UI::UIMessage messages[] = {GW::UI::UIMessage::kDialogBody, GW::UI::UIMessage::kVendorWindow, GW::UI::UIMessage::kAgentSpeechBubble};

    for (auto message_id : messages) {
        GW::UI::RegisterUIMessageCallback(&PreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x1);
        GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry, message_id, OnPostUIMessage, 0x4000);
    }
}

void NPCVoiceModule::Terminate()
{
    ToolboxModule::Terminate();

    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&PreUIMessage_HookEntry);
}

void NPCVoiceModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    auto tmp = (uint32_t)current_tts_provider;
    LOAD_UINT(tmp);
    current_tts_provider = static_cast<TTSProvider>(tmp);
    LOAD_STRING(elevenlabs_api_key);
    strncpy_s(elevenlabs_api_key_buffer, elevenlabs_api_key.c_str(), sizeof(elevenlabs_api_key_buffer) - 1);
    LOAD_STRING(openai_api_key);
    strncpy_s(openai_api_key_buffer, openai_api_key.c_str(), sizeof(openai_api_key_buffer) - 1);
}

void NPCVoiceModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);

    // Save TTS provider
    SAVE_UINT(current_tts_provider);
    SAVE_STRING(elevenlabs_api_key);
    SAVE_STRING(openai_api_key);
}

void NPCVoiceModule::DrawSettingsInternal()
{
    ImGui::Separator();
    ImGui::Text("TTS Provider:");

    // TTS Provider selection
    const char* provider_names[] = {"ElevenLabs", "OpenAI TTS"};
    int current_provider = static_cast<int>(current_tts_provider);
    if (ImGui::Combo("TTS Service", &current_provider, provider_names, IM_ARRAYSIZE(provider_names))) {
        current_tts_provider = static_cast<TTSProvider>(current_provider);
    }

    ImGui::Separator();
    ImGui::Text("API Configuration:");
    const ImColor col(102, 187, 238, 255);
    // Show appropriate API key input based on selected provider
    if (current_tts_provider == TTSProvider::ElevenLabs) {
        if (ImGui::InputText("ElevenLabs API Key", elevenlabs_api_key_buffer, sizeof(elevenlabs_api_key_buffer))) {
            elevenlabs_api_key = elevenlabs_api_key_buffer;
        }
        
        ImGui::TextColored(col.Value, "Click Here to get an ElevenLabs API Key");
        if (ImGui::IsItemClicked()) {
            GW::GameThread::Enqueue([this]() {
                SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)"https://elevenlabs.io/app/settings/api-keys");
            });
        }
    }
    else {
        if (ImGui::InputText("OpenAI API Key", openai_api_key_buffer, sizeof(openai_api_key_buffer))) {
            openai_api_key = openai_api_key_buffer;
        }
        ImGui::TextColored(col.Value, "Click Here to get an OpenAI API Key");
        if (ImGui::IsItemClicked()) {
            GW::GameThread::Enqueue([this]() {
                SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)"https://platform.openai.com/api-keys");
            });
        }
    }

    ImGui::Separator();
    ImGui::Checkbox("Only process the first dialog of an NPC", &only_use_first_dialog);
    ImGui::ShowHelp("If enabled, only the first dialog of an NPC will be processed for TTS, similar to Eye of the North dialog.");
    // Simple log display
    ImGui::Separator();
    ImGui::Text("Recent Activity:");

    if (voice_log_messages.empty()) {
        ImGui::TextDisabled("No recent activity");
    }
    else {
        // Show messages in a read-only text box
        std::string combined_log;
        for (const auto& msg : voice_log_messages) {
            if (!combined_log.empty()) {
                combined_log += "\n";
            }
            combined_log += msg;
        }

        ImGui::InputTextMultiline("##VoiceLog", const_cast<char*>(combined_log.c_str()), combined_log.size(), ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
    }
}
