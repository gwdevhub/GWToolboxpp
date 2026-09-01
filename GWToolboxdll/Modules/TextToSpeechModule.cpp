#include "TextToSpeechModule.h"
#include "stdafx.h"

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
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
#include <thread>

#include <Functiondiscoverykeys_devpkey.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

namespace {

    TextToSpeechModule::Settings settings;

    const char* voice_id_human_male = "2EiwWnXFnvU5JabPnv8n";
    const char* voice_id_human_female = "EXAVITQu4vr4xnSDxMaL";
    const char* voice_id_dwarven_male = "N2lVS1w4EtoT3dr4eOWO";

    const char* gwtts_hostname = "https://tts.gwtoolbox.com";
    // const char* gwtts_hostname = "http://localhost:8081";

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

    struct APIConfig {
        GenerateVoiceCallback callback;
        const char* name;
        const char* signup_url;
        const char* note = nullptr;
        bool has_user_id = false;
        // Some providers issue long keys (e.g. OpenAI project keys are ~200 chars), and Kokoro stores a server URL here
        char api_key[512] = {0};
        char user_id[512] = {0};
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
        {GenerateVoiceGWDevHub, "GWDevHub TTS", "", "这是一项专为激战玩家提供的免费 TTS 服务，由 GWDevHub 提供"},
        {GenerateVoiceElevenLabs, "ElevenLabs", "https://elevenlabs.io/app/settings/api-keys"},
        {GenerateVoiceOpenAI, "OpenAI", "https://platform.openai.com/api-keys"},
        {GenerateVoiceGoogle, "Google Cloud", "https://console.cloud.google.com/apis/credentials", "注意：请确保在您的 Google Cloud 项目中启用了文本转语音 API"},
        {GenerateVoicePlayHT, "Play.ht", "https://elevenlabs.io/app/settings/api-keys", "注意：Play.ht 需要同时提供 API Key 和 User ID", true},
        {GenerateVoiceKokoro, "Kokoro（自托管）", "https://github.com/remsky/Kokoro-FastAPI", "请在 API Key 字段中输入您的 Kokoro-FastAPI 服务器 URL（默认：http://localhost:8880）"},
    };

    APIConfig* GetCurrentAPIConfig()
    {
        const auto found = std::ranges::find(api_configs, settings.current_tts_provider, &APIConfig::name);
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
        if (cache) {
            last_cached_system_volume = TIMER_INIT();
            Resources::EnqueueWorkerTask([] { GetSystemVolume(false); });
            return cached_system_volume;
        }

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
                return "人类";
            case GWRace::Charr:
                return "夏尔";
            case GWRace::Norn:
                return "诺恩";
            case GWRace::Asura:
                return "阿苏拉";
            case GWRace::Tengu:
                return "天狗";
            case GWRace::Dwarf:
                return "矮人";
            case GWRace::Centaur:
                return "半人马";
            default:
                return "未知";
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

    const wchar_t* generic_goodbye_messages[] = {L"再会，旅者。",           L"愿你的旅途平安。", L"愿你的前路清晰。", L"后会有期。", L"一路顺风，冒险者。",         L"祝你好运。",
                                                 L"愿命运眷顾你。",        L"旅途平安，朋友。",        L"再见，保重。", L"愿你安好。",      L"愿众神守护你。", L"在外小心。",
                                                 L"旅途愉快。",          L"愿你的旅程快捷。",    L"安全前行。",         L"带着我的祝福出发。", L"愿和平与你同在。",       L"英雄，一路平安。",
                                                 L"旅途好运。", L"愿你的道路受到庇护。"};

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

    uint32_t npc_ids_to_ignore[] = {1991}; // 德曼德

    uint32_t last_dialog_agent_id = 0;
    size_t max_text_length = 512;
    VoiceProfile default_voice_profile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f, "");

    std::wstring PreprocessEncodedTextForTTS(const std::wstring& text);
    VoiceProfile* GetVoiceProfile(uint32_t agent_id, GW::Constants::MapID map_id);

    // -------------------------------------------------------------------------
    // PendingNPCAudio — shared_ptr 管理，跨线程安全
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

        // 工厂方法 — 始终通过此方法构造，以便在对象接触 pending_audio 之前建立 shared_ptr。
        static std::shared_ptr<PendingNPCAudio> Create(uint32_t agent_id, const wchar_t* message, bool is_dialog_window = false);

        ~PendingNPCAudio();

