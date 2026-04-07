#include "TextToSpeechModule.h"
#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Utilities/Scanner.h>

#include <Logger.h>
#include <Modules/AudioSettings.h>
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>

#include <Defines.h>
#include <GWCA/GameEntities/Frame.h>
#include <GWCA/Utilities/Hooker.h>
#include <ImGuiAddons.h>
#include <RestClient.h>
#include <Timer.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/ToolboxUtils.h>
#include <algorithm>
#include <sstream>
#include <thread>
#include "GwDatTextureModule.h"

#include <Functiondiscoverykeys_devpkey.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

namespace {

    const char* voice_id_human_male = "2EiwWnXFnvU5JabPnv8n";
    const char* voice_id_human_female = "EXAVITQu4vr4xnSDxMaL";
    const char* voice_id_dwarven_male = "N2lVS1w4EtoT3dr4eOWO";

    const char* gwtts_hostname = "https://tts.gwtoolbox.com";
    // const char* gwtts_hostname = "http://localhost:8081";

    // Cost optimization settings
    bool stop_speech_when_dialog_closed = false;
    bool play_goodbye_messages = true;
    bool only_use_first_dialog = false;
    bool only_use_first_sentence = true;
    bool play_speech_from_non_friendly_npcs = true;
    bool play_speech_bubbles_in_explorable = false;
    bool play_speech_bubbles_in_outpost = true;
    bool play_speech_bubbles_from_party_members = false;
    float npc_speech_bubble_range = GW::Constants::Range::Earshot;
    bool play_speech_from_vendors = true;
    bool play_tts_in_explorable_areas = true;
    bool play_tts_in_outposts = true;

    struct PendingNPCAudio;
    typedef std::string (*GenerateVoiceCallback)(PendingNPCAudio* audio);

    struct DialogueFrameContext {
        GW::UI::UICtlCallback* vtable;
        uint32_t frame_id;
        uint32_t flags;
        uint32_t agent_id;
        float duration;
    };
    static_assert(sizeof(DialogueFrameContext) == 0x14);

    uint32_t last_dialogue_message_agent_id = 0;
    clock_t last_dialogue_message_time = 0;

    DialogueFrameContext* GetCurrentlyShowingDialogue()
    {
        struct UIFrameQueue_Context {
            GW::Array<void*> h0000;
            uint32_t showing_frame_id;
            uint32_t count;
        };
        static_assert(sizeof(UIFrameQueue_Context) == 0x18);
        auto frame = GW::UI::GetFrameByLabel(L"NpcDialogue1");
        if (frame && frame->frame_callbacks.size() > 0) {
            const auto context = (UIFrameQueue_Context*)frame->frame_callbacks[1].uictl_context;
            if (context->showing_frame_id) {
                const auto current = GW::UI::GetFrameById(context->showing_frame_id);
                return (DialogueFrameContext*)GW::UI::GetFrameContext(current);
            }
        }
        return nullptr;
    }

    // API Configuration structure
    struct APIConfig {
        GenerateVoiceCallback callback;
        const char* name;
        const char* signup_url;
        const char* note = nullptr;
        bool has_user_id = false;
        char api_key[128] = {0};
        char user_id[128] = {0};
    };

    GW::Constants::Language GetAudioLanguage()
    {
        return (GW::Constants::Language)GW::UI::GetPreference(GW::UI::NumberPreference::LanguageAudio);
    }

    enum class GWRace : uint8_t { Human, Charr, Norn, Asura, Tengu, Dwarf, Centaur, Count };

    std::map<GWRace, bool> play_speech_from_race;

    std::string GenerateVoiceOpenAI(PendingNPCAudio*);
    std::string GenerateVoiceElevenLabs(PendingNPCAudio*);
    std::string GenerateVoiceGoogle(PendingNPCAudio*);
    std::string GenerateVoicePlayHT(PendingNPCAudio*);
    std::string GenerateVoiceGWDevHub(PendingNPCAudio*);
    std::string GenerateVoiceKokoro(PendingNPCAudio*);

    APIConfig api_configs[] = {
        {GenerateVoiceGWDevHub, "GWDevHub TTS", "", "This is a free TTS service specifically for Guild Wars players, provided by GWDevHub"},
        {GenerateVoiceElevenLabs, "ElevenLabs", "https://elevenlabs.io/app/settings/api-keys"},
        {GenerateVoiceOpenAI, "OpenAI", "https://platform.openai.com/api-keys"},
        {GenerateVoiceGoogle, "Google Cloud", "https://console.cloud.google.com/apis/credentials", "Note: Make sure to enable the Text-to-Speech API in your Google Cloud project"},
        {GenerateVoicePlayHT, "Play.ht", "https://elevenlabs.io/app/settings/api-keys", "Note: Play.ht requires both an API Key and User ID", true},
        {GenerateVoiceKokoro, "Kokoro (Self-Hosted)", "https://github.com/remsky/Kokoro-FastAPI", "Enter your Kokoro-FastAPI server URL in the API Key field (default: http://localhost:8880)"},
    };

    std::string current_tts_provider = api_configs[0].name;

    APIConfig* GetCurrentAPIConfig()
    {
        const auto found = std::ranges::find(api_configs, current_tts_provider, &APIConfig::name);
        return found != std::end(api_configs) ? &(*found) : nullptr;
    }

    const char* playht_voice_male_default = "s3://voice-cloning-zero-shot/a61556e4-d465-492d-9aac-1daac5f0e7cc/original/manifest.json";
    const char* playht_voice_female_default = "s3://voice-cloning-zero-shot/f28a58a7-269f-4881-bf64-2d9ab025e326/original/manifest.json";

    char custom_npc_id_buffer[32] = {0};
    char custom_voice_id_buffer[256] = {0};

    enum class Gender : uint8_t { Male, Female, Unknown };
    std::map<uint32_t, uint32_t> sound_file_by_model_file_id;

    bool NeedToPreprocessEncodedStr()
    {
        const auto api_config = GetCurrentAPIConfig();
        return api_config && api_config->signup_url[0];
    }

    const char* GetApiKey()
    {
        const auto api_config = GetCurrentAPIConfig();
        return api_config ? api_config->api_key : nullptr;
    }

    uint32_t GetAgentAtPosition(const GW::Vec2f& position, float tolerance = 50.0f)
    {
        const auto agents = GW::Agents::GetAgentArray();
        if (!agents) return 0;

        uint32_t closest_agent_id = 0;
        float closest_distance = tolerance;

        for (const auto agent : *agents) {
            if (!agent) continue;
            float dx = agent->pos.x - position.x;
            float dy = agent->pos.y - position.y;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < closest_distance) {
                closest_distance = distance;
                closest_agent_id = agent->agent_id;
            }
        }

        return closest_agent_id;
    }

    bool GetDeathSoundForModelFileId(uint32_t file_id, uint32_t* file_id_out)
    {
        ArenaNetFileParser::ArenaNetFile asset;
        if (!asset.readFromDat(file_id)) return false;

        auto animations_chunk = (ArenaNetFileParser::FileNamesChunk*)asset.FindChunk(ArenaNetFileParser::ChunkType::BBC_FileReferences);
        if (!animations_chunk) {
            animations_chunk = (ArenaNetFileParser::FileNamesChunk*)asset.FindChunk(ArenaNetFileParser::ChunkType::BBD_AnimationRefs);
            if (!(animations_chunk && asset.readFromDat(animations_chunk->filenames[0].filename))) return false;
            animations_chunk = (ArenaNetFileParser::FileNamesChunk*)asset.FindChunk(ArenaNetFileParser::ChunkType::BBC_FileReferences);
        }
        if (!(animations_chunk && asset.readFromDat(animations_chunk->filenames[0].filename))) return false;
        if (asset.getFFNAType() != 8) return false;
        const auto soundtracks_chunk = (ArenaNetFileParser::FileNamesChunkWithoutLength*)asset.FindChunk(ArenaNetFileParser::ChunkType::Type8_AssetRefs);
        if (!(soundtracks_chunk && soundtracks_chunk->num_filenames() > 0)) return false;
        *file_id_out = ArenaNetFileParser::FileHashToFileId(soundtracks_chunk->filenames[0].filename);
        return true;
    }

    uint32_t cached_dialog_volume = 0xff;
    uint32_t GetDialogVolume(bool cache = true)
    {
        if (cache && cached_dialog_volume != 0xff) return cached_dialog_volume;
        const auto d1 = GW::UI::GetPreference(GW::UI::NumberPreference::VolDialog);
        const auto d2 = GW::UI::GetPreference(GW::UI::NumberPreference::VolMaster);
        return cached_dialog_volume = std::min(d1, d2), cached_dialog_volume;
    }

    float cached_system_volume = 1.f;
    clock_t last_cached_system_volume = 0;
    float GetSystemVolume(bool cache = true)
    {
        if (cache && TIMER_DIFF(last_cached_system_volume) < 10000) return cached_system_volume;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool needs_uninit = SUCCEEDED(hr);
        if (hr == RPC_E_CHANGED_MODE) {
            needs_uninit = false;
            hr = S_OK;
        }
        else if (hr == S_FALSE) {
            needs_uninit = false;
            hr = S_OK;
        }

        IMMDeviceEnumerator* deviceEnumerator = nullptr;
        IMMDevice* defaultDevice = nullptr;
        IAudioEndpointVolume* endpointVolume = nullptr;
        float systemVolume = 0.f;
        BOOL isMuted = FALSE;

        do {
            if (FAILED(hr)) break;
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
            if (FAILED(hr)) break;
            hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
            if (FAILED(hr)) break;
            hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&endpointVolume);
            if (FAILED(hr)) break;
            hr = endpointVolume->GetMute(&isMuted);
            if (FAILED(hr)) isMuted = FALSE;
            hr = endpointVolume->GetMasterVolumeLevelScalar(&systemVolume);
            if (FAILED(hr)) systemVolume = 1.0f;
        } while (false);

        if (endpointVolume) endpointVolume->Release();
        if (defaultDevice) defaultDevice->Release();
        if (deviceEnumerator) deviceEnumerator->Release();
        if (needs_uninit) CoUninitialize();
        if (isMuted) systemVolume = 0.f;

        last_cached_system_volume = TIMER_INIT();
        return cached_system_volume = systemVolume, cached_system_volume;
    }

    GW::NPC* GetAgentAsNPC(uint32_t agent_id)
    {
        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!(agent && agent->GetIsLivingType())) return 0;
        if (!agent->IsNPC()) return 0;
        return GW::Agents::GetNPCByID(agent->player_number);
    }

    uint32_t GetDescriptiveModelFileId(uint32_t agent_id)
    {
        const auto npc = GetAgentAsNPC(agent_id);
        if (!npc) return 0;
        if (!sound_file_by_model_file_id.contains(npc->model_file_id)) {
            uint32_t file_id = 0;
            GetDeathSoundForModelFileId(npc->model_file_id, &file_id);
            sound_file_by_model_file_id[npc->model_file_id] = file_id;
        }
        if (!sound_file_by_model_file_id[npc->model_file_id]) {
            return npc->files_count > 0 ? npc->model_files[0] : 0;
        }
        return sound_file_by_model_file_id[npc->model_file_id];
    }