    private:
        // 私有构造函数 — 使用 Create()
        PendingNPCAudio(uint32_t _agent_id, const wchar_t* message, bool _is_dialog_window);
    };

    // -------------------------------------------------------------------------
    // 全局音频状态
    // -------------------------------------------------------------------------
    std::recursive_mutex playing_audio_mutex;
    std::vector<std::shared_ptr<PendingNPCAudio>> pending_audio;
    std::map<uint32_t, std::shared_ptr<PendingNPCAudio>> playing_audio_map;

    // 原子变量，工作线程和游戏线程可以无锁安全读写。
    std::atomic<bool> generating_voice{false};

    // -------------------------------------------------------------------------
    // 共享集合上的辅助函数（必须持有 playing_audio_mutex）
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
            Log::Error("获取 %s 文件大小失败：%s", audio_file.string().c_str(), err.message().c_str());
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
    // PendingNPCAudio 实现
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
        // 注意：此处不添加到 pending_audio — Create() 在 shared_ptr 建立后执行此操作，
        // 以便 shared_from_this() 有效。
    }

    /*static*/ std::shared_ptr<PendingNPCAudio> PendingNPCAudio::Create(uint32_t agent_id, const wchar_t* message, bool is_dialog_window)
    {
        // 直接使用 new，因为构造函数是私有的；make_shared 需要友元或公共构造函数。
        std::shared_ptr<PendingNPCAudio> audio(new PendingNPCAudio(agent_id, message, is_dialog_window));
        std::scoped_lock lock(playing_audio_mutex);
        pending_audio.push_back(audio);
        return audio;
    }

    PendingNPCAudio::~PendingNPCAudio()
    {
        // 停止播放。pending_audio / playing_audio_map 中的 shared_ptr 持有者负责清除自身；
        // 析构函数仅清理声音句柄。
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
        // --- 阶段 1：在锁下检查重复/准备 ---
        {
            std::scoped_lock lock(playing_audio_mutex);

            auto found = playing_audio_map.find(agent_id);
            if (found != playing_audio_map.end()) {
                if (found->second->path == path && found->second->IsPlaying()) return; // 同一音频已在播放 — 无需操作。
                // 该代理的不同音频：停止它。
                found->second->Stop();
                playing_audio_map.erase(found);
            }

            if (!duration) duration = EstimateAudioDuration(path);
        }

        const auto pos = GetAgentVec3f(agent_id);
        VoiceLog("正在播放音频文件：%s（估计时长：%dms）", path.filename().string().c_str(), duration);
        const uint32_t flags = is_dialog_window ? SoundFlags_Dialog : (SoundFlags_Dialog | SoundFlags_Positional);
        const bool success = AudioSettings::PlaySound(path.wstring().c_str(), &pos, flags, &gw_handle);

        if (!success) {
            // PlaySound 失败；从 pending 中移除并退出。
            std::scoped_lock lock(playing_audio_mutex);
            RemoveFromPending(shared_from_this());
            return;
        }

        // --- 阶段 3：在锁下注册为正在播放 ---
        {
            std::scoped_lock lock(playing_audio_mutex);
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
        std::scoped_lock lock(playing_audio_mutex);
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
        std::scoped_lock lock(playing_audio_mutex);
        for (auto& [id, audio] : playing_audio_map)
            audio->Stop();
        playing_audio_map.clear();
        pending_audio.clear(); // shared_ptrs 释放；析构函数安全运行。
    }

    // -------------------------------------------------------------------------
    // 语音生成辅助函数
    // -------------------------------------------------------------------------
    std::shared_ptr<PendingNPCAudio> FindAlreadyProcessingAudio(const std::shared_ptr<PendingNPCAudio>& compare)
    {
        if (!compare) return nullptr;
        std::scoped_lock lock(playing_audio_mutex);
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
                                             "被选中者\x0001">(text);
        result = TextUtils::ctre_regex_replace<L"[\x0101\x102\x103\x104][\x8100-\xffff]*.", L"">(result);
        return result;
    }

    std::wstring PreprocessTextForTTS(const std::wstring& text)
    {
        std::wstring processed = text;
        processed = TextUtils::ctre_regex_replace<L"<a=[^<]+[^>]+>", L"">(processed);
        processed = TextUtils::ctre_regex_replace<L"<br>|<brx>|<p>", L". ">(processed);
        processed = TextUtils::StripTags(processed);
        if (settings.only_use_first_sentence) processed = ExtractFirstSentence(processed);
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
        if (!settings.play_speech_from_non_friendly_npcs && agent->allegiance == GW::Constants::Allegiance::Enemy) return nullptr;

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

    void GenerateVoice(std::shared_ptr<PendingNPCAudio> audio);

    void GenerateVoiceFromDecodedString(std::shared_ptr<PendingNPCAudio> audio)
    {
        if (!(audio && !audio->decoded_message.empty())) return; // shared_ptr 释放，析构函数处理清理
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
        if (!GetApiKey()) return; // shared_ptr 释放
        if (!(audio && !audio->encoded_message.empty() && audio->profile)) return;
        if (FindAlreadyProcessingAudio(audio)) return;

        // 通过在堆上存储新的 shared_ptr 来保持 audio 在异步解码期间存活。
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

    GW::HookEntry FrameUIMessage_HookEntry;

    bool was_dialog_already_open = false;

    void OnNPCDialogClosed()
    {
        if (settings.stop_speech_when_dialog_closed) CancelDialogSpeech(last_dialog_agent_id);

        if (settings.play_goodbye_messages && !was_dialog_already_open && GetDistanceFromAgentId(last_dialog_agent_id) < GW::Constants::Range::Adjacent) {
            if (GetAudioLanguage() != GW::Constants::Language::English) return;
            const auto num_goodbye_messages = sizeof(generic_goodbye_messages) / sizeof(generic_goodbye_messages[0]);
            const wchar_t* goodbye_msg = generic_goodbye_messages[rand() % num_goodbye_messages];

            auto audio = PendingNPCAudio::Create(last_dialog_agent_id, L"", true);
            audio->decoded_message = PreprocessTextForTTS(goodbye_msg);
            audio->profile = GetVoiceProfile(last_dialog_agent_id, GW::Map::GetMapID());
            if (audio->profile) GenerateVoiceFromDecodedString(std::move(audio));
            // 否则 shared_ptr 释放，对象自行清理
        }
    }

    // Not a hook on the frame's callback: DialogModule and GWCA hook that same function, and
    // GWCA fatally asserts when its own CreateHook then collides on the already-hooked target.
    void OnDialogFrameDestroyed(GW::HookStatus*, const GW::UI::Frame* frame, GW::UI::UIMessage, void*, void*)
    {
        if (frame == GW::UI::GetFrameByLabel(L"NPCInteract") || frame == GW::UI::GetFrameByLabel(L"Vendor")) {
            OnNPCDialogClosed();
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
                if (settings.only_use_first_dialog && was_dialog_already_open) return;
                GenerateVoiceFromEncodedString(PendingNPCAudio::Create(packet->agent_id, packet->message_enc, true));
            } break;
        }
    }

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (msgid) {
            case GW::UI::UIMessage::kDialogBody: {
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
                    Log::Log("为 %d 生成语音（对话消息）", current->agent_id);
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
                        if (!settings.play_speech_from_vendors) return;
                        auto audio = PendingNPCAudio::Create(packet->unk, L"", true);
                        // 通过堆上的 shared_ptr 在异步名称查找期间保持 audio 存活。
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
                                // 否则 shared_ptr 释放，自动清理
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

    std::string PostJson(RestClient& client, const std::string& url, const glz::generic& request_body, const std::string& service_name = "API", int timeout_sec = 2)
    {
        client.SetUrl(url.c_str());
        client.SetHeader("Content-Type", "application/json");
        client.SetPostContent(glz::write_json(request_body).value_or(std::string{}), ContentFlag::Copy);
        client.SetFollowLocation(true);
        client.SetVerifyHost(false);
        client.SetVerifyPeer(false);
        client.SetTimeoutSec(timeout_sec);
        client.Execute();
        if (!client.IsSuccessful()) {
            VoiceLog("%s 返回错误代码：%ld", service_name.c_str(), client.GetStatusCode());
            std::string error_response = std::move(client.GetContent());
            if (!error_response.empty()) VoiceLog("错误响应：%s", error_response.c_str());
            return "";
        }
        return std::move(client.GetContent());
    }

    std::string GenerateVoiceOpenAI(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("无音频配置"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("无 API Key"), "";

        glz::generic request_body;
        request_body["model"] = "gpt-4o-mini-tts";
        request_body["input"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"] = (audio->profile->voice_id == voice_id_human_female) ? "nova" : "onyx";
        request_body["response_format"] = "mp3";
        request_body["speed"] = audio->profile->speaking_rate;
        request_body["language"] = LanguageToAbbreviation(audio->language);

        RestClient client;
        client.SetHeader("Authorization", ("Bearer " + std::string(api_config->api_key)).c_str());
        const auto audio_data = PostJson(client, "https://api.openai.com/v1/audio/speech", request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("OpenAI 语音生成成功，收到 %zu 字节", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceElevenLabs(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("无音频配置"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("无 API Key"), "";

        glz::generic voice_settings;
        voice_settings["stability"] = audio->profile->stability;
        voice_settings["similarity_boost"] = audio->profile->similarity;
        voice_settings["style"] = audio->profile->style;
        voice_settings["use_speaker_boost"] = true;

        glz::generic request_body;
        request_body["text"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["model_id"] = "eleven_flash_v2_5";
        request_body["voice_settings"] = voice_settings;
        request_body["language"] = LanguageToAbbreviation(audio->language);

        RestClient client;
        client.SetHeader("xi-api-key", api_config->api_key);
        client.SetHeader("Accept", "audio/mpeg");
        const auto audio_data = PostJson(client, "https://api.elevenlabs.io/v1/text-to-speech/" + audio->profile->voice_id, request_body, api_config->name);
        if (!audio_data.empty()) VoiceLog("ElevenLabs 语音生成成功，收到 %zu 字节", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceGWDevHub(PendingNPCAudio* audio)
    {
        std::vector<uint32_t> encoded_array;
        encoded_array.reserve(audio->encoded_message.size());
        for (const auto& c : audio->encoded_message)
            encoded_array.push_back(static_cast<uint32_t>(c));

        std::vector<uint32_t> decoded_array;
        decoded_array.reserve(audio->decoded_message.size());
        for (const auto& c : audio->decoded_message)
            decoded_array.push_back(static_cast<uint32_t>(c));

        glz::generic request_body = glz::generic::object_t{};
        request_body["encoded"] = encoded_array;
        request_body["decoded"] = decoded_array;

        if (!audio->encoded_npc_name.empty()) {
            std::vector<uint32_t> encoded_npc_name_arr;
            encoded_npc_name_arr.reserve(audio->encoded_npc_name.size());
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
        if (!audio_data.empty()) VoiceLog("GWDevHub 语音生成成功，收到 %zu 字节", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceKokoro(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("无音频配置"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!api_config) return VoiceLog("无 API 配置"), "";

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

        glz::generic request_body;
        request_body["model"] = "kokoro";
        request_body["input"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"] = (audio->gender == Gender::Female) ? "af_bella" : "am_adam";
        request_body["response_format"] = "mp3";
        request_body["speed"] = audio->profile->speaking_rate;
        request_body["lang_code"] = lang_code;

        RestClient client;
        // Kokoro 在生成时流式传输音频；在 CPU 硬件上长文本可能需要 60 秒以上
        const auto audio_data = PostJson(client, base_url + "/v1/audio/speech", request_body, api_config->name, 120);
        if (!audio_data.empty()) VoiceLog("Kokoro 语音生成成功，收到 %zu 字节", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoiceGoogle(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("无音频配置"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("无 API Key"), "";

        const std::string voice_name = (audio->profile->voice_id == voice_id_human_female) ? "en-US-Studio-O" : "en-US-Studio-Q";

        glz::generic request_body = glz::generic::object_t{};
        request_body["input"]["text"] = TextUtils::WStringToString(audio->decoded_message);
        request_body["voice"]["name"] = voice_name;
        request_body["voice"]["languageCode"] = "en-US";
        request_body["audioConfig"]["audioEncoding"] = "MP3";
        request_body["audioConfig"]["speakingRate"] = audio->profile->speaking_rate;
        request_body["audioConfig"]["pitch"] = 0.0f;

        RestClient client;
        const auto response_str = PostJson(client, "https://texttospeech.googleapis.com/v1/text:synthesize?key=" + std::string(api_config->api_key), request_body, api_config->name);
        if (response_str.empty()) return response_str;

        glz::generic json_response;
        if (auto ec = glz::read_json(json_response, response_str); ec || !json_response.contains("audioContent") || !json_response.at("audioContent").is_string()) {
            VoiceLog("解析 Google TTS 响应 JSON 失败");
            return "";
        }

        std::string base64_audio = json_response.at("audioContent").get<std::string>();
        if (base64_audio.empty()) {
            VoiceLog("Google TTS 返回空音频内容");
            return "";
        }

        std::string audio_data = TextUtils::Base64Decode<char>(base64_audio);
        VoiceLog("Google 语音生成成功，解码 %zu 字节", audio_data.size());
        return audio_data;
    }

    std::string GenerateVoicePlayHT(PendingNPCAudio* audio)
    {
        if (!(audio && audio->profile)) return VoiceLog("无音频配置"), "";
        const auto api_config = GetCurrentAPIConfig();
        if (!(api_config && *api_config->api_key)) return VoiceLog("无 API Key"), "";
        if (!*api_config->user_id) return VoiceLog("无 User ID"), "";

        const std::string voice_id = (GetAgentGender(audio->agent_id) == Gender::Female) ? playht_voice_female_default : playht_voice_male_default;
        const std::string lang_code = LanguageToAbbreviation(audio->language);

        glz::generic request_body = glz::generic::object_t{};
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
        if (!audio_data.empty()) VoiceLog("Play.ht 语音生成成功，收到 %zu 字节", audio_data.size());
        return audio_data;
    }

    // -------------------------------------------------------------------------
    // GenerateVoice — 主分发函数
    //
    // 获取 audio 的共享所有权。工作线程 lambda 捕获 weak_ptr，以便
    // 如果音频在工作线程运行时被取消（ClearSounds / CancelDialogSpeech），
    // lock() 返回 null，我们干净地退出而不触碰已释放的内存。
    // -------------------------------------------------------------------------
    void GenerateVoice(std::shared_ptr<PendingNPCAudio> audio)
    {
        if (!audio) return;

        // 一次只允许一个语音生成。如果忙碌则丢弃。
        bool expected = false;
        if (!generating_voice.compare_exchange_strong(expected, true)) return; // shared_ptr 释放，pending_audio 仍持有引用

        // --- 早期退出检查（游戏线程侧，在交给工作线程之前）---
        auto bail = [&]() {
            // 从 pending 中移除以免残留，然后释放。
            {
                std::scoped_lock lock(playing_audio_mutex);
                RemoveFromPending(audio);
            }
            generating_voice = false;
        };

        if (GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) {
            VoiceLog("激战窗口未处于焦点");
            return bail();
        }
        if (audio->gender == Gender::Unknown) {
            VoiceLog("未知性别");
            return bail();
        }
        if (!play_speech_from_race[audio->race]) {
            VoiceLog("已阻止种族 %s", GetRaceName(audio->race));
            return bail();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && !settings.play_tts_in_outposts) {
            VoiceLog("已在前哨站阻止 TTS");
            return bail();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && !settings.play_tts_in_explorable_areas) {
            VoiceLog("已在可探索区域阻止 TTS");
            return bail();
        }
        if (GetDialogVolume() * GetSystemVolume() == 0.f) {
            VoiceLog("对话音量为 0");
            return bail();
        }

        std::weak_ptr<PendingNPCAudio> weak_audio = audio;
        // 现在释放我们的强引用 — pending_audio 仍持有一个。
        audio.reset();

        Resources::EnqueueWorkerTask([weak_audio]() {
            // 尝试获取强所有权。
            auto audio = weak_audio.lock();
            if (!audio) {
                generating_voice = false;
                return;
            }

            // 确认仍在 pending_audio 中（在入队和执行之间未被取消）。
            {
                std::scoped_lock lock(playing_audio_mutex);
                if (!IsPending(audio)) {
                    generating_voice = false;
                    return;
                }
            }

            const auto api_config = GetCurrentAPIConfig();
            const auto cache_key = GenerateOptimizedCacheKey(audio.get());
            audio->path = Resources::GetPath("NPCVoiceCache") / LanguageToAbbreviation(audio->language) / cache_key;

            // 缓存命中 — 立即播放。
            if (std::filesystem::exists(audio->path)) {
                audio->Play();
                generating_voice = false;
                return;
            }

            // 通过选定的提供商生成音频数据。
            std::string audio_data = api_config ? api_config->callback(audio.get()) : "";

            // 重新检查音频在（可能较慢的）API 调用期间是否被取消。
            {
                std::scoped_lock lock(playing_audio_mutex);
                if (!IsPending(audio)) {
                    generating_voice = false;
                    return;
                }
            }

            if (audio_data.empty()) {
                VoiceLog("生成语音数据失败");
                std::scoped_lock lock(playing_audio_mutex);
                RemoveFromPending(audio);
                generating_voice = false;
                return;
            }

            if (!Resources::WriteFile(audio->path, audio_data)) {
                VoiceLog("写入音频数据到文件失败：%s", TextUtils::WStringToString(audio->path.wstring()).c_str());
                std::scoped_lock lock(playing_audio_mutex);
                RemoveFromPending(audio);
                generating_voice = false;
                return;
            }

            // 播放前的最终 pending 检查。
            {
                std::scoped_lock lock(playing_audio_mutex);
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
    // OnPlaySound — 当代理有 TTS 音频时阻止游戏内 NPC 语音
    // -------------------------------------------------------------------------
    void OnPlaySound(GW::HookStatus* status, const wchar_t* filename, SoundProps* props)
    {
        if (status->blocked) return;
        if (!(props && (props->flags & 0xffff) == (SoundFlags_Dialog | SoundFlags_Positional))) return;
        if (GW::Map::GetIsInCinematic()) return;
        if (wcslen(filename) > 4) return;
        if (!GetApiKey()) return;

        const auto agent_id = GetAgentAtPosition({props->position.x, props->position.y}, 20.f);
        if (!(agent_id && GetVoiceProfile(agent_id, GW::Map::GetMapID()))) return;

        if (wcslen(filename) < 10) {
            status->blocked = true;
            return;
        }

        std::scoped_lock lock(playing_audio_mutex);
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

                if (!settings.play_speech_bubbles_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) break;
                if (!settings.play_speech_bubbles_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) break;
                if (!settings.play_speech_bubbles_from_party_members && GW::PartyMgr::IsAgentInParty(agent_id)) break;
                if (GW::GetDistance(agent->pos, GetPlayerPosition()) > settings.npc_speech_bubble_range) break;

                Log::Log("为 %d 生成语音（对话气泡）", agent_id);
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

    std::scoped_lock lock(playing_audio_mutex);
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
    SettingsRegistry::Register(this, settings);

    if (play_speech_from_race.empty()) {
        for (size_t i = 0; i < (size_t)GWRace::Count; i++)
            play_speech_from_race[(GWRace)i] = true;
    }

    voice_matrix.clear();
    special_npc_voices.clear();

    // 按地区和性别的人类语音
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

    // 非人类种族
    voice_matrix[{Gender::Male, GWRace::Charr, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.8f, 0.4f, 0.3f, 0.85f, "growling");
    voice_matrix[{Gender::Female, GWRace::Charr, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.7f, 0.5f, 0.4f, 0.90f, "fierce");
    voice_matrix[{Gender::Male, GWRace::Dwarf, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_dwarven_male, 0.9f, 0.3f, 0.4f, 0.80f, "gravelly");
    voice_matrix[{Gender::Female, GWRace::Dwarf, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_dwarven_male, 0.8f, 0.4f, 0.5f, 0.85f, "robust");

    // 默认回退
    voice_matrix[{Gender::Male, GWRace::Human, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_male, 0.5f, 0.5f, 0.5f, 1.0f);
    voice_matrix[{Gender::Female, GWRace::Human, GW::Region::Region_DevRegion}] = VoiceProfile(voice_id_human_female, 0.5f, 0.5f, 0.5f, 1.0f);

    // 商人问候语（全部翻译为中文）
    merchant_greetings.clear();
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::Merchant}] = L"欢迎，旅者，时局艰难，但我仍有货物可交易。";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::ArmorCrafter}] = L"需要能抵御夏尔利爪的护甲吗？";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::WeaponCustomizer}] = L"想要能穿透夏尔皮毛的武器吗？";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RuneTrader}] = L"古老的符文，从我们王国的废墟中发掘。";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::MaterialTrader}] = L"从废墟中回收的材料，价格公道。";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::RareMaterialTrader}] = L"稀有材料，从灼晶灾变中幸存。";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::DyeTrader}] = L"为这黑暗时代增添色彩的染料。";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::OtherItemCrafter}] = L"需要能在灼晶灾变中幸存的工艺制品吗？";
    merchant_greetings[{GW::Region::Region_Ascalon, TraderType::SkillTrainer}] = L"我传授在这片被诅咒的土地上生存所需的战斗技能。";

    merchant_greetings[{GW::Region::Region_Presearing, TraderType::Merchant}] = L"日安，欢迎来到美丽的阿斯卡隆！";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::ArmorCrafter}] = L"繁荣的阿斯卡隆最精美的护甲。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::WeaponCustomizer}] = L"欢迎，这些武器代表了阿斯卡隆数世纪的传承。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RuneTrader}] = L"来自我们王国黄金时代的古老符文。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::MaterialTrader}] = L"来自我们繁荣王国各地的最优质材料。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::RareMaterialTrader}] = L"来自阿斯卡隆富饶之地的稀有珍宝。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::DyeTrader}] = L"来自我们和平国度的鲜艳色彩。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::OtherItemCrafter}] = L"日安，我提供阿斯卡隆最精湛的工艺。";
    merchant_greetings[{GW::Region::Region_Presearing, TraderType::SkillTrainer}] = L"欢迎，我传授阿斯卡隆战争艺术的尊贵传统。";

    merchant_greetings[{GW::Region::Region_Kryta, TraderType::Merchant}] = L"日安，欢迎光临我的店铺！";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::ArmorCrafter}] = L"适合贵族和英雄的防护。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::WeaponCustomizer}] = L"在寻找一把与众不同的武器吗？";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RuneTrader}] = L"啊，一位品味高雅的魔法增强追求者。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::MaterialTrader}] = L"来自王国各地的优质材料，为您服务。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::RareMaterialTrader}] = L"来自科瑞塔皇家宝库的稀有珍宝。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::DyeTrader}] = L"为尊贵顾客准备的皇家色彩。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::OtherItemCrafter}] = L"日安，我提供科瑞塔最精湛的工艺服务。";
    merchant_greetings[{GW::Region::Region_Kryta, TraderType::SkillTrainer}] = L"我将传授您科瑞塔贵族精炼的战斗艺术。";

    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::Merchant}] = L"欢迎，勇敢的灵魂，很少有人深入这片丛林。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::ArmorCrafter}] = L"经过丛林致命生物考验的护甲。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::WeaponCustomizer}] = L"经过丛林深处猛兽考验的武器。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RuneTrader}] = L"丛林精灵低语的古老魔法。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::MaterialTrader}] = L"来自马古马丛林深处的异国材料。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::RareMaterialTrader}] = L"如果您有勇气，这里有稀有的丛林珍宝。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::DyeTrader}] = L"来自自然最危险领域的野性色彩。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::OtherItemCrafter}] = L"日安，我用丛林精灵祝福的材料进行制作。";
    merchant_greetings[{GW::Region::Region_Maguuma, TraderType::SkillTrainer}] = L"丛林教会了严酷的教训。我可以教您如何在其中生存。";

    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::Merchant}] = L"欢迎，沙漠流浪者，这里的补给很珍贵。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::ArmorCrafter}] = L"在水晶洞穴中锻造的防护。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::WeaponCustomizer}] = L"在水晶洞穴中锻造的武器，锋利如龙晶。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RuneTrader}] = L"凝聚在沙漠沙粒中的古老力量。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::MaterialTrader}] = L"稀有的水晶和沙漠珍宝等待有缘人。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::RareMaterialTrader}] = L"来自沙漠深处的神秘水晶。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::DyeTrader}] = L"如水晶般绚丽的色彩。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::OtherItemCrafter}] = L"日安，我用水晶和沙之祝福的材料工作。";
    merchant_greetings[{GW::Region::Region_CrystalDesert, TraderType::SkillTrainer}] = L"我传授沙漠之风低语的古老技艺。";

    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::Merchant}] = L"欢迎来到山峰，暖暖身子，看看我的货物吧。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::ArmorCrafter}] = L"矮人的防护，经久耐用！";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::WeaponCustomizer}] = L"按照古老矮人传统锻造的武器。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RuneTrader}] = L"刻在石头和记忆中的古老矮人符文。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::MaterialTrader}] = L"山石和矮人钢，最优质的材料。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::RareMaterialTrader}] = L"来自最深矿脉的珍贵宝石。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::DyeTrader}] = L"如矮人勇气般大胆的山地色彩。";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::OtherItemCrafter}] = L"日安，矮人工艺经久耐用！";
    merchant_greetings[{GW::Region::Region_NorthernShiverpeaks, TraderType::SkillTrainer}] = L"我将教您世代相传的矮人战士战斗技巧！";

    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::Merchant}] = L"以熔炉之名，又一位旅者到达了这遥远山峰！";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::ArmorCrafter}] = L"经受最严酷严寒和狂风考验的护甲。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::WeaponCustomizer}] = L"经最冷寒风和最硬岩石淬炼的武器。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RuneTrader}] = L"世界边缘被遗忘的符文。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::MaterialTrader}] = L"来自世界边缘的材料，稀有无比。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::RareMaterialTrader}] = L"来自世界尽头的珍宝。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::DyeTrader}] = L"来自世界冰封边缘的色彩。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::OtherItemCrafter}] = L"欢迎，坚韧的灵魂，在这偏远之地寻求工艺的人不多。";
    merchant_greetings[{GW::Region::Region_FarShiverpeaks, TraderType::SkillTrainer}] = L"在世界边缘，只有最强的技巧才能生存。让我教给您。";

    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::Merchant}] = L"可敬的学徒，欢迎来到这个学习之地。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::ArmorCrafter}] = L"以古代大师智慧锻造的防护。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::WeaponCustomizer}] = L"在星岬寺神圣传统中锻造的武器。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RuneTrader}] = L"受寺院智慧祝福的古老符号。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::MaterialTrader}] = L"受寺院古老智慧祝福的材料。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::RareMaterialTrader}] = L"来自神圣档案的稀有珍宝。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::DyeTrader}] = L"如寺院花园般和谐的色彩。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::OtherItemCrafter}] = L"日安，我以古代大师的智慧进行制作。";
    merchant_greetings[{GW::Region::Region_ShingJea, TraderType::SkillTrainer}] = L"欢迎，年轻的学徒——我将指导您学习星岬寺古老的武术之道。";

    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::Merchant}] = L"欢迎来到凯宁城，帝国最优质的货物！";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::ArmorCrafter}] = L"城市护甲，传统与创新的结合。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::WeaponCustomizer}] = L"在帝国最伟大城市中心锻造的武器。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RuneTrader}] = L"来自帝国庞大收藏的皇家符文。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::MaterialTrader}] = L"来自凯珊各个角落的城市珍宝和异国材料。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::RareMaterialTrader}] = L"来自帝国无尽市场的稀有商品。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::DyeTrader}] = L"来自无尽色彩之城的帝国色彩。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::OtherItemCrafter}] = L"日安，城市工艺与古老传统的结合。";
    merchant_greetings[{GW::Region::Region_Kaineng, TraderType::SkillTrainer}] = L"在帝国最伟大的城市，我传授最精炼的战斗技巧。";

    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::Merchant}] = L"欢迎来到我们神圣的森林，大自然的恩赐等待着你。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::ArmorCrafter}] = L"受森林精灵祝福的防护。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::WeaponCustomizer}] = L"受古老森林精灵祝福的武器。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RuneTrader}] = L"森林符文，由古老树木低语而成。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::MaterialTrader}] = L"尊重永恒森林收获的材料。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::RareMaterialTrader}] = L"大自然慷慨给予的神圣森林珍宝。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::DyeTrader}] = L"如树木般永恒的森林色彩。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::OtherItemCrafter}] = L"日安，森林之友，我以自然的祝福进行制作。";
    merchant_greetings[{GW::Region::Region_Kurzick, TraderType::SkillTrainer}] = L"永恒森林低语着它的秘密。我可以教您它古老的战斗方式。";

    merchant_greetings[{GW::Region::Region_Luxon, TraderType::Merchant}] = L"欢迎，海之友，翡翠之风带来好运。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::ArmorCrafter}] = L"受翡翠海力量祝福的防护。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::WeaponCustomizer}] = L"以永恒潮汐之力锻造的武器。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RuneTrader}] = L"由潮汐力量雕刻的海之符文。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::MaterialTrader}] = L"来自翡翠海的珍宝，由顺风带来。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::RareMaterialTrader}] = L"来自永恒之海的稀有翡翠馈赠。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::DyeTrader}] = L"如翡翠之水般变幻的海之色彩。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::OtherItemCrafter}] = L"日安，我用受翡翠海祝福的材料进行制作。";
    merchant_greetings[{GW::Region::Region_Luxon, TraderType::SkillTrainer}] = L"如同变幻的潮汐，我传授路克森舰队流畅的战斗艺术。";

    merchant_greetings[{GW::Region::Region_Istan, TraderType::Merchant}] = L"Ahlan wa sahlan，欢迎来到伊洛纳的明珠！";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::ArmorCrafter}] = L"海岛防护，轻如海风。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::WeaponCustomizer}] = L"受海岛之风和晨光祝福的武器。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RuneTrader}] = L"受海洋精灵祝福的海岛符文。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::MaterialTrader}] = L"来自伊斯坦贸易风带来的异国材料。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::RareMaterialTrader}] = L"来自隐秘海湾的珍贵海岛珍宝。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::DyeTrader}] = L"如天堂般明亮的热带色彩。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::OtherItemCrafter}] = L"日安，尊敬的旅者，最精湛的海岛工艺。";
    merchant_greetings[{GW::Region::Region_Istan, TraderType::SkillTrainer}] = L"欢迎，旅者！我传授在伊洛纳阳光下完善的优雅战斗艺术。";

    merchant_greetings[{GW::Region::Region_Kourna, TraderType::Merchant}] = L"欢迎，沙漠战士，库尔纳的货物属于强者。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::ArmorCrafter}] = L"经受半人马和海盗考验的护甲。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::WeaponCustomizer}] = L"经受半人马和海盗考验的武器。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RuneTrader}] = L"在冲突中锻造的战斗符文。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::MaterialTrader}] = L"经受阳光和生存考验的材料，在战斗中证明了自己。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::RareMaterialTrader}] = L"来自严酷大陆的战利品。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::DyeTrader}] = L"如战士勇气般大胆的沙漠色彩。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::OtherItemCrafter}] = L"日安，我在大陆的严酷火焰中锻造。";
    merchant_greetings[{GW::Region::Region_Kourna, TraderType::SkillTrainer}] = L"在库尔纳，只有强者才能生存。我传授沙漠战争的残酷艺术。";

    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::Merchant}] = L"欢迎来到法比，王子和繁荣之地！";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::ArmorCrafter}] = L"适合王子的护甲，饰以珍贵宝石。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::WeaponCustomizer}] = L"适合王子的武器，饰以珍贵宝石。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RuneTrader}] = L"来自王子收藏的皇家符文。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::MaterialTrader}] = L"黄金能买到的最优质材料，来自法比的宝库。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::RareMaterialTrader}] = L"来自皇家宝库的王子珍宝。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::DyeTrader}] = L"如宫殿丝绸般奢华的皇家色彩。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::OtherItemCrafter}] = L"日安，法比只有最精湛的工艺。";
    merchant_greetings[{GW::Region::Region_Vaabi, TraderType::SkillTrainer}] = L"我传授法比贵族王子们喜爱的优雅战斗艺术。";

    merchant_greetings[{GW::Region::Region_Desolation, TraderType::Merchant}] = L"欢迎，勇敢的灵魂，很少有人敢在这片被诅咒的土地上交易。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::ArmorCrafter}] = L"抵御恶魔和被诅咒之风的防护。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::WeaponCustomizer}] = L"为面对恶魔和不死大军而锻造的武器。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RuneTrader}] = L"被诅咒的符文，强大但危险。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::MaterialTrader}] = L"被黑暗触碰的材料，强大但危险。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::RareMaterialTrader}] = L"来自被诅咒领域的禁忌珍宝。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::DyeTrader}] = L"来自阴影领域的黑暗色彩。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::OtherItemCrafter}] = L"日安，尽管硫磺之风和黑暗，我仍在工作。";
    merchant_greetings[{GW::Region::Region_Desolation, TraderType::SkillTrainer}] = L"在这个被诅咒的领域，我传授对抗恶魔所需的禁忌技艺。";

    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::Merchant}] = L"你敢在夏尔之地交易，你的勇气令我印象深刻。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::ArmorCrafter}] = L"从夏尔锻造炉中偷来的护甲。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::WeaponCustomizer}] = L"在我们敌人的熔炉中锻造的武器。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RuneTrader}] = L"从他们萨满那里夺来的夏尔符文。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::MaterialTrader}] = L"从夏尔权力中心偷来的材料。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::RareMaterialTrader}] = L"来自敌人国土的战利品。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::DyeTrader}] = L"来自敌方领土的战争色彩。";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::OtherItemCrafter}] = L"以火焰之名，一个人类在夏尔领土寻求工艺！";
    merchant_greetings[{GW::Region::Region_CharrHomelands, TraderType::SkillTrainer}] = L"难以置信！一个人类在夏尔领土中心寻求训练。我钦佩你的胆量。";

    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::Merchant}] = L"欢迎，永恒战士，即使在这里，商业也能找到出路。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::ArmorCrafter}] = L"在永恒之火中锻造的防护。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::WeaponCustomizer}] = L"在永恒领域之火中锻造的武器。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RuneTrader}] = L"超越凡人理解的永恒符文。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::MaterialTrader}] = L"来自凡人领域之外的材料，比黄金更珍贵。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::RareMaterialTrader}] = L"来自永恒之焰领域的珍宝。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::DyeTrader}] = L"来自永恒领域的色彩。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::OtherItemCrafter}] = L"日安，我用受永恒触碰的材料进行制作。";
    merchant_greetings[{GW::Region::Region_FissureOfWoe, TraderType::SkillTrainer}] = L"在永恒之焰的领域，我传授超越凡人理解的技术。";

    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::Merchant}] = L"欢迎来到这个阴影领域，即使在这里交易也持续存在。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::ArmorCrafter}] = L"在痛苦和阴影中锻造的防护。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::WeaponCustomizer}] = L"在痛苦中锻造，以永恒苦难淬炼的武器。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RuneTrader}] = L"来自阴影领域的折磨符文。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::MaterialTrader}] = L"痛苦中诞生的材料，超越凡人理解的强大。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::RareMaterialTrader}] = L"来自永恒折磨领域的珍宝。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::DyeTrader}] = L"来自痛苦领域的阴影色彩。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::OtherItemCrafter}] = L"日安，阴影行者，我用折磨的本质进行制作。";
    merchant_greetings[{GW::Region::Region_DomainOfAnguish, TraderType::SkillTrainer}] = L"在这个无尽折磨的领域，我传授永恒苦难中诞生的黑暗技艺。";

    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::Merchant}] = L"欢迎，学者，阿苏拉有许多奇妙的发明可供交易。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::ArmorCrafter}] = L"阿苏拉技术与传统防护的结合。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::WeaponCustomizer}] = L"以阿苏拉独创性和魔法创新增强的武器。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::RuneTrader}] = L"经阿苏拉智力精炼的符文魔法。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::MaterialTrader}] = L"来自阿苏拉实验室的先进材料和技术组件。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::RareMaterialTrader}] = L"来自最先进阿苏拉研究设施的稀有组件。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::DyeTrader}] = L"通过阿苏拉卓越炼金工艺创造的颜料。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::OtherItemCrafter}] = L"日安，我使用最先进的阿苏拉方法论进行制作。";
    merchant_greetings[{GW::Region::Region_TarnishedCoast, TraderType::SkillTrainer}] = L"我传授经阿苏拉卓越智力分析增强的战斗技巧。";

    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::Merchant}] = L"欢迎来到深处，很少有地表居民敢冒险到这么远的地下。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::ArmorCrafter}] = L"在最深洞穴和地下锻造炉中锻造的防护。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::WeaponCustomizer}] = L"用泰瑞亚深处隐藏的珍宝打造的武器。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::RuneTrader}] = L"在最深地下密室中发现的古老符文。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::MaterialTrader}] = L"从世界最深矿脉中开采的材料。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::RareMaterialTrader}] = L"来自世界隐藏深处的珍贵宝石和金属。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::DyeTrader}] = L"来自地下领域的深土色彩。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::OtherItemCrafter}] = L"日安，我用来自世界最深处地方的材料工作。";
    merchant_greetings[{GW::Region::Region_DepthsOfTyria, TraderType::SkillTrainer}] = L"在很少有人敢涉足的深处，我传授地下战斗艺术。";

    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::Merchant}] = L"欢迎来到启航滩！来自世界各地的商品。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::ArmorCrafter}] = L"来自所有传统的护甲，汇聚一堂。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::WeaponCustomizer}] = L"来自世界各个角落的武器。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::RuneTrader}] = L"来自已知世界各地的符文。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::MaterialTrader}] = L"来自所有土地的材料，您需要什么？";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::RareMaterialTrader}] = L"来自遥远海岸的稀有珍宝。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::DyeTrader}] = L"来自每种文化和传统的色彩。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::OtherItemCrafter}] = L"日安，我使用来自世界各地的技术。";
    merchant_greetings[{GW::Region::Region_BattleIslands, TraderType::SkillTrainer}] = L"欢迎！我传授来自所有传统的技艺。";

    const GW::UI::UIMessage messages[] = {GW::UI::UIMessage::kDialogBody, GW::UI::UIMessage::kVendorWindow,           GW::UI::UIMessage::kAgentSpeechBubble,     GW::UI::UIMessage::kMapChange,
                                          GW::UI::UIMessage::kMapLoaded,  GW::UI::UIMessage::kPreferenceValueChanged, GW::UI::UIMessage::kDialogueMessageUpdated};
    for (auto message_id : messages) {
        RegisterUIMessageCallback(&PreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x1);
        RegisterUIMessageCallback(&UIMessage_HookEntry, message_id, OnPostUIMessage, 0x4000);
    }
    AudioSettings::RegisterPlaySoundCallback(&UIMessage_HookEntry, OnPlaySound);
    GW::UI::RegisterFrameUIMessageCallback(&FrameUIMessage_HookEntry, GW::UI::UIMessage::kDestroyFrame, OnDialogFrameDestroyed);

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
    GW::UI::RemoveFrameUIMessageCallback(&FrameUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&PreUIMessage_HookEntry);
    AudioSettings::RemovePlaySoundCallback(&UIMessage_HookEntry);
    if (OnAgentSpeechBubble_UICallback_Func) {
        GW::Hook::RemoveHook(OnAgentSpeechBubble_UICallback_Func);
        OnAgentSpeechBubble_UICallback_Func = nullptr;
    }
}

void TextToSpeechModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    if (!GetCurrentAPIConfig()) settings.current_tts_provider = api_configs[0].name;

    for (auto& config : api_configs) {
        std::string value;
        auto key_name = std::string(config.name) + "_api_key";
        if (!doc.Get(Name(), key_name, value) && legacy) value = legacy->GetValue(Name(), key_name.c_str(), "");
        strncpy(config.api_key, value.c_str(), _countof(config.api_key) - 1);
        value.clear();
        key_name = std::string(config.name) + "_user_id";
        if (!doc.Get(Name(), key_name, value) && legacy) value = legacy->GetValue(Name(), key_name.c_str(), "");
        strncpy(config.user_id, value.c_str(), _countof(config.user_id) - 1);
    }

    std::map<uint32_t, std::string> npc_voices;
    if (doc.Get(Name(), "npc_voices", npc_voices)) {
        for (const auto& [npc_id, voice_id] : npc_voices) {
            special_npc_voices[npc_id] = VoiceProfile(voice_id, 0.5f, 0.5f, 0.5f, 1.0f, "");
        }
    }
    else if (legacy) {
        TNamesDepend keys;
        legacy->GetAllKeys(Name(), keys);
        for (const auto& key : keys) {
            std::string key_name = key.pItem;
            if (key_name.find("npc_voice_") == 0) {
                uint32_t npc_id = std::stoul(key_name.substr(10));
                std::string voice_id = legacy->GetValue(Name(), key.pItem, voice_id_human_male);
                special_npc_voices[npc_id] = VoiceProfile(voice_id, 0.5f, 0.5f, 0.5f, 1.0f, "");
            }
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(GWRace::Count); i++) {
        GWRace race = static_cast<GWRace>(i);
        std::string key = std::format("play_speech_from_race_{}", GetRaceName(race));
        bool enabled = true;
        if (!doc.Get(Name(), key, enabled) && legacy) enabled = legacy->GetBoolValue(Name(), key.c_str(), true);
        play_speech_from_race[race] = enabled;
    }
}

void TextToSpeechModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    for (auto& config : api_configs) {
        auto key_name = std::string(config.name) + "_api_key";
        doc.Set(Name(), key_name, std::string(config.api_key));
        key_name = std::string(config.name) + "_user_id";
        doc.Set(Name(), key_name, std::string(config.user_id));
    }

    std::map<uint32_t, std::string> npc_voices;
    for (const auto& [npc_id, voice_profile] : special_npc_voices) {
        npc_voices[npc_id] = voice_profile.voice_id;
    }
    doc.Set(Name(), "npc_voices", npc_voices);

    for (const auto& [race, enabled] : play_speech_from_race) {
        std::string key = std::format("play_speech_from_race_{}", GetRaceName(race));
        doc.Set(Name(), key, enabled);
    }
}

void TextToSpeechModule::DrawSettingsInternal()
{
    static bool show_passwords = false;
    ImGui::Separator();
    ImGui::Text("TTS 提供商：");

    std::vector<const char*> provider_names;
    int current_provider = 0;
    for (size_t i = 0; i < _countof(api_configs); i++) {
        provider_names.push_back(api_configs[i].name);
        if (api_configs[i].name == settings.current_tts_provider) current_provider = (int)i;
    }
    if (ImGui::Combo("TTS 服务", &current_provider, provider_names.data(), (int)provider_names.size())) settings.current_tts_provider = api_configs[current_provider].name;

    ImGui::Separator();
    ImGui::Text("API 配置：");
    const ImColor col(102, 187, 238, 255);
    auto api_config = GetCurrentAPIConfig();
    bool is_api_locked_down = !(api_config && *api_config->signup_url);

    if (api_config) {
        if (!is_api_locked_down) {
            // Fill the rest of the line, leaving room for the show/hide password button
            const auto secret_input_width = [] {
                return std::max(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x, 100.f * ImGui::FontScale());
            };
            ImGui::Text("%s API Key: ", api_config->name);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(secret_input_width());
            ImGui::InputTextSecret("###current provider API Key", api_config->api_key, _countof(api_config->api_key), &show_passwords);
            if (api_config->has_user_id) {
                ImGui::Text("%s User ID：", api_config->name);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(secret_input_width());
                ImGui::InputTextSecret("###current provider User ID", api_config->user_id, _countof(api_config->user_id), &show_passwords);
            }
            ImGui::TextColored(col.Value, "点击此处获取 %s API 凭证", api_config->name);
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
        ImGui::CheckboxWithHelp("仅处理对话的第一句话", &settings.only_use_first_sentence, "如果启用，将只处理 NPC 对话的第一句话。");
        show_warning |= !settings.only_use_first_sentence;
    }
    else {
        ImGui::TextDisabled("注意：使用所选 TTS API 时，将只处理 NPC 对话的第一句话。");
    }

    ImGui::CheckboxWithHelp("仅处理 NPC 的第一段对话", &settings.only_use_first_dialog, "如果启用，将只处理 NPC 对话的第一段。");
    show_warning |= !settings.only_use_first_dialog;

    ImGui::CheckboxWithHelp("关闭 NPC 对话时播放告别消息", &settings.play_goodbye_messages, "如果启用，关闭对话时 NPC 会说随机告别语。\n\n注意：目前仅支持英语。");
    show_warning |= settings.play_goodbye_messages;

    ImGui::CheckboxWithHelp("播放商人和交易者的问候语", &settings.play_speech_from_vendors, "注意：对于某些类型的交易者，目前仅支持英语。");

    ImGui::Checkbox("对话窗口关闭时停止语音", &settings.stop_speech_when_dialog_closed);

    ImGui::TextUnformatted("在以下区域播放语音：");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("可探索区域", &settings.play_tts_in_explorable_areas);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("前哨站", &settings.play_tts_in_outposts);
    ImGui::Unindent();

    ImGui::TextUnformatted("播放对话气泡：");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("在前哨站中", &settings.play_speech_bubbles_in_outpost, "如果启用，将处理前哨站中 NPC 上方的对话气泡");
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("在可探索区域中", &settings.play_speech_bubbles_in_explorable, "如果启用，将处理可探索区域内技能和敌友对话气泡范围内的引用。");
    show_warning |= settings.play_speech_bubbles_in_explorable;
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("来自其他队伍成员", &settings.play_speech_bubbles_from_party_members, "如果启用，将处理对话气泡范围内队伍成员的技能引用。");
    show_warning |= settings.play_speech_bubbles_from_party_members;
    ImGui::NextSpacedElement();
    ImGui::Checkbox("来自非友好 NPC", &settings.play_speech_from_non_friendly_npcs);
    show_warning |= settings.play_speech_from_non_friendly_npcs;
    ImGui::Unindent();

    if (ImGui::InputFloat("NPC 对话气泡范围", &settings.npc_speech_bubble_range, GW::Constants::Range::Adjacent, GW::Constants::Range::Adjacent)) {
        settings.npc_speech_bubble_range = std::clamp(settings.npc_speech_bubble_range, 0.f, 2500.f);
    }
    ImGui::ShowHelp("处理 NPC 对话气泡的范围。设为 0 以禁用。");
    show_warning |= (settings.npc_speech_bubble_range > 166.f);

    if (show_warning && !is_api_locked_down) ImGui::TextColored(ImColor(IM_COL32(245, 245, 0, 255)), "警告：处理更多对话行将消耗更多 API 额度！");

    ImGui::TextUnformatted("从以下种族播放语音：");
    ImGui::Indent();
    ImGui::StartSpacedElements(264.f);
    for (auto& it : play_speech_from_race) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox(GetRaceName(it.first), (bool*)&it.second);
    }
    ImGui::Unindent();

    if (!is_api_locked_down) {
        ImGui::Separator();
        ImGui::Text("自定义 NPC 语音分配：");
        ImGui::Text("通过 NPC ID 为个别 NPC 分配特定语音。");
        ImGui::Text("语音设置（稳定性、相似度等）将从 NPC 的地区/性别继承。");

        ImGui::PushItemWidth(100);
        ImGui::InputTextWithHint("##npc_id_custom", "例如 1234", custom_npc_id_buffer, sizeof(custom_npc_id_buffer), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("NPC ID");
        ImGui::SameLine();
        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##voice_id_custom", "例如 2EiwWnXFnvU5JabPnv8n", custom_voice_id_buffer, sizeof(custom_voice_id_buffer));
        ImGui::SameLine();
        ImGui::Text("Voice ID");
        ImGui::PopItemWidth();

        if (ImGui::Button("添加自定义语音分配")) {
            if (strlen(custom_npc_id_buffer) > 0 && strlen(custom_voice_id_buffer) > 0) {
                try {
                    uint32_t npc_id = std::stoul(custom_npc_id_buffer);
                    special_npc_voices[npc_id] = VoiceProfile(custom_voice_id_buffer, 0.5f, 0.5f, 0.5f, 1.0f, "");
                    memset(custom_npc_id_buffer, 0, sizeof(custom_npc_id_buffer));
                    memset(custom_voice_id_buffer, 0, sizeof(custom_voice_id_buffer));
                    VoiceLog("为 NPC ID %u 添加了自定义语音分配", npc_id);
                } catch (const std::exception&) {
                    VoiceLog("输入的 NPC ID 无效");
                }
            }
        }
        ImGui::PopItemWidth();

        if (!special_npc_voices.empty()) {
            ImGui::Separator();
            ImGui::Text("当前自定义语音分配：");
            std::vector<uint32_t> to_remove;
            for (const auto& [npc_id, voice_profile] : special_npc_voices) {
                ImGui::PushID(npc_id);
                ImGui::Text("NPC ID: %u", npc_id);
                ImGui::SameLine(120);
                ImGui::Text("语音：%s", voice_profile.voice_id.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImColor(150, 150, 150), "（继承地区/性别设置）");
                ImGui::SameLine();
                if (ImGui::Button("移除")) to_remove.push_back(npc_id);
                ImGui::PopID();
            }
            for (uint32_t npc_id : to_remove) {
                special_npc_voices.erase(npc_id);
                VoiceLog("移除了 NPC ID %u 的自定义语音分配", npc_id);
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("最近活动：");
    if (voice_log_messages.empty()) {
        ImGui::TextDisabled("无最近活动");
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