Gender GetGenderByFileId(const uint32_t file_id)
    {
        switch (file_id) {
            case 0x4f19:
            case 0x6ef5:
            case 0x8b56:
            case 0x12b3d:
            case 0x13e25:
            case 0x13eaa:
            case 0x13ef3:
            case 0x13f49:
            case 0x13f6f:
            case 0x13fdb:
            case 0x16dfc:
            case 0x17390:
            case 0x2d145:
            case 0x2f15d:
            case 0x2f1a1:
            case 0x37614:
            case 0x37794:
            case 0x4a4a8:
                return Gender::Male;
            case 0x97fa:
            case 0x13e4f:
            case 0x13ece:
            case 0x13f22:
            case 0x13f93:
            case 0x16dcf:
            case 0x203e4:
            case 0x2f17e:
            case 0x4541c:
            case 0x4c47a:
                return Gender::Female;
            case 0x4c29e:
                return Gender::Unknown;
        }
        return Gender::Unknown;
    }

    GWRace GetRaceByFileId(const uint32_t file_id)
    {
        switch (file_id) {
            case 0x8b56:
            case 0x17390:
                return GWRace::Dwarf;
            case 0x4c29e:
                return GWRace::Asura;
            case 0x4a4a8:
            case 0x4c47a:
                return GWRace::Norn;
            case 0x12b3d:
                return GWRace::Centaur;
            case 0x6ef5:
            case 0x4f19:
                return GWRace::Charr;
        }
        return GWRace::Human;
    }

    const char* GetRaceName(GWRace race)
    {
        switch (race) {
            case GWRace::Human:
                return "Human";
            case GWRace::Charr:
                return "Charr";
            case GWRace::Norn:
                return "Norn";
            case GWRace::Asura:
                return "Asura";
            case GWRace::Tengu:
                return "Tengu";
            case GWRace::Dwarf:
                return "Dwarf";
            case GWRace::Centaur:
                return "Centaur";
            default:
                return "Unknown";
        }
    }

    GWRace GetAgentRace(uint32_t agent_id)
    {
        return GetRaceByFileId(GetDescriptiveModelFileId(agent_id));
    }
    Gender GetAgentGender(uint32_t agent_id)
    {
        if (GetAgentRace(agent_id) == GWRace::Asura) {
            const auto npc = GetAgentAsNPC(agent_id);
            if (!npc) return Gender::Unknown;
            return (npc->appearance & 0x1) == 1 ? Gender::Female : Gender::Male;
        }
        return GetGenderByFileId(GetDescriptiveModelFileId(agent_id));
    }

    enum class TraderType : uint8_t { Merchant, RuneTrader, ArmorCrafter, WeaponCustomizer, MaterialTrader, RareMaterialTrader, DyeTrader, OtherItemCrafter, SkillTrainer };

    const wchar_t* generic_goodbye_messages[] = {L"Farewell, traveler.",           L"Safe travels on your journey.", L"May your path be clear.", L"Until we meet again.", L"Go well, adventurer.",         L"Good luck on your quest.",
                                                 L"May fortune favor you.",        L"Travel safely, friend.",        L"Goodbye, and take care.", L"Fare thee well.",      L"May the gods watch over you.", L"Be safe out there.",
                                                 L"Good journey to you.",          L"May your travels be swift.",    L"Walk in safety.",         L"Go with my blessing.", L"May peace be with you.",       L"Safe passage, hero.",
                                                 L"Good fortune on your travels.", L"May your way be protected."};

    std::map<std::tuple<GW::Region, TraderType>, std::wstring> merchant_greetings;

    std::deque<std::string> voice_log_messages;
    const size_t MAX_LOG_MESSAGES = 5;

    void VoiceLog(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        voice_log_messages.push_front(std::string(buffer));
        while (voice_log_messages.size() > MAX_LOG_MESSAGES)
            voice_log_messages.pop_back();
        Log::Log("%s", buffer);
    }

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
                return "zh";
            case GW::Constants::Language::Japanese:
                return "ja";
            case GW::Constants::Language::Polish:
                return "pl";
            case GW::Constants::Language::Russian:
                return "ru";
            case GW::Constants::Language::BorkBorkBork:
                return "sv";
            default:
                return "en";
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
        if (trader_name.find(L"skill") != std::wstring::npos) return TraderType::SkillTrainer;
        return TraderType::OtherItemCrafter;
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

    std::map<std::tuple<Gender, GWRace, GW::Region>, VoiceProfile> voice_matrix;
    std::map<uint32_t, VoiceProfile> special_npc_voices;

    uint32_t npc_ids_to_ignore[] = {1991}; // Durmand

    uint32_t last_dialog_agent_id = 0;
    size_t max_text_length = 512;
    VoiceProfile default_voice_profile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f, "");

    std::wstring PreprocessEncodedTextForTTS(const std::wstring& text);
    VoiceProfile* GetVoiceProfile(uint32_t agent_id, GW::Constants::MapID map_id);

    // -------------------------------------------------------------------------
    // PendingNPCAudio — shared_ptr managed, safe across threads
    // -------------------------------------------------------------------------
    struct PendingNPCAudio : std::enable_shared_from_this<PendingNPCAudio> {
        GW::Constants::Language language = GW::Constants::Language::English;
        std::wstring encoded_message;
        std::wstring encoded_npc_name;
        std::wstring decoded_message;
        Gender gender;
        GWRace race;
        VoiceProfile* profile = nullptr;
        uint32_t agent_id = 0;
        uint32_t model_file_id = 0;
        std::filesystem::path path;
        clock_t started = 0;
        clock_t duration = 0;
        bool is_dialog_window = false;
        void* gw_handle = nullptr;

        bool IsPlaying() const { return duration == 0 || TIMER_DIFF(started) < duration; }

        void Play();
        void Stop();

        // Factory — always construct via this so shared_ptr is established before
        // the object touches pending_audio.
        static std::shared_ptr<PendingNPCAudio> Create(uint32_t agent_id, const wchar_t* message, bool is_dialog_window = false);

        ~PendingNPCAudio();

    private:
        // Private constructor — use Create()
        PendingNPCAudio(uint32_t _agent_id, const wchar_t* message, bool _is_dialog_window);
    };

    // -------------------------------------------------------------------------
    // Global audio state
    // -------------------------------------------------------------------------
    std::recursive_mutex playing_audio_mutex;
    std::vector<std::shared_ptr<PendingNPCAudio>> pending_audio;
    std::map<uint32_t, std::shared_ptr<PendingNPCAudio>> playing_audio_map;

    // Atomic so worker thread and game thread can safely read/write without a lock.
    std::atomic<bool> generating_voice{false};

    // -------------------------------------------------------------------------
    // Helpers that work on the shared collections (must hold playing_audio_mutex)
    // -------------------------------------------------------------------------
    bool IsPending(const std::shared_ptr<PendingNPCAudio>& audio)
    {
        return std::ranges::find(pending_audio, audio) != pending_audio.end();
    }

    void RemoveFromPending(const std::shared_ptr<PendingNPCAudio>& audio)
    {
        std::erase(pending_audio, audio);
    }

    clock_t EstimateAudioDuration(const std::filesystem::path& audio_file)
    {
        std::error_code err;
        auto file_size = std::filesystem::file_size(audio_file, err);
        if (err.value() != 0) {
            Log::Error("Failed to get file size for %s: %s", audio_file.string().c_str(), err.message().c_str());
            return 0;
        }
        clock_t duration = static_cast<clock_t>(file_size / 16000.0f) * CLOCKS_PER_SEC;
        return std::max((clock_t)500, duration);
    }

    GW::Vec3f GetAgentVec3f(uint32_t agent_id)
    {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        return agent ? GW::Vec3f(agent->pos.x, agent->pos.y, agent->z) : GW::Vec3f();
    }

    // -------------------------------------------------------------------------
    // PendingNPCAudio implementation
    // -------------------------------------------------------------------------
    PendingNPCAudio::PendingNPCAudio(uint32_t _agent_id, const wchar_t* message, bool _is_dialog_window) : agent_id(_agent_id), started(0), duration(0), is_dialog_window(_is_dialog_window)
    {
        gender = GetAgentGender(_agent_id);
        race = GetAgentRace(_agent_id);
        const auto name_enc = GW::Agents::GetAgentEncName(_agent_id);
        if (name_enc) encoded_npc_name = name_enc;
        model_file_id = GetDescriptiveModelFileId(_agent_id);
        encoded_message = NeedToPreprocessEncodedStr() ? PreprocessEncodedTextForTTS(message) : message;
        profile = GetVoiceProfile(agent_id, GW::Map::GetMapID());
        language = GetAudioLanguage();
        // Note: NOT added to pending_audio here — Create() does that after
        // shared_ptr is established, so shared_from_this() is valid.
    }

    /*static*/ std::shared_ptr<PendingNPCAudio> PendingNPCAudio::Create(uint32_t agent_id, const wchar_t* message, bool is_dialog_window)
    {
        // Use new directly because the constructor is private; make_shared would
        // need friendship or a public constructor.
        std::shared_ptr<PendingNPCAudio> audio(new PendingNPCAudio(agent_id, message, is_dialog_window));
        std::lock_guard lock(playing_audio_mutex);
        pending_audio.push_back(audio);
        return audio;
    }

    PendingNPCAudio::~PendingNPCAudio()
    {
        // Stop playback. The shared_ptr holders in pending_audio / playing_audio_map
        // are responsible for erasing themselves; the destructor just cleans the sound handle.
        Stop();
    }

    void PendingNPCAudio::Stop()
    {
        if (gw_handle) {
            AudioSettings::StopSound(gw_handle);
            gw_handle = nullptr;
        }
    }

    void PendingNPCAudio::Play()
    {
        // --- Phase 1: check for duplicate / prepare under lock ---
        {
            std::lock_guard lock(playing_audio_mutex);

            auto found = playing_audio_map.find(agent_id);
            if (found != playing_audio_map.end()) {
                if (found->second->path == path && found->second->IsPlaying()) return; // Same audio already playing — nothing to do.
                // Different audio for this agent: stop it.
                found->second->Stop();
                playing_audio_map.erase(found);
            }

            if (!duration) duration = EstimateAudioDuration(path);
        }

        // --- Phase 2: call into the audio system WITHOUT holding the lock ---
        // shared_ptr keeps `this` alive even if another thread calls ClearSounds()
        // or CancelDialogSpeech() here — they will Stop() and drop their shared_ptr
        // but our refcount won't hit zero until this scope exits.
        const auto pos = GetAgentVec3f(agent_id);
        VoiceLog("Playing audio file: %s (estimated duration: %dms)", path.filename().string().c_str(), duration);
        const uint32_t flags = is_dialog_window ? 0x4 : 0x1404;
        const bool success = AudioSettings::PlaySound(path.wstring().c_str(), &pos, flags, &gw_handle);

        if (!success) {
            // PlaySound failed; remove ourselves from pending and bail.
            std::lock_guard lock(playing_audio_mutex);
            RemoveFromPending(shared_from_this());
            return;
        }

        // --- Phase 3: register as playing under lock ---
        {
            std::lock_guard lock(playing_audio_mutex);
            started = TIMER_INIT();
            RemoveFromPending(shared_from_this());
            playing_audio_map[agent_id] = shared_from_this();
        }
    }

    // -------------------------------------------------------------------------
    // CancelDialogSpeech / ClearSounds
    // -------------------------------------------------------------------------
    void CancelDialogSpeech(uint32_t agent_id)
    {
        std::lock_guard lock(playing_audio_mutex);
        auto found = playing_audio_map.find(agent_id);
        if (found != playing_audio_map.end()) {
            found->second->Stop();
            playing_audio_map.erase(found);
        }
        std::erase_if(pending_audio, [agent_id](const auto& a) {
            return a->agent_id == agent_id;
        });
    }

    void ClearSounds()
    {
        std::lock_guard lock(playing_audio_mutex);
        for (auto& [id, audio] : playing_audio_map)
            audio->Stop();
        playing_audio_map.clear();
        pending_audio.clear(); // shared_ptrs drop; destructors run safely.
    }

    // -------------------------------------------------------------------------
    // Voice generation helpers
    // -------------------------------------------------------------------------
    std::shared_ptr<PendingNPCAudio> FindAlreadyProcessingAudio(const std::shared_ptr<PendingNPCAudio>& compare)
    {
        if (!compare) return nullptr;
        std::lock_guard lock(playing_audio_mutex);
        for (const auto& audio : pending_audio) {
            if (audio != compare && audio->agent_id == compare->agent_id && audio->encoded_message == compare->encoded_message && audio->IsPlaying()) {
                return audio;
            }
        }
        return nullptr;
    }

    GW::GamePos GetPlayerPosition()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        return player ? player->pos : GW::GamePos();
    }

    std::wstring ExtractFirstSentence(const std::wstring& text)
    {
        if (text.empty()) return L"";

        std::wstring trimmed = text;
        size_t start = trimmed.find_first_not_of(L" \t\n\r");
        if (start == std::wstring::npos) return L"";
        trimmed = trimmed.substr(start);

        std::vector<wchar_t> sentence_endings = {L'.', L'!', L'?', L';', L':'};
        size_t min_pos = std::wstring::npos;

        for (wchar_t ending : sentence_endings) {
            size_t pos = trimmed.find(ending);
            if (pos == std::wstring::npos) continue;
            bool is_sentence_end = true;

            if (ending == L'.' && pos > 0) {
                if (pos >= 2) {
                    std::wstring before = trimmed.substr(pos - 2, 2);
                    if (before == L"Mr" || before == L"Dr" || before == L"Ms" || before == L"St" || before == L"Mt" || before == L"vs") is_sentence_end = false;
                }
                if (pos > 0 && pos < trimmed.length() - 1 && trimmed[pos - 1] != L' ' && trimmed[pos + 1] == L' ' && iswupper(trimmed[pos - 1])) is_sentence_end = false;
                if (pos > 0 && pos < trimmed.length() - 1 && iswdigit(trimmed[pos - 1]) && iswdigit(trimmed[pos + 1])) is_sentence_end = false;
            }

            if (is_sentence_end && (min_pos == std::wstring::npos || pos < min_pos)) min_pos = pos;
        }

        if (min_pos == std::wstring::npos) {
            if (trimmed.length() > max_text_length) {
                trimmed = trimmed.substr(0, max_text_length);
                auto last_space = trimmed.find_last_of(L' ');
                if (last_space != std::wstring::npos && last_space > max_text_length * 0.7) trimmed = trimmed.substr(0, last_space);
                trimmed += L"...";
            }
            return trimmed;
        }

        std::wstring first_sentence = trimmed.substr(0, min_pos + 1);
        size_t end = first_sentence.find_last_not_of(L" \t\n\r");
        if (end != std::wstring::npos) first_sentence = first_sentence.substr(0, end + 1);
        return first_sentence;
    }

    std::wstring PreprocessEncodedTextForTTS(const std::wstring& text)
    {
        auto result = TextUtils::ctre_regex_replace<
            L"\x0ba9\x0107[^\x0001]+\x0001", L"\x0ba9\x0107"
                                             "Chosen\x0001">(text);
        result = TextUtils::ctre_regex_replace<L"[\x0101\x102\x103\x104][\x8100-\xffff]*.", L"">(result);
        return result;
    }

    std::wstring PreprocessTextForTTS(const std::wstring& text)
    {
        std::wstring processed = text;
        processed = TextUtils::ctre_regex_replace<L"<a=[^<]+[^>]+>", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"<br>|<brx>|<p>", L". ">(processed);
        processed = TextUtils::StripTags(processed);
        if (only_use_first_sentence) processed = ExtractFirstSentence(processed);
        if (processed.empty()) return L"";
        processed = TextUtils::ctre_regex_replace<L"[.]{2,}", L".">(processed);
        processed = TextUtils::ctre_regex_replace<L"[!]{2,}", L"!">(processed);
        processed = TextUtils::ctre_regex_replace<L"[?]{2,}", L"?">(processed);
        processed = TextUtils::ctre_regex_replace<L"\\([^)]*\\)", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"\\[[^\\]]*\\]", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"\\s+", L" ">(processed);
        if (processed.length() > max_text_length) {
            processed = processed.substr(0, max_text_length);
            auto last_space = processed.find_last_of(L' ');
            if (last_space != std::wstring::npos && last_space > max_text_length * 0.8) processed = processed.substr(0, last_space) + L"...";
        }
        if (TextUtils::RemovePunctuation(processed).empty()) return L"";
        return processed;
    }

    GW::Region GetRegionFromMapID(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        return map_info->region;
    }

    VoiceProfile* GetVoiceProfile(uint32_t agent_id, GW::Constants::MapID map_id)
    {
        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!agent) return nullptr;
        const wchar_t* name = GW::Agents::GetAgentEncName(agent);
        if (!(name && *name && agent->GetIsLivingType() && agent->IsNPC())) return nullptr;
        if (!play_speech_from_non_friendly_npcs && agent->allegiance == GW::Constants::Allegiance::Enemy) return nullptr;

        auto special_it = special_npc_voices.find(agent->player_number);
        if (special_it != special_npc_voices.end()) return &special_it->second;

        const auto race = GetAgentRace(agent->agent_id);
        const auto gender = GetAgentGender(agent->agent_id);
        if (gender == Gender::Unknown) return nullptr;

        const auto region = GetRegionFromMapID(map_id);

        auto exact_it = voice_matrix.find({gender, race, region});
        if (exact_it != voice_matrix.end()) return &exact_it->second;

        auto by_race = voice_matrix.find({gender, race, GW::Region::Region_DevRegion});
        if (by_race != voice_matrix.end()) return &by_race->second;

        auto by_gender = voice_matrix.find({gender, GWRace::Human, GW::Region::Region_DevRegion});
        if (by_gender != voice_matrix.end()) return &by_gender->second;

        return nullptr;
    }

    GW::HookEntry UIMessage_HookEntry;
    GW::HookEntry PreUIMessage_HookEntry;

    // Forward declaration
    void GenerateVoice(std::shared_ptr<PendingNPCAudio> audio);

    void GenerateVoiceFromDecodedString(std::shared_ptr<PendingNPCAudio> audio)
    {
        if (!(audio && !audio->decoded_message.empty())) return; // shared_ptr drops, destructor handles cleanup
        GenerateVoice(std::move(audio));
    }

    void GetNPCName(uint32_t agent_id, GW::UI::DecodeStr_Callback callback, void* param = nullptr)
    {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        const auto name = agent ? GW::Agents::GetAgentEncName(agent) : nullptr;
        if (!(name && *name)) {
            callback(param, L"");
            return;
        }
        GW::UI::AsyncDecodeStr(name, callback, param, GW::Constants::Language::English);
    }

    void GenerateVoiceFromEncodedString(std::shared_ptr<PendingNPCAudio> audio)
    {
        if (!GetApiKey()) return; // shared_ptr drops
        if (!(audio && !audio->encoded_message.empty() && audio->profile)) return;
        if (FindAlreadyProcessingAudio(audio)) return;

        // Keep audio alive across the async decode by storing in a raw context pointer.
        // We use a new shared_ptr on the heap so the refcount stays elevated.
        auto* ctx = new std::shared_ptr<PendingNPCAudio>(audio);

        GW::UI::AsyncDecodeStr(
            audio->encoded_message.c_str(),
            [](void* param, const wchar_t* s) {
                auto* ctx = static_cast<std::shared_ptr<PendingNPCAudio>*>(param);
                auto audio = std::move(*ctx);
                delete ctx;
                audio->decoded_message = PreprocessTextForTTS(s);
                GenerateVoiceFromDecodedString(std::move(audio));
            },
            ctx, audio->language
        );
    }

    GW::UI::Frame* dialog_frame = nullptr;

    float GetDistanceFromAgentId(uint32_t agent_id)
    {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!agent) return FLT_MAX;
        return GW::GetDistance(agent->pos, GetPlayerPosition());
    }

    GW::UI::UIInteractionCallback OnNPCInteract_UICallback_Func = nullptr, OnNPCInteract_UICallback_Ret = nullptr;
    GW::UI::UIInteractionCallback OnVendorInteract_UICallback_Func = nullptr, OnVendorInteract_UICallback_Ret = nullptr;

    bool was_dialog_already_open = false;

    void OnNPCDialogClosed()
    {
        if (stop_speech_when_dialog_closed) CancelDialogSpeech(last_dialog_agent_id);

        if (play_goodbye_messages && !was_dialog_already_open && GetDistanceFromAgentId(last_dialog_agent_id) < GW::Constants::Range::Adjacent) {
            if (GetAudioLanguage() != GW::Constants::Language::English) return;
            const auto num_goodbye_messages = sizeof(generic_goodbye_messages) / sizeof(generic_goodbye_messages[0]);
            const wchar_t* goodbye_msg = generic_goodbye_messages[rand() % num_goodbye_messages];

            auto audio = PendingNPCAudio::Create(last_dialog_agent_id, L"", true);
            audio->decoded_message = PreprocessTextForTTS(goodbye_msg);
            audio->profile = GetVoiceProfile(last_dialog_agent_id, GW::Map::GetMapID());
            if (audio->profile) GenerateVoiceFromDecodedString(std::move(audio));
            // else shared_ptr drops, object cleans itself up
        }
    }

    void OnNPCInteract_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        OnNPCInteract_UICallback_Ret(message, wParam, lParam);
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) OnNPCDialogClosed();
        GW::Hook::LeaveHook();
    }

    void OnVendorInteract_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        OnVendorInteract_UICallback_Ret(message, wParam, lParam);
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) OnNPCDialogClosed();
        GW::Hook::LeaveHook();
    }

    void HookNPCInteractFrame()
    {
        if (!OnNPCInteract_UICallback_Func) {
            const auto frame = GW::UI::GetFrameByLabel(L"NPCInteract");
            if (frame && frame->frame_callbacks.size()) {
                OnNPCInteract_UICallback_Func = frame->frame_callbacks[0].callback;
                GW::Hook::CreateHook((void**)&OnNPCInteract_UICallback_Func, OnNPCInteract_UICallback, (void**)&OnNPCInteract_UICallback_Ret);
                GW::Hook::EnableHooks(OnNPCInteract_UICallback_Func);
            }
        }
        if (!OnVendorInteract_UICallback_Func) {
            const auto vendor_frame = GW::UI::GetFrameByLabel(L"Vendor");
            if (vendor_frame && vendor_frame->frame_callbacks.size()) {
                OnVendorInteract_UICallback_Func = vendor_frame->frame_callbacks[0].callback;
                GW::Hook::CreateHook((void**)&OnVendorInteract_UICallback_Func, OnVendorInteract_UICallback, (void**)&OnVendorInteract_UICallback_Ret);
                GW::Hook::EnableHooks(OnVendorInteract_UICallback_Func);
            }
        }
    }

    void UnHookNPCInteractFrame()
    {
        if (OnNPCInteract_UICallback_Func) {
            GW::Hook::RemoveHook(OnNPCInteract_UICallback_Func);
            OnNPCInteract_UICallback_Func = nullptr;
        }
        if (OnVendorInteract_UICallback_Func) {
            GW::Hook::RemoveHook(OnVendorInteract_UICallback_Func);
            OnVendorInteract_UICallback_Func = nullptr;
        }
    }

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (msgid) {
            case GW::UI::UIMessage::kDialogBody: {
                const auto packet = (GW::UI::DialogBodyInfo*)wParam;
                if (!(packet && packet->message_enc && *packet->message_enc)) return;
                was_dialog_already_open = GW::UI::GetFrameByLabel(L"NPCInteract") && packet->agent_id == last_dialog_agent_id;
                last_dialog_agent_id = packet->agent_id;
                CancelDialogSpeech(last_dialog_agent_id);
                if (only_use_first_dialog && was_dialog_already_open) return;
                GenerateVoiceFromEncodedString(PendingNPCAudio::Create(packet->agent_id, packet->message_enc, true));
            } break;
        }
    }

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (msgid) {
            case GW::UI::UIMessage::kDialogBody: {
                HookNPCInteractFrame();
                was_dialog_already_open = false;
            } break;
            case GW::UI::UIMessage::kPreferenceValueChanged: {
                GetDialogVolume(false);
            } break;
            case GW::UI::UIMessage::kDialogueMessageUpdated: {
                GW::GameThread::Enqueue([]() {
                    const auto current = GetCurrentlyShowingDialogue();
                    if (!current) return;
                    const auto frame = GW::UI::GetFrameById(current->frame_id);
                    const auto message_frame = (GW::MultiLineTextLabelFrame*)GW::UI::GetChildFrame(frame, 1);
                    const auto message_enc = message_frame ? message_frame->GetEncodedLabel() : nullptr;
                    if (!(message_enc && current->agent_id)) return;
                    if (TIMER_DIFF(last_dialogue_message_time) < 2000 && current->agent_id == last_dialogue_message_agent_id) return;
                    CancelDialogSpeech(current->agent_id);
                    Log::Log("GenerateVoiceFromEncodedString for %d (Dialogue Message)", current->agent_id);
                    GenerateVoiceFromEncodedString(PendingNPCAudio::Create(current->agent_id, message_enc, true));
                    last_dialogue_message_time = TIMER_INIT();
                    last_dialogue_message_agent_id = current->agent_id;
                });
            } break;
            case GW::UI::UIMessage::kMapChange:
            case GW::UI::UIMessage::kMapLoaded: {
                ClearSounds();
            } break;
            case GW::UI::UIMessage::kVendorWindow: {
                HookNPCInteractFrame();
                const auto packet = (GW::UI::UIPacket::kVendorWindow*)wParam;
                last_dialog_agent_id = packet->unk;
                switch (packet->transaction_type) {
                    case GW::Merchant::TransactionType::CollectorBuy: {
                        const auto collector_dialog = (GW::MultiLineTextLabelFrame*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Vendor"), 0, 0, 2);
                        const auto enc_text = collector_dialog ? collector_dialog->GetEncodedLabel() : nullptr;
                        if (enc_text && *enc_text) GenerateVoiceFromEncodedString(PendingNPCAudio::Create(packet->unk, enc_text, true));
                    } break;
                    case GW::Merchant::TransactionType::DonateFaction: {
                        const auto collector_dialog = (GW::MultiLineTextLabelFrame*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"NPCInteract"), 0, 0, 0);
                        const auto enc_text = collector_dialog ? collector_dialog->GetEncodedLabel() : nullptr;
                        if (enc_text && *enc_text) GenerateVoiceFromEncodedString(PendingNPCAudio::Create(packet->unk, enc_text, true));
                    } break;
                    case GW::Merchant::TransactionType::SkillTrainer:
                    case GW::Merchant::TransactionType::MerchantBuy:
                    case GW::Merchant::TransactionType::CrafterBuy:
                    case GW::Merchant::TransactionType::WeaponsmithCustomize:
                    case GW::Merchant::TransactionType::TraderBuy: {
                        if (GetAudioLanguage() != GW::Constants::Language::English) return;
                        if (!play_speech_from_vendors) return;
                        auto audio = PendingNPCAudio::Create(packet->unk, L"", true);
                        // Keep audio alive across the async name lookup via shared_ptr on heap.
                        auto* ctx = new std::shared_ptr<PendingNPCAudio>(audio);
                        GetNPCName(
                            packet->unk,
                            [](void* param, const wchar_t* s) {
                                auto* ctx = static_cast<std::shared_ptr<PendingNPCAudio>*>(param);
                                auto audio = std::move(*ctx);
                                delete ctx;
                                const auto region = GetRegionFromMapID(GW::Map::GetMapID());
                                auto key = std::make_tuple(region, GetTraderType(s));
                                auto it = merchant_greetings.find(key);
                                if (it != merchant_greetings.end() && !it->second.empty()) {
                                    audio->decoded_message = PreprocessTextForTTS(it->second);
                                    GenerateVoiceFromDecodedString(std::move(audio));
                                }
                                // else shared_ptr drops, cleanup automatic
                            },
                            ctx
                        );
                    } break;
                }
            } break;
        }
    }

    Gender GetPlayerGender()
    {
        const auto agent = GW::Agents::GetControlledCharacter();
        return agent && agent->GetIsFemale() ? Gender::Female : Gender::Male;
    }

    std::string GenerateOptimizedCacheKey(const PendingNPCAudio* p)
    {
        auto text_hash = std::hash<std::wstring>{}(p->decoded_message);
        return std::format("{}_{}_{}_{:x}_{}.mp3", (uint8_t)p->race, (uint8_t)p->gender, (uint32_t)p->language, text_hash, p->decoded_message.size());
    }

    size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
    {
        size_t total_size = size * nmemb;
        userp->append((char*)contents, total_size);
        return total_size;
    }

    std::string PostJson(RestClient& client, const std::string& url, const nlohmann::json& request_body, const std::string& service_name = "API")
    {
        client.SetUrl(url.c_str());
        client.SetHeader("Content-Type", "application/json");
        client.SetPostContent(request_body.dump(), ContentFlag::Copy);
        client.SetFollowLocation(true);
        client.SetVerifyHost(false);
        client.SetVerifyPeer(false);
        client.SetTimeoutSec(2);
        client.Execute();
        if (!client.IsSuccessful()) {
            VoiceLog("%s returned error code: %ld", service_name.c_str(), client.GetStatusCode());
            std::string error_response = std::move(client.GetContent());
            if (!error_response.empty()) VoiceLog("Error response: %s", error_response.c_str());
            return "";
        }
        return std::move(client.GetContent());
    }

    std::string GenerateVoiceOpenAI(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("No Audio Profile"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("No API Key"), "";

        nlohmann::json request_body;
        request_body["model"] = "gpt-4o-mini-tts";
        request_body["input"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"] = (audio->profile->voice_id == voice_id_human_female) ? "nova" : "onyx";
        request_body["response_format"] = "mp3";
        request_body["speed"] = audio->profile->speaking_rate;
        request_body["language"] = LanguageToAbbreviation(audio->language);

        RestClient client;
        client.SetHeader("Authorization", ("Bearer " + std::string(api_config->api_key)).c_str());
        const auto audio_data = PostJson(client, "https://api.openai.com/v1/audio/speech", request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("OpenAI voice generation successful, received %zu bytes", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceElevenLabs(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("No Audio Profile"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("No API Key"), "";

        nlohmann::json voice_settings;
        voice_settings["stability"] = audio->profile->stability;
        voice_settings["similarity_boost"] = audio->profile->similarity;
        voice_settings["style"] = audio->profile->style;
        voice_settings["use_speaker_boost"] = true;

        nlohmann::json request_body;
        request_body["text"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["model_id"] = "eleven_flash_v2_5";
        request_body["voice_settings"] = voice_settings;
        request_body["language"] = LanguageToAbbreviation(audio->language);

        RestClient client;
        client.SetHeader("xi-api-key", api_config->api_key);
        client.SetHeader("Accept", "audio/mpeg");
        const auto audio_data = PostJson(client, "https://api.elevenlabs.io/v1/text-to-speech/" + audio->profile->voice_id, request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("ElevenLabs voice generation successful, received %zu bytes", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceGWDevHub(PendingNPCAudio* audio)
    {
        nlohmann::json encoded_array = nlohmann::json::array();
        for (const auto& c : audio->encoded_message)
            encoded_array.push_back(static_cast<uint32_t>(c));

        nlohmann::json decoded_array = nlohmann::json::array();
        for (const auto& c : audio->decoded_message)
            decoded_array.push_back(static_cast<uint32_t>(c));

        nlohmann::json request_body = nlohmann::json::object();
        request_body["encoded"] = encoded_array;
        request_body["decoded"] = decoded_array;

        if (!audio->encoded_npc_name.empty()) {
            nlohmann::json encoded_npc_name_arr = nlohmann::json::array();
            for (const auto& c : audio->encoded_npc_name)
                encoded_npc_name_arr.push_back(static_cast<uint32_t>(c));
            request_body["npc_name"] = encoded_npc_name_arr;
        }

        request_body["language"] = static_cast<uint32_t>(audio->language);
        request_body["speaker_gender"] = audio->gender == Gender::Male ? "m" : "f";
        request_body["speaker_race"] = GetRaceName(GetRaceByFileId(audio->model_file_id));
        request_body["player_gender"] = GetPlayerGender() == Gender::Male ? "m" : "f";

        RestClient client;
        client.SetHeader("Accept", "audio/mpeg");
        const auto audio_data = PostJson(client, std::format("{}/decode.mp3", gwtts_hostname), request_body, "GWDevHub");
        if (!audio_data.empty()) VoiceLog("GWDevHub voice generation successful, received %zu bytes", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceKokoro(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("No Audio Profile"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!api_config) return VoiceLog("No API config"), "";

        std::string base_url = *api_config->api_key ? api_config->api_key : "http://localhost:8880";
        if (!base_url.empty() && base_url.back() == '/') base_url.pop_back();

        std::string lang_code;
        switch (audio->language) {
            case GW::Constants::Language::English:
                lang_code = "a";
                break;
            case GW::Constants::Language::Spanish:
                lang_code = "e";
                break;
            case GW::Constants::Language::French:
                lang_code = "f";
                break;
            case GW::Constants::Language::Italian:
                lang_code = "i";
                break;
            case GW::Constants::Language::Japanese:
                lang_code = "j";
                break;
            case GW::Constants::Language::TraditionalChinese:
                lang_code = "z";
                break;
            default:
                lang_code = "a";
                break;
        }

        nlohmann::json request_body;
        request_body["model"] = "kokoro";
        request_body["input"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"] = (audio->gender == Gender::Female) ? "af_bella" : "am_adam";
        request_body["response_format"] = "mp3";
        request_body["speed"] = audio->profile->speaking_rate;
        request_body["lang_code"] = lang_code;

        RestClient client;
        const auto audio_data = PostJson(client, base_url + "/v1/audio/speech", request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("Kokoro voice generation successful, received %zu bytes", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceGoogle(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("No Audio Profile"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("No API Key"), "";

        const std::string voice_name = (audio->profile->voice_id == voice_id_human_female) ? "en-US-Studio-O" : "en-US-Studio-Q";

        nlohmann::json request_body = nlohmann::json::object();
        request_body["input"]["text"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"]["name"] = voice_name;
        request_body["voice"]["languageCode"] = "en-US";
        request_body["audioConfig"]["audioEncoding"] = "MP3";
        request_body["audioConfig"]["speakingRate"] = audio->profile->speaking_rate;
        request_body["audioConfig"]["pitch"] = 0.0f;

        RestClient client;
        const auto response_str = PostJson(client, "https://texttospeech.googleapis.com/v1/text:synthesize?key=" + std::string(api_config->api_key), request_body, api_config->name);
        if (response_str.empty()) return response_str;

        const auto json_response = nlohmann::json::parse(response_str, nullptr, false);
        if (!(!json_response.is_discarded() && json_response.contains("audioContent") && json_response["audioContent"].is_string())) {
            VoiceLog("Failed to parse Google TTS response JSON");
            return "";
        }

        std::string base64_audio = json_response["audioContent"].get<std::string>();
        if (base64_audio.empty()) {
            VoiceLog("Google TTS returned empty audio content");
            return "";
        }

        std::string audio_data = TextUtils::Base64Decode(base64_audio);
        VoiceLog("Google voice generation successful, decoded %zu bytes", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoicePlayHT(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("No Audio Profile"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("No API Key"), "";
        if (!*api_config->user_id) return VoiceLog("No User ID"), "";

        const std::string voice_id = (GetAgentGender(audio->agent_id) == Gender::Female) ? playht_voice_female_default : playht_voice_male_default;
        const std::string lang_code = LanguageToAbbreviation(audio->language);

        nlohmann::json request_body = nlohmann::json::object();
        request_body["text"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["output_format"] = "mp3";
        request_body["quality"] = "medium";
        request_body["speed"] = audio->profile->speaking_rate;
        request_body["voice"] = voice_id;
        request_body["voice_engine"] = (lang_code == "en") ? "PlayHT2.0-turbo" : "PlayHT2.0";

        RestClient client;
        client.SetHeader("Authorization", ("Bearer " + std::string(api_config->api_key)).c_str());
        client.SetHeader("X-User-ID", api_config->user_id);
        client.SetHeader("Accept", "audio/mpeg");
        const auto audio_data = PostJson(client, "https://api.play.ht/api/v2/tts", request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("Play.ht voice generation successful, received %zu bytes", audio_data.size());
        return audio_data;
    }

    // -------------------------------------------------------------------------
    // GenerateVoice — the main dispatch function
    //
    // Takes shared ownership of `audio`. The worker lambda captures a weak_ptr so
    // that if the audio is cancelled (ClearSounds / CancelDialogSpeech) while the
    // worker is running, the lock() returns null and we bail out cleanly without
    // touching freed memory.
    // -------------------------------------------------------------------------
    void GenerateVoice(std::shared_ptr<PendingNPCAudio> audio)
    {
        if (!audio) return;

        // Only one voice generation at a time. If we're busy, discard.
        bool expected = false;
        if (!generating_voice.compare_exchange_strong(expected, true)) return; // shared_ptr drops, pending_audio still holds a ref

        // --- Early-exit checks (game-thread side, before handing off to worker) ---
        auto bail = [&]() {
            // Remove from pending so it's not left dangling, then release.
            {
                std::lock_guard lock(playing_audio_mutex);
                RemoveFromPending(audio);
            }
            generating_voice = false;
        };

        if (GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) {
            VoiceLog("Guild Wars not in focus");
            return bail();
        }
        if (audio->gender == Gender::Unknown) {
            VoiceLog("Unknown Gender");
            return bail();
        }
        if (!play_speech_from_race[audio->race]) {
            VoiceLog("Blocked race %s", GetRaceName(audio->race));
            return bail();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && !play_tts_in_outposts) {
            VoiceLog("Blocked TTS in outposts");
            return bail();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && !play_tts_in_explorable_areas) {
            VoiceLog("Blocked TTS in explorable areas");
            return bail();
        }
        if (GetDialogVolume() * GetSystemVolume() == 0.f) {
            VoiceLog("Dialog volume is 0");
            return bail();
        }

        // --- Hand off to worker thread ---
        // Capture a weak_ptr; if audio is cancelled while we're waiting for the
        // network call, lock() will return nullptr and we bail safely.
        std::weak_ptr<PendingNPCAudio> weak_audio = audio;
        // Release our strong ref now — pending_audio still holds one.
        audio.reset();

        Resources::EnqueueWorkerTask([weak_audio]() {
            // Try to acquire strong ownership.
            auto audio = weak_audio.lock();
            if (!audio) {
                generating_voice = false;
                return;
            }

            // Confirm still in pending_audio (not cancelled between enqueue and execution).
            {
                std::lock_guard lock(playing_audio_mutex);
                if (!IsPending(audio)) {
                    generating_voice = false;
                    return;
                }
            }

            const auto api_config = GetCurrentAPIConfig();
            const auto cache_key = GenerateOptimizedCacheKey(audio.get());
            audio->path = Resources::GetPath("NPCVoiceCache") / LanguageToAbbreviation(audio->language) / cache_key;

            // Cache hit — play immediately.
            if (std::filesystem::exists(audio->path)) {
                audio->Play();
                generating_voice = false;
                return;
            }

            // Generate audio data via the selected provider.
            std::string audio_data = api_config ? api_config->callback(audio.get()) : "";

            // Re-check that audio wasn't cancelled during the (potentially slow) API call.
            {
                std::lock_guard lock(playing_audio_mutex);
                if (!IsPending(audio)) {
                    generating_voice = false;
                    return;
                }
            }

            if (audio_data.empty()) {
                VoiceLog("Failed to generate voice data");
                std::lock_guard lock(playing_audio_mutex);
                RemoveFromPending(audio);
                generating_voice = false;
                return;
            }

            if (!Resources::WriteFile(audio->path, audio_data)) {
                VoiceLog("Failed to write audio data to file: %s", TextUtils::WStringToString(audio->path.wstring()).c_str());
                std::lock_guard lock(playing_audio_mutex);
                RemoveFromPending(audio);
                generating_voice = false;
                return;
            }

            // Final pending check before play.
            {
                std::lock_guard lock(playing_audio_mutex);
                if (!IsPending(audio)) {
                    generating_voice = false;
                    return;
                }
            }

            audio->Play();
            generating_voice = false;
        });
    }

    // -------------------------------------------------------------------------
    // OnPlaySound — block in-game NPC voice when we have TTS audio for the agent
    // -------------------------------------------------------------------------
    void OnPlaySound(GW::HookStatus* status, const wchar_t* filename, SoundProps* props)
    {
        if (status->blocked) return;
        if (!(props && (props->flags & 0xffff) == 0x1404)) return;
        if (GW::Map::GetIsInCinematic()) return;
        if (wcslen(filename) > 4) return;
        if (!GetApiKey()) return;

        const auto agent_id = GetAgentAtPosition({props->position.x, props->position.y}, 20.f);
        if (!(agent_id && GetVoiceProfile(agent_id, GW::Map::GetMapID()))) return;

        if (wcslen(filename) < 10) {
            status->blocked = true;
            return;
        }

        std::lock_guard lock(playing_audio_mutex);
        auto found = playing_audio_map.find(agent_id);
        if (found != playing_audio_map.end() && found->second->IsPlaying()) {
            status->blocked = true;
            return;
        }
        const bool has_pending = std::ranges::any_of(pending_audio, [agent_id](const auto& a) {
            return a->agent_id == agent_id;
        });
        if (has_pending) status->blocked = true;
    }

    GW::UI::UIInteractionCallback OnAgentSpeechBubble_UICallback_Func = nullptr, OnAgentSpeechBubble_UICallback_Ret = nullptr;

    void OnAgentSpeechBubble_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        OnAgentSpeechBubble_UICallback_Ret(message, wParam, lParam);
        switch (message->message_id) {
            case GW::UI::UIMessage::kInitFrame: {
                const auto frame = GW::UI::GetFrameById(message->frame_id);
                if (!frame) break;
                uint32_t agent_id = frame->child_offset_id & 0xfffff;
                const auto msg_frame = (GW::TextLabelFrame*)GW::UI::GetChildFrame(frame, 3);
                const auto msg_enc = msg_frame ? msg_frame->GetEncodedLabel() : nullptr;
                if (!msg_enc) break;
                const auto agent = GW::Agents::GetAgentByID(agent_id);
                if (!agent) break;

                if (!play_speech_bubbles_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) break;
                if (!play_speech_bubbles_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) break;
                if (!play_speech_bubbles_from_party_members && GW::PartyMgr::IsAgentInParty(agent_id)) break;
                if (GW::GetDistance(agent->pos, GetPlayerPosition()) > npc_speech_bubble_range) break;

                Log::Log("GenerateVoiceFromEncodedString for %d (Speech Bubble)", agent_id);
                GenerateVoiceFromEncodedString(PendingNPCAudio::Create(agent_id, msg_enc));
            } break;
        }
        GW::Hook::LeaveHook();
    }

} // namespace

// =============================================================================
// TextToSpeechModule
// =============================================================================

void TextToSpeechModule::Update(float delta)
{
    static float last_check = 0.f;
    last_check += delta;
    if (last_check < 1.f) return;
    last_check = 0.f;

    std::lock_guard lock(playing_audio_mutex);
    for (auto it = playing_audio_map.begin(); it != playing_audio_map.end();) {
        if (TIMER_DIFF(it->second->started) > std::max(it->second->duration, (clock_t)10000)) {
            it->second->Stop();
            it = playing_audio_map.erase(it);
        }
        else {
            ++it;
        }
    }
}

void TextToSpeechModule::Initialize()
{
    ToolboxModule::Initialize();

    if (play_speech_from_race.empty()) {
        for (size_t i = 0; i < (size_t)GWRace::Count; i++)
            play_speech_from_race[(GWRace)i] = true;
    }

    voice_matrix.clear();
    special_npc_voices.clear();

    // Human voices by region and gender
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Ascalon}] = VoiceProfile(voice_id_human_male, 0.7f, 0.6f, 0.5f, 0.95f, "gruff");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Ascalon}] = VoiceProfile(voice_id_human_female, 0.6f, 0.7f, 0.6f, 1.0f, "determined");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Kryta}] = VoiceProfile(voice_id_human_male, 0.5f, 0.7f, 0.4f, 1.0f, "refined");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Kryta}] = VoiceProfile(voice_id_human_female, 0.4f, 0.8f, 0.5f, 1.05f, "noble");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Maguuma}] = VoiceProfile(voice_id_human_male, 0.6f, 0.5f, 0.6f, 0.90f, "tribal");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Maguuma}] = VoiceProfile(voice_id_human_female, 0.5f, 0.6f, 0.7f, 0.95f, "wild");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_CrystalDesert}] = VoiceProfile(voice_id_human_male, 0.7f, 0.4f, 0.3f, 0.85f, "mystical");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_CrystalDesert}] = VoiceProfile(voice_id_human_female, 0.6f, 0.5f, 0.4f, 0.90f, "ancient");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_NorthernShiverpeaks}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.4f, 0.80f, "mountain");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_NorthernShiverpeaks}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.5f, 0.85f, "hardy");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_FarShiverpeaks}] = VoiceProfile(voice_id_human_male, 0.9f, 0.2f, 0.3f, 0.75f, "isolated");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_FarShiverpeaks}] = VoiceProfile(voice_id_human_female, 0.8f, 0.3f, 0.4f, 0.80f, "distant");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_ShingJea}] = VoiceProfile(voice_id_human_male, 0.3f, 0.7f, 0.2f, 1.0f, "canthan_scholarly");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_ShingJea}] = VoiceProfile(voice_id_human_female, 0.2f, 0.8f, 0.3f, 1.05f, "canthan_gentle");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Kaineng}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 1.0f, "canthan_urban");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Kaineng}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 1.0f, "canthan_refined");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Kurzick}] = VoiceProfile(voice_id_human_male, 0.5f, 0.5f, 0.4f, 0.95f, "canthan_forest");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Kurzick}] = VoiceProfile(voice_id_human_female, 0.4f, 0.6f, 0.5f, 1.0f, "canthan_mystic");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Luxon}] = VoiceProfile(voice_id_human_male, 0.5f, 0.6f, 0.4f, 0.95f, "canthan_sea");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Luxon}] = VoiceProfile(voice_id_human_female, 0.4f, 0.7f, 0.5f, 1.0f, "canthan_tide");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Istan}] = VoiceProfile(voice_id_human_male, 0.4f, 0.7f, 0.5f, 1.0f, "elonian_island");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Istan}] = VoiceProfile(voice_id_human_female, 0.3f, 0.8f, 0.6f, 1.05f, "elonian_tropical");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Kourna}] = VoiceProfile(voice_id_human_male, 0.7f, 0.5f, 0.4f, 0.90f, "elonian_warrior");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Kourna}] = VoiceProfile(voice_id_human_female, 0.6f, 0.6f, 0.5f, 0.95f, "elonian_strong");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Vaabi}] = VoiceProfile(voice_id_human_male, 0.3f, 0.8f, 0.6f, 1.1f, "elonian_royal");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Vaabi}] = VoiceProfile(voice_id_human_female, 0.2f, 0.9f, 0.7f, 1.15f, "elonian_luxurious");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_Desolation}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.2f, 0.85f, "elonian_cursed");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_Desolation}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.3f, 0.90f, "elonian_haunted");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_TarnishedCoast}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 1.05f, "intellectual");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_TarnishedCoast}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 1.10f, "scholarly");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_DepthsOfTyria}] = VoiceProfile(voice_id_human_male, 0.6f, 0.4f, 0.3f, 0.85f, "underground");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_DepthsOfTyria}] = VoiceProfile(voice_id_human_female, 0.5f, 0.5f, 0.4f, 0.90f, "deep");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_CharrHomelands}] = VoiceProfile(voice_id_human_male, 0.9f, 0.2f, 0.2f, 0.80f, "prisoner");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_CharrHomelands}] = VoiceProfile(voice_id_human_female, 0.8f, 0.3f, 0.3f, 0.85f, "captive");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_HeroesAscent}] = VoiceProfile(voice_id_human_male, 0.6f, 0.7f, 0.8f, 1.0f, "legendary");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_HeroesAscent}] = VoiceProfile(voice_id_human_female, 0.5f, 0.8f, 0.9f, 1.05f, "heroic");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_FissureOfWoe}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.3f, 0.90f, "eternal");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_FissureOfWoe}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.4f, 0.95f, "otherworldly");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_DomainOfAnguish}] = VoiceProfile(voice_id_human_male, 0.8f, 0.3f, 0.2f, 0.85f, "tormented");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_DomainOfAnguish}] = VoiceProfile(voice_id_human_female, 0.7f, 0.4f, 0.3f, 0.90f, "anguished");
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_BattleIslands}] = VoiceProfile(voice_id_human_male, 0.4f, 0.6f, 0.4f, 1.0f, "worldly");
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_BattleIslands}] = VoiceProfile(voice_id_human_female, 0.3f, 0.7f, 0.5f, 1.05f, "cosmopolitan");

    // Non-human races
    voice_matrix[{Gender::Male, GWRace::Charr, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.8f, 0.4f, 0.3f, 0.85f, "growling");
    voice_matrix[{Gender::Female, GWRace::Charr, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.7f, 0.5f, 0.4f, 0.90f, "fierce");
    voice_matrix[{Gender::Male, GWRace::Dwarf, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_dwarven_male, 0.9f, 0.3f, 0.4f, 0.80f, "gravelly");
    voice_matrix[{Gender::Female, GWRace::Dwarf, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_dwarven_male, 0.8f, 0.4f, 0.5f, 0.85f, "robust");

    // Default fallbacks
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f);
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.5f, 0.5f, 0.5f, 1.0f);

    // Merchant greetings
    merchant_greetings.clear();
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::Merchant}] = L"Welcome, traveler, times are hard but I still have goods to trade.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::ArmorCrafter}] = L"Need armor that can withstand Charr claws?";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::WeaponCustomizer}] = L"Looking for a weapon that can pierce Charr hide?";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RuneTrader}] = L"Ancient runes, salvaged from the ruins of our kingdom.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::MaterialTrader}] = L"Materials salvaged from the ruins, at fair prices.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RareMaterialTrader}] = L"Rare materials, recovered from the Searing's aftermath.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::DyeTrader}] = L"Colors to brighten these dark times.";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::OtherItemCrafter}] = L"Need something crafted by skills that survived the Searing?";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::SkillTrainer}] = L"I teach the combat skills needed to survive in these cursed lands.";

    merchant_greetings[{GW::Region::Region_Presearing, TraderType::Merchant}] = L"Good day, welcome to beautiful Ascalon!";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::ArmorCrafter}] = L"The finest armor in all of prosperous Ascalon.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::WeaponCustomizer}] = L"Welcome, these weapons represent centuries of Ascalonian tradition.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RuneTrader}] = L"Ancient runes from our kingdom's golden age.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::MaterialTrader}] = L"The finest materials from across our prosperous kingdom.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RareMaterialTrader}] = L"Rare treasures from the abundant lands of Ascalon.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::DyeTrader}] = L"Vibrant colors from our peaceful realm.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::OtherItemCrafter}] = L"Greetings, I offer the finest craftsmanship in all of Ascalon.";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::SkillTrainer}] = L"Welcome, I teach the honored traditions of Ascalonian warfare.";

    merchant_greetings[{GW::Region::Region_Kryta, TraderType::Merchant}] = L"Good day to you, welcome to my establishment!";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::ArmorCrafter}] = L"Protection fit for nobles and heroes alike.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::WeaponCustomizer}] = L"Seeking a weapon of distinction?";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RuneTrader}] = L"Ah, a seeker of magical enhancement with excellent taste.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::MaterialTrader}] = L"Fine materials from across the kingdom, at your service.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RareMaterialTrader}] = L"Rare treasures from the royal vaults of Kryta.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::DyeTrader}] = L"Royal colors for distinguished customers.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::OtherItemCrafter}] = L"Greetings, I offer the finest crafting services in Kryta.";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::SkillTrainer}] = L"I shall teach you the refined combat arts of the Krytan nobility.";

    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::Merchant}] = L"Welcome, brave soul, few venture this deep into the jungle.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::ArmorCrafter}] = L"Armor tested against the jungle's deadly creatures.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::WeaponCustomizer}] = L"Weapons tested against the beasts of the deep jungle.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RuneTrader}] = L"Ancient magic, whispered by the jungle spirits.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::MaterialTrader}] = L"Exotic materials from the heart of the Maguuma.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RareMaterialTrader}] = L"Rare jungle treasures, if you have the courage.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::DyeTrader}] = L"Wild colors from nature's most dangerous realm.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials blessed by the jungle spirits.";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::SkillTrainer}] = L"The jungle teaches harsh lessons. I can teach you to survive them.";

    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::Merchant}] = L"Welcome, desert wanderer, supplies are precious here.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::ArmorCrafter}] = L"Protection forged in the crystal caves.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::WeaponCustomizer}] = L"Weapons forged in the crystal caves, sharp as dragon glass.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RuneTrader}] = L"Ancient power, crystallized in the desert sands.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::MaterialTrader}] = L"Rare crystals and desert treasures await the worthy.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RareMaterialTrader}] = L"Mystical crystals from the heart of the desert.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::DyeTrader}] = L"Colors as brilliant as crystal formations.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::OtherItemCrafter}] = L"Greetings, I work with crystal and sand-blessed materials.";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::SkillTrainer}] = L"I teach the ancient arts whispered by the desert winds.";

    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::Merchant}] = L"Welcome to the peaks, warm yourself and browse my wares.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::ArmorCrafter}] = L"Dwarven protection, built to last centuries!";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::WeaponCustomizer}] = L"Weapons forged in the ancient dwarven tradition.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RuneTrader}] = L"Ancient dwarven runes, carved in stone and memory.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::MaterialTrader}] = L"Mountain stone and dwarven steel, the finest materials.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RareMaterialTrader}] = L"Precious gems from the deepest mountain veins.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::DyeTrader}] = L"Mountain colors, bold as dwarven courage.";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::OtherItemCrafter}] = L"Greetings, dwarven craftsmanship built to last centuries!";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::SkillTrainer}] = L"I'll teach ye the battle techniques passed down through generations of dwarven warriors!";

    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::Merchant}] = L"By the forge, another traveler reaches these distant peaks!";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::ArmorCrafter}] = L"Armor tempered by the harshest cold and winds.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::WeaponCustomizer}] = L"Weapons tempered by the coldest winds and hardest stone.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RuneTrader}] = L"Forgotten runes from the edge of the world.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::MaterialTrader}] = L"Materials from the edge of the world, rare beyond measure.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RareMaterialTrader}] = L"Treasures from where the world ends.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::DyeTrader}] = L"Colors from the world's frozen edge.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::OtherItemCrafter}] = L"Welcome, hardy soul, few seek crafting in these remote lands.";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::SkillTrainer}] = L"At the world's edge, only the strongest techniques survive. Let me teach them to you.";

    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::Merchant}] = L"Honorable student, welcome to this place of learning.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::ArmorCrafter}] = L"Protection crafted with the wisdom of ancient masters.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::WeaponCustomizer}] = L"Weapons forged in the sacred traditions of Shing Jea.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RuneTrader}] = L"Ancient symbols blessed by monastery wisdom.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::MaterialTrader}] = L"Materials blessed by the monastery's ancient wisdom.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RareMaterialTrader}] = L"Rare treasures from the sacred archives.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::DyeTrader}] = L"Colors harmonious as monastery gardens.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::OtherItemCrafter}] = L"Greetings, I craft with the wisdom of ancient masters.";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::SkillTrainer}] = L"Welcome, young student - I shall guide you in the ancient martial ways of Shing Jea.";

    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::Merchant}] = L"Welcome to Kaineng City, the finest goods from across the empire!";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::ArmorCrafter}] = L"City armor, where tradition meets innovation.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::WeaponCustomizer}] = L"Weapons crafted in the heart of the empire's greatest city.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RuneTrader}] = L"Imperial runes from the empire's vast collection.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::MaterialTrader}] = L"Urban treasures and exotic materials from every corner of Cantha.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RareMaterialTrader}] = L"Rare goods from the empire's endless markets.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::DyeTrader}] = L"Imperial colors from the city of endless hues.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::OtherItemCrafter}] = L"Greetings, city craftsmanship meets ancient tradition.";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::SkillTrainer}] = L"In the empire's greatest city, I teach the most refined combat techniques.";

    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::Merchant}] = L"Welcome to our sacred forests, nature's bounty awaits.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::ArmorCrafter}] = L"Protection blessed by the forest spirits.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::WeaponCustomizer}] = L"Weapons blessed by the ancient spirits of the wood.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RuneTrader}] = L"Forest runes, whispered by ancient trees.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::MaterialTrader}] = L"Materials harvested with respect for the eternal forest.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RareMaterialTrader}] = L"Sacred forest treasures, given freely by nature.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::DyeTrader}] = L"Forest colors, as eternal as the trees.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::OtherItemCrafter}] = L"Greetings, friend of the forest, I craft with nature's blessing.";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::SkillTrainer}] = L"The eternal forest whispers its secrets. I can teach you its ancient fighting ways.";

    merchant_greetings[{GW::Region::Region_Luxon, TraderType::Merchant}] = L"Welcome, sea-friend, the jade winds bring good fortune.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::ArmorCrafter}] = L"Protection blessed by the jade sea's power.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::WeaponCustomizer}] = L"Weapons forged with the strength of the eternal tides.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RuneTrader}] = L"Sea runes, carved by tidal forces.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::MaterialTrader}] = L"Treasures from the jade sea, brought by favorable winds.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RareMaterialTrader}] = L"Rare jade gifts from the eternal sea.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::DyeTrader}] = L"Sea colors, shifting like jade waters.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials blessed by the jade sea.";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::SkillTrainer}] = L"Like the shifting tides, I teach the fluid combat arts of the Luxon armadas.";

    merchant_greetings[{GW::Region::Region_Istan, TraderType::Merchant}] = L"Ahlan wa sahlan, welcome to the jewel of Elona!";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::ArmorCrafter}] = L"Island protection, light as sea breeze.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::WeaponCustomizer}] = L"Weapons blessed by the island winds and morning sun.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RuneTrader}] = L"Island runes, blessed by ocean spirits.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::MaterialTrader}] = L"Exotic materials from the trade winds of Istan.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RareMaterialTrader}] = L"Precious island treasures from hidden coves.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::DyeTrader}] = L"Tropical colors, bright as paradise.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::OtherItemCrafter}] = L"Greetings, honored traveler, island craftsmanship at its finest.";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::SkillTrainer}] = L"Welcome, traveler! I teach the graceful combat arts perfected under Elonian sun.";

    merchant_greetings[{GW::Region::Region_Kourna, TraderType::Merchant}] = L"Welcome, desert warrior, Kourna's goods serve the strong.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::ArmorCrafter}] = L"Armor tested against centaurs and corsairs.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::WeaponCustomizer}] = L"Weapons tested against the centaurs and corsairs.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RuneTrader}] = L"Battle runes, forged in conflict.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::MaterialTrader}] = L"Materials hardened by sun and survival, proven in battle.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RareMaterialTrader}] = L"War spoils from the harsh mainland.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::DyeTrader}] = L"Desert colors, bold as warrior courage.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::OtherItemCrafter}] = L"Greetings, I forge in the harsh fires of the mainland.";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::SkillTrainer}] = L"In Kourna, only the strong survive. I teach the brutal arts of desert warfare.";

    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::Merchant}] = L"Welcome to Vabbi, land of princes and prosperity!";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::ArmorCrafter}] = L"Armor fit for princes, adorned with precious gems.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::WeaponCustomizer}] = L"Weapons fit for princes, adorned with precious gems.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RuneTrader}] = L"Royal runes from the prince's collections.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::MaterialTrader}] = L"The finest materials gold can buy, from Vabbi's treasuries.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RareMaterialTrader}] = L"Princely treasures from royal vaults.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::DyeTrader}] = L"Royal colors, luxurious as palace silks.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::OtherItemCrafter}] = L"Greetings, only the most exquisite craftsmanship in Vabbi.";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::SkillTrainer}] = L"I teach the elegant combat arts favored by Vabbi's noble princes.";

    merchant_greetings[{GW::Region::Region_Desolation, TraderType::Merchant}] = L"Welcome, brave soul, few dare trade in these cursed lands.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::ArmorCrafter}] = L"Protection against demons and cursed winds.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::WeaponCustomizer}] = L"Weapons forged to face demons and the undead hordes.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RuneTrader}] = L"Cursed runes, powerful but dangerous.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::MaterialTrader}] = L"Materials touched by darkness, powerful but dangerous.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RareMaterialTrader}] = L"Forbidden treasures from the cursed realm.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::DyeTrader}] = L"Dark colors from the realm of shadows.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::OtherItemCrafter}] = L"Greetings, I work despite the sulfurous winds and darkness.";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::SkillTrainer}] = L"In this cursed realm, I teach the forbidden arts needed to fight demons.";

    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::Merchant}] = L"You dare trade in Charr lands, your courage impresses me.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::ArmorCrafter}] = L"Armor stolen from Charr forges.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::WeaponCustomizer}] = L"Weapons forged in the furnaces of our enemies.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RuneTrader}] = L"Charr runes, taken from their shamans.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::MaterialTrader}] = L"Materials stolen from the very heart of Charr power.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RareMaterialTrader}] = L"War trophies from the enemy homeland.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::DyeTrader}] = L"Colors of war from enemy territory.";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::OtherItemCrafter}] = L"By the flame, a human seeks crafting in Charr territory!";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::SkillTrainer}] = L"Incredible! A human seeks training in the heart of Charr territory. I admire your boldness.";

    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::Merchant}] = L"Welcome, eternal warrior, even here commerce finds a way.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::ArmorCrafter}] = L"Protection forged in eternal fires.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::WeaponCustomizer}] = L"Weapons forged in the fires of the eternal realm.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RuneTrader}] = L"Eternal runes from beyond mortal understanding.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::MaterialTrader}] = L"Materials from beyond the mortal realm, precious beyond gold.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RareMaterialTrader}] = L"Treasures from the realm of eternal flame.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::DyeTrader}] = L"Colors from the eternal realm.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::OtherItemCrafter}] = L"Greetings, I craft with materials touched by eternity.";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::SkillTrainer}] = L"In the realm of eternal flame, I teach techniques that transcend mortal understanding.";

    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::Merchant}] = L"Welcome to this realm of shadows, trade persists even here.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::ArmorCrafter}] = L"Protection forged in anguish and shadow.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::WeaponCustomizer}] = L"Weapons forged in anguish, tempered by eternal suffering.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RuneTrader}] = L"Runes of torment from the shadow realm.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::MaterialTrader}] = L"Materials born of anguish, powerful beyond mortal understanding.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RareMaterialTrader}] = L"Treasures from the realm of eternal torment.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::DyeTrader}] = L"Shadow colors from the realm of anguish.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::OtherItemCrafter}] = L"Greetings, shadow-walker, I craft with the essence of torment.";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::SkillTrainer}] = L"In this realm of endless torment, I teach the dark arts born of eternal suffering.";

    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::Merchant}] = L"Welcome, scholar, the Asura have many wondrous inventions to trade.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::ArmorCrafter}] = L"Asura technology meets traditional protection.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::WeaponCustomizer}] = L"Weapons enhanced with Asura ingenuity and magical innovation.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::RuneTrader}] = L"Runic magic refined by Asura intellectual prowess.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::MaterialTrader}] = L"Advanced materials and technological components from Asura laboratories.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::RareMaterialTrader}] = L"Rare components from the most advanced Asura research facilities.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::DyeTrader}] = L"Pigments created through superior Asura alchemical processes.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::OtherItemCrafter}] = L"Greetings, I craft using the most advanced Asura methodologies.";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::SkillTrainer}] = L"I teach combat techniques enhanced by superior Asura intellectual analysis.";

    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::Merchant}] = L"Welcome to the depths, few surface dwellers venture this far below.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::ArmorCrafter}] = L"Protection forged in the deepest caverns and underground forges.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::WeaponCustomizer}] = L"Weapons crafted from the treasures hidden in Tyria's depths.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::RuneTrader}] = L"Ancient runes discovered in the deepest underground chambers.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::MaterialTrader}] = L"Materials mined from the deepest veins beneath the world.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::RareMaterialTrader}] = L"Precious stones and metals from the world's hidden depths.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::DyeTrader}] = L"Deep earth colors from the underground realm.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::OtherItemCrafter}] = L"Greetings, I work with materials from the deepest places of the world.";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::SkillTrainer}] = L"In the depths where few dare tread, I teach the underground fighting arts.";

    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::Merchant}] = L"Welcome to Embark Beach! Goods from across the world.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::ArmorCrafter}] = L"Armor from every tradition, all in one place.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::WeaponCustomizer}] = L"Weapons from every corner of the world.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::RuneTrader}] = L"Runes from across the known world.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::MaterialTrader}] = L"Materials from every land, what do you need?";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::RareMaterialTrader}] = L"Rare treasures from distant shores.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::DyeTrader}] = L"Colors from every culture and tradition.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::OtherItemCrafter}] = L"Greetings, I use techniques from across the world.";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::SkillTrainer}] = L"Welcome! I teach arts from every tradition.";

    const GW::UI::UIMessage messages[] = {GW::UI::UIMessage::kDialogBody, GW::UI::UIMessage::kVendorWindow,           GW::UI::UIMessage::kAgentSpeechBubble,     GW::UI::UIMessage::kMapChange,
                                          GW::UI::UIMessage::kMapLoaded,  GW::UI::UIMessage::kPreferenceValueChanged, GW::UI::UIMessage::kDialogueMessageUpdated};
    for (auto message_id : messages) {
        GW::UI::RegisterUIMessageCallback(&PreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x1);
        GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry, message_id, OnPostUIMessage, 0x4000);
    }
    AudioSettings::RegisterPlaySoundCallback(&UIMessage_HookEntry, OnPlaySound);

    OnAgentSpeechBubble_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("AtMonolog.cpp", "msg.createParam", 0, 0), 0xfff);
    if (OnAgentSpeechBubble_UICallback_Func) {
        GW::Hook::CreateHook((void**)&OnAgentSpeechBubble_UICallback_Func, OnAgentSpeechBubble_UICallback, (void**)&OnAgentSpeechBubble_UICallback_Ret);
        GW::Hook::EnableHooks(OnAgentSpeechBubble_UICallback_Func);
    }
    DEBUG_ASSERT(OnAgentSpeechBubble_UICallback_Func);
}

void TextToSpeechModule::Terminate()
{
    ToolboxModule::Terminate();
    ClearSounds();
    UnHookNPCInteractFrame();
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&PreUIMessage_HookEntry);
    AudioSettings::RemovePlaySoundCallback(&UIMessage_HookEntry);
    if (OnAgentSpeechBubble_UICallback_Func) {
        GW::Hook::RemoveHook(OnAgentSpeechBubble_UICallback_Func);
        OnAgentSpeechBubble_UICallback_Func = nullptr;
    }
}

void TextToSpeechModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    LOAD_STRING(current_tts_provider);
    if (!GetCurrentAPIConfig()) current_tts_provider = api_configs[0].name;

    for (auto& config : api_configs) {
        auto key_name = std::string(config.name) + "_api_key";
        strncpy(config.api_key, ini->GetValue(Name(), key_name.c_str(), ""), _countof(config.api_key));
        key_name = std::string(config.name) + "_user_id";
        strncpy(config.user_id, ini->GetValue(Name(), key_name.c_str(), ""), _countof(config.user_id));
    }

    LOAD_BOOL(stop_speech_when_dialog_closed);
    LOAD_BOOL(play_goodbye_messages);
    LOAD_BOOL(only_use_first_dialog);
    LOAD_BOOL(only_use_first_sentence);
    LOAD_BOOL(play_speech_from_non_friendly_npcs);
    LOAD_BOOL(play_speech_bubbles_in_explorable);
    LOAD_BOOL(play_speech_bubbles_in_outpost);
    LOAD_BOOL(play_speech_bubbles_from_party_members);
    LOAD_BOOL(play_speech_from_vendors);
    LOAD_BOOL(play_tts_in_explorable_areas);
    LOAD_BOOL(play_tts_in_outposts);
    LOAD_FLOAT(npc_speech_bubble_range);

    CSimpleIniA::TNamesDepend keys;
    ini->GetAllKeys(Name(), keys);
    for (const auto& key : keys) {
        std::string key_name = key.pItem;
        if (key_name.find("npc_voice_") == 0) {
            uint32_t npc_id = std::stoul(key_name.substr(10));
            std::string voice_id = ini->GetValue(Name(), key.pItem, voice_id_human_male);
            special_npc_voices[npc_id] = VoiceProfile(voice_id, 0.5f, 0.5f, 0.5f, 1.0f, "");
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(GWRace::Count); i++) {
        GWRace race = static_cast<GWRace>(i);
        std::string key = std::format("play_speech_from_race_{}", GetRaceName(race));
        play_speech_from_race[race] = ini->GetBoolValue(Name(), key.c_str(), true);
    }
}

void TextToSpeechModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);

    SAVE_STRING(current_tts_provider);

    for (auto& config : api_configs) {
        auto key_name = std::string(config.name) + "_api_key";
        ini->SetValue(Name(), key_name.c_str(), config.api_key);
        key_name = std::string(config.name) + "_user_id";
        ini->SetValue(Name(), key_name.c_str(), config.user_id);
    }

    SAVE_BOOL(stop_speech_when_dialog_closed);
    SAVE_BOOL(play_goodbye_messages);
    SAVE_BOOL(only_use_first_dialog);
    SAVE_BOOL(only_use_first_sentence);
    SAVE_BOOL(play_speech_from_non_friendly_npcs);
    SAVE_BOOL(play_speech_bubbles_in_explorable);
    SAVE_BOOL(play_speech_bubbles_in_outpost);
    SAVE_BOOL(play_speech_bubbles_from_party_members);
    SAVE_BOOL(play_speech_from_vendors);
    SAVE_BOOL(play_tts_in_explorable_areas);
    SAVE_BOOL(play_tts_in_outposts);
    SAVE_FLOAT(npc_speech_bubble_range);

    // Remove stale custom voice entries
    CSimpleIniA::TNamesDepend keys;
    ini->GetAllKeys(Name(), keys);
    for (const auto& key : keys) {
        if (std::string(key.pItem).find("npc_voice_") == 0) ini->Delete(Name(), key.pItem);
    }
    for (const auto& [npc_id, voice_profile] : special_npc_voices) {
        std::string key = std::format("npc_voice_{}", npc_id);
        ini->SetValue(Name(), key.c_str(), voice_profile.voice_id.c_str());
    }
    for (const auto& [race, enabled] : play_speech_from_race) {
        std::string key = std::format("play_speech_from_race_{}", GetRaceName(race));
        ini->SetBoolValue(Name(), key.c_str(), enabled);
    }
}

void TextToSpeechModule::DrawSettingsInternal()
{
    static bool show_passwords = false;
    ImGui::Separator();
    ImGui::Text("TTS Provider:");

    std::vector<const char*> provider_names;
    int current_provider = 0;
    for (size_t i = 0; i < _countof(api_configs); i++) {
        provider_names.push_back(api_configs[i].name);
        if (api_configs[i].name == current_tts_provider) current_provider = (int)i;
    }
    if (ImGui::Combo("TTS Service", &current_provider, provider_names.data(), (int)provider_names.size())) current_tts_provider = api_configs[current_provider].name;

    ImGui::Separator();
    ImGui::Text("API Configuration:");
    const ImColor col(102, 187, 238, 255);
    auto api_config = GetCurrentAPIConfig();
    bool is_api_locked_down = !(api_config && *api_config->signup_url);

    if (api_config) {
        if (!is_api_locked_down) {
            ImGui::Text("%s API Key: ", api_config->name);
            ImGui::SameLine();
            ImGui::InputTextSecret("###current provider API Key", api_config->api_key, _countof(api_config->api_key), &show_passwords);
            if (api_config->has_user_id) {
                ImGui::Text("%s User ID: ", api_config->name);
                ImGui::SameLine();
                ImGui::InputTextSecret("###current provider User ID", api_config->user_id, _countof(api_config->user_id), &show_passwords);
            }
            ImGui::TextColored(col.Value, "Click Here to get %s API credentials", api_config->name);
            if (ImGui::IsItemClicked()) {
                GW::GameThread::Enqueue([api_config]() {
                    SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)api_config->signup_url);
                });
            }
        }
        if (api_config->note && *api_config->note) ImGui::TextColored(ImColor(255, 255, 0), api_config->note);
    }

    ImGui::Separator();
    bool show_warning = false;

    if (!is_api_locked_down) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Only process the first sentence of a dialog", &only_use_first_sentence);
        ImGui::ShowHelp("If enabled, only the first sentence of an NPC dialog will be processed.");
        show_warning |= !only_use_first_sentence;
    }
    else {
        ImGui::TextDisabled("Note: With the chosen TTS API, only the first sentence of an NPC's dialog will be processed.");
    }

    ImGui::Checkbox("Only process the first dialog of an NPC", &only_use_first_dialog);
    ImGui::ShowHelp("If enabled, only the first dialog of an NPC conversation will be processed.");
    show_warning |= !only_use_first_dialog;

    ImGui::Checkbox("Play goodbye message when closing NPC dialog", &play_goodbye_messages);
    ImGui::ShowHelp("If enabled, NPCs will say a random goodbye when you close their dialog.\n\nNote: Only english language is currently supported.");
    show_warning |= play_goodbye_messages;

    ImGui::Checkbox("Play greetings from merchants and traders", &play_speech_from_vendors);
    ImGui::ShowHelp("Note: For some types of traders, Only english language is currently supported.");

    ImGui::Checkbox("Stop speech when dialog window is closed", &stop_speech_when_dialog_closed);

    ImGui::TextUnformatted("Play speech in:");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Explorable Areas", &play_tts_in_explorable_areas);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Outposts", &play_tts_in_outposts);
    ImGui::Unindent();

    ImGui::TextUnformatted("Play speech bubbles:");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("When in an outpost", &play_speech_bubbles_in_outpost);
    ImGui::ShowHelp("If enabled, speech bubbles above an NPC when in an outpost will be processed");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("When in an explorable area", &play_speech_bubbles_in_explorable);
    ImGui::ShowHelp("If enabled, speech bubbles from skills and quotes from enemies and allies within speech bubble range will be processed.");
    show_warning |= play_speech_bubbles_in_explorable;
    ImGui::NextSpacedElement();
    ImGui::Checkbox("From other party members", &play_speech_bubbles_from_party_members);
    ImGui::ShowHelp("If enabled, speech bubbles from skills and quotes from party members within speech bubble range will be processed.");
    show_warning |= play_speech_bubbles_from_party_members;
    ImGui::NextSpacedElement();
    ImGui::Checkbox("From non-friendly NPCs", &play_speech_from_non_friendly_npcs);
    show_warning |= play_speech_from_non_friendly_npcs;
    ImGui::Unindent();

    if (ImGui::InputFloat("NPC speech bubble range", &npc_speech_bubble_range, GW::Constants::Range::Adjacent, GW::Constants::Range::Adjacent)) {
        npc_speech_bubble_range = std::clamp(npc_speech_bubble_range, 0.f, 2500.f);
    }
    ImGui::ShowHelp("The range at which NPC speech bubbles will be processed. Set to 0 to disable.");
    show_warning |= (npc_speech_bubble_range > 166.f);

    if (show_warning && !is_api_locked_down) ImGui::TextColored(ImColor(IM_COL32(245, 245, 0, 255)), "Warning: Processing more lines of dialog will use up more API credits!");

    ImGui::TextUnformatted("Play speech from:");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    for (auto& it : play_speech_from_race) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox(GetRaceName(it.first), (bool*)&it.second);
    }
    ImGui::Unindent();

    if (!is_api_locked_down) {
        ImGui::Separator();
        ImGui::Text("Custom NPC Voice Assignment:");
        ImGui::Text("Assign specific voices to individual NPCs by their NPC ID.");
        ImGui::Text("Voice settings (stability, similarity, etc.) will be inherited from the NPC's region/gender.");

        ImGui::PushItemWidth(100);
        ImGui::InputTextWithHint("##npc_id_custom", "e.g. 1234", custom_npc_id_buffer, sizeof(custom_npc_id_buffer), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("NPC ID");
        ImGui::SameLine();
        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##voice_id_custom", "e.g. 2EiwWnXFnvU5JabPnv8n", custom_voice_id_buffer, sizeof(custom_voice_id_buffer));
        ImGui::SameLine();
        ImGui::Text("Voice ID");
        ImGui::PopItemWidth();

        if (ImGui::Button("Add Custom Voice Assignment")) {
            if (strlen(custom_npc_id_buffer) > 0 && strlen(custom_voice_id_buffer) > 0) {
                try {
                    uint32_t npc_id = std::stoul(custom_npc_id_buffer);
                    special_npc_voices[npc_id] = VoiceProfile(custom_voice_id_buffer, 0.5f, 0.5f, 0.5f, 1.0f, "");
                    memset(custom_npc_id_buffer, 0, sizeof(custom_npc_id_buffer));
                    memset(custom_voice_id_buffer, 0, sizeof(custom_voice_id_buffer));
                    VoiceLog("Added custom voice assignment for NPC ID %u", npc_id);
                } catch (const std::exception&) {
                    VoiceLog("Invalid NPC ID entered");
                }
            }
        }
        ImGui::PopItemWidth();

        if (!special_npc_voices.empty()) {
            ImGui::Separator();
            ImGui::Text("Current Custom Voice Assignments:");
            std::vector<uint32_t> to_remove;
            for (const auto& [npc_id, voice_profile] : special_npc_voices) {
                ImGui::PushID(npc_id);
                ImGui::Text("NPC ID: %u", npc_id);
                ImGui::SameLine(120);
                ImGui::Text("Voice: %s", voice_profile.voice_id.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImColor(150, 150, 150), "(inherits region/gender settings)");
                ImGui::SameLine();
                if (ImGui::Button("Remove")) to_remove.push_back(npc_id);
                ImGui::PopID();
            }
            for (uint32_t npc_id : to_remove) {
                special_npc_voices.erase(npc_id);
                VoiceLog("Removed custom voice assignment for NPC ID %u", npc_id);
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Recent Activity:");
    if (voice_log_messages.empty()) {
        ImGui::TextDisabled("No recent activity");
    }
    else {
        std::string combined_log;
        for (const auto& msg : voice_log_messages) {
            if (!combined_log.empty()) combined_log += "\n";
            combined_log += msg;
        }
        ImGui::InputTextMultiline("##VoiceLog", const_cast<char*>(combined_log.c_str()), combined_log.size(), ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
    }
}
