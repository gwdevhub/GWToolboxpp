#include "stdafx.h"

#include <Windows/QuestTrackerWindow.h>
#include <Defines.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/QuestProgressContractExporter.h>
#include <Modules/QuestProgressLive.h>
#include <Modules/Resources.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <Utils/FontLoader.h>
#include <Utils/TextUtils.h>

#include <bcrypt.h>

#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace {
    constexpr ImU32 TEXT_COLOR_COMPLETED = 0xffbbbbbb;
    constexpr ImU32 TEXT_COLOR_ACTIVE = 0xff00ff00;
    constexpr ImU32 TEXT_COLOR_READY = 0xff66ccff;
    constexpr auto custom_marker_quest_id = static_cast<GW::Constants::QuestID>(0x0000fdd);
    constexpr char kContractContentFingerprintExportedAt[] = "1970-01-01T00:00:00.000Z";

    std::string Sha256Hex(std::string_view bytes)
    {
        constexpr DWORD kSha256Size = 32;
        BYTE hash[kSha256Size] = {};
        if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                static_cast<ULONG>(bytes.size()), hash, kSha256Size) != 0) {
            return {};
        }
        std::ostringstream hex;
        hex << std::hex << std::setfill('0');
        for (const auto b : hash) {
            hex << std::setw(2) << static_cast<unsigned>(b);
        }
        return hex.str();
    }

    std::string CompactUtcForFilename(std::string_view exported_at_utc)
    {
        std::string out;
        out.reserve(16);
        for (const auto ch : exported_at_utc) {
            if ((ch >= '0' && ch <= '9') || ch == 'T' || ch == 'Z') {
                out.push_back(ch);
            }
        }
        return out.empty() ? "unknown" : out;
    }

    std::filesystem::path ContractExportSidecarPath(
        const std::filesystem::path& exports_folder, std::string_view character_key)
    {
        const auto key_hash = Sha256Hex(character_key);
        const auto short_hash = key_hash.size() >= 16 ? key_hash.substr(0, 16) : key_hash;
        return exports_folder / (std::string("last_content_") + short_hash + ".sha256");
    }

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return {};
        }
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    bool WriteTextFile(const std::filesystem::path& path, std::string_view contents)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return static_cast<bool>(out);
    }

    void EnqueueSetActiveQuest(GW::Constants::QuestID quest_id)
    {
        GW::GameThread::Enqueue([quest_id] {
            if (quest_id == GW::Constants::QuestID::None) return;
            if (quest_id == custom_marker_quest_id) return;
            if (!GW::Map::GetIsMapLoaded()) return;
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
            if (!GW::QuestMgr::GetQuest(quest_id)) return;
            if (GW::QuestMgr::GetActiveQuestId() == quest_id) return;
            GW::QuestMgr::SetActiveQuestId(quest_id);
        });
    }
}

void QuestTrackerWindow::Initialize()
{
    terminating_ = false;
    ToolboxWindow::Initialize();
    observation_.Initialize();
    progress_.Initialize();
    progress_.SetStoreDirectory(Resources::GetPath(L"QuestProgress"));
}

void QuestTrackerWindow::Update(float delta)
{
    const auto steady_now = std::chrono::steady_clock::now();
    const auto wall_now = std::chrono::system_clock::now();

    if (!terminating_) {
        observation_.Update(delta);

        const auto snap = observation_.AcquireSnapshot();
        const bool world_ready = snap && snap->world_ready && !snap->loading;
        const bool logout = observation_.ConsumeLogoutSignal();

        auto drain_scoped_evidence = [&]() {
            std::vector<QuestEvidenceStamp> evidence;
            observation_.DrainPendingEvidence(evidence);
            if (evidence.empty()) {
                return;
            }
            std::vector<QuestProgress::EvidenceStamp> owned;
            owned.reserve(evidence.size());
            for (const auto& e : evidence) {
                QuestProgress::EvidenceStamp s;
                s.game_quest_id = e.game_quest_id;
                s.kind = e.kind;
                s.steady_at = e.steady_at;
                // Scope captured against the currently bound identity before any rebind.
                s.account_generation = progress_.account_generation();
                s.character_generation = progress_.character_generation();
                s.account_key = progress_.identity().account_key;
                s.character_key = progress_.identity().character_key;
                owned.push_back(std::move(s));
            }
            progress_.IngestEvidence(std::move(owned));
        };

        // 1–3) Drain/classify evidence for the currently bound identity first.
        drain_scoped_evidence();

        if (logout) {
            // Explicit session end — not map-load. Do not apply a post-logout snap to the old char.
            progress_.UnbindIdentity();
        }
        else if (world_ready) {
            const auto next = QuestProgress::SampleLiveSessionIdentity(true);
            const bool character_changed =
                progress_.identity().kind != next.kind
                || progress_.identity().account_key != next.account_key
                || progress_.identity().character_key != next.character_key;
            const uint64_t barrier_rev = snap ? snap->revision : 0;
            // 5–7) Flush/retain old session if needed, then bind new identity.
            // Never feed the live snap into the outgoing character — it already belongs to `next`.
            progress_.BindIdentity(
                next,
                false,
                character_changed ? barrier_rev : progress_.snapshot_barrier_revision(),
                character_changed);
            if (character_changed) {
                observation_.RequestFullRefresh();
                hom_requested_character_.clear();
                hom_ingested_fingerprint_.clear();
            }
            // 8) Evidence arriving after rebind is scoped to the new identity only.
            drain_scoped_evidence();
        }

        // 9) Ingest snapshot for the active bound identity (map-load skips via loading flag).
        // Identity barrier rejects pre-bind / cross-character published views.
        if (snap && progress_.identity().kind != QuestProgress::IdentityKind::Unbound) {
            progress_.IngestSnapshot(QuestProgress::ToQuestSnapshot(*snap), wall_now, steady_now);
            if (world_ready
                && progress_.identity().kind == QuestProgress::IdentityKind::Persistent) {
                const auto missions = QuestProgress::SampleLiveMissionCompletion(wall_now);
                progress_.IngestMissionCompletion(std::move(missions));

                const auto* stored = [&]() -> const QuestProgress::StoredCharacter* {
                    for (const auto& [key, ch] : progress_.account_store().characters) {
                        if (key == progress_.identity().character_key) {
                            return &ch;
                        }
                    }
                    return nullptr;
                }();
                const auto journey = QuestProgress::SampleLiveJourneySnapshot(
                    wall_now,
                    stored ? stored->titles : std::map<uint32_t, QuestProgress::TitleStateRecord>{},
                    stored ? stored->last_known_level : std::nullopt,
                    stored ? stored->last_map_id : std::nullopt,
                    stored ? stored->journey_events : std::vector<QuestProgress::JourneyEventRecord>{});
                progress_.IngestJourneySnapshot(std::move(journey));

                std::vector<JourneyMilestoneHint> hints;
                observation_.DrainPendingJourneyHints(hints);
                if (!hints.empty()) {
                    const auto* after = [&]() -> const QuestProgress::StoredCharacter* {
                        for (const auto& [key, ch] : progress_.account_store().characters) {
                            if (key == progress_.identity().character_key) {
                                return &ch;
                            }
                        }
                        return nullptr;
                    }();
                    const auto& prior = after ? after->journey_events
                                             : std::vector<QuestProgress::JourneyEventRecord>{};
                    std::vector<QuestProgress::JourneyEventRecord> timed;
                    for (const auto& hint : hints) {
                        const auto observed_at = QuestProgress::FormatCanonicalUtc(hint.wall_at);
                        if (!QuestProgress::IsCanonicalUtcTimestamp(observed_at)) {
                            continue;
                        }
                        QuestProgress::AppendUniqueJourneyEvents(
                            timed,
                            QuestProgress::BuildTimedMapClearEvents(
                                hint.kind, hint.map_id, prior, observed_at));
                    }
                    progress_.IngestJourneyEvents(std::move(timed));
                }

                MaybeRefreshHallOfMonuments();
            }
        }
    }

    progress_.Tick(steady_now, wall_now);
}

void QuestTrackerWindow::MaybeRefreshHallOfMonuments()
{
    if (terminating_) {
        return;
    }
    if (progress_.identity().kind != QuestProgress::IdentityKind::Persistent) {
        return;
    }
    const auto display = progress_.identity().display_name;
    if (display.empty()) {
        return;
    }
    const auto wide_name = TextUtils::StringToWString(display);

    if (hom_achievements_.isReady() && hom_requested_character_ == wide_name) {
        const auto fingerprint = std::format(
            "{}:{}:{}:{}:{}:{}:{}:{}:{}:{}",
            hom_achievements_.hom_code,
            hom_achievements_.resilience_points_total,
            hom_achievements_.fellowship_points_total,
            hom_achievements_.honor_points_total,
            hom_achievements_.valor_points_total,
            hom_achievements_.devotion_points_total,
            hom_achievements_.resilience_tally,
            hom_achievements_.fellowship_tally,
            hom_achievements_.honor_tally,
            hom_achievements_.valor_tally);
        if (fingerprint != hom_ingested_fingerprint_) {
            QuestProgress::HomSnapshotRecord snap;
            snap.hom_code = hom_achievements_.hom_code;
            snap.observed_at = QuestProgress::FormatCanonicalUtc(std::chrono::system_clock::now());
            snap.resilience_points = hom_achievements_.resilience_points_total;
            snap.fellowship_points = hom_achievements_.fellowship_points_total;
            snap.honor_points = hom_achievements_.honor_points_total;
            snap.valor_points = hom_achievements_.valor_points_total;
            snap.devotion_points = hom_achievements_.devotion_points_total;
            snap.resilience_dedicated.clear();
            snap.fellowship_dedicated.clear();
            snap.honor_dedicated.clear();
            snap.valor_dedicated.clear();
            snap.devotion_counts.clear();
            for (size_t i = 0; i < static_cast<size_t>(ResilienceDetail::Count); ++i) {
                snap.resilience_dedicated.push_back(hom_achievements_.resilience_detail[i] ? 1u : 0u);
            }
            for (size_t i = 0; i < static_cast<size_t>(FellowshipDetail::Count); ++i) {
                snap.fellowship_dedicated.push_back(hom_achievements_.fellowship_detail[i] ? 1u : 0u);
            }
            for (size_t i = 0; i < static_cast<size_t>(HonorDetail::Count); ++i) {
                snap.honor_dedicated.push_back(hom_achievements_.honor_detail[i] ? 1u : 0u);
            }
            for (size_t i = 0; i < static_cast<size_t>(ValorDetail::Count); ++i) {
                snap.valor_dedicated.push_back(hom_achievements_.valor_detail[i] ? 1u : 0u);
            }
            for (size_t i = 0; i < static_cast<size_t>(DevotionDetail::Count); ++i) {
                snap.devotion_counts.push_back(hom_achievements_.devotion_detail[i]);
            }
            progress_.IngestHomSnapshot(std::move(snap));
            hom_ingested_fingerprint_ = fingerprint;
        }
        return;
    }

    if (hom_achievements_.isLoading()) {
        return;
    }
    if (hom_requested_character_ == wide_name
        && hom_achievements_.state == HallOfMonumentsAchievements::State::Error) {
        return;
    }

    hom_requested_character_ = wide_name;
    hom_ingested_fingerprint_.clear();
    hom_achievements_ = HallOfMonumentsAchievements{};
    HallOfMonumentsModule::AsyncGetAccountAchievements(wide_name, &hom_achievements_);
}

void QuestTrackerWindow::SignalTerminate()
{
    terminating_ = true;
    progress_.SignalTerminate();
    observation_.SignalTerminate();
    ToolboxWindow::SignalTerminate();
}

void QuestTrackerWindow::Terminate()
{
    terminating_ = true;
    for (int i = 0; i < 500 && hom_achievements_.isLoading(); ++i) {
        Sleep(10);
    }
    if (hom_achievements_.isLoading()) {
        hom_achievements_.state = HallOfMonumentsAchievements::State::Error;
    }
    progress_.Terminate();
    ClearDecodeCache();
    observation_.Terminate();
    ToolboxWindow::Terminate();
}

void QuestTrackerWindow::ClearDecodeCache()
{
    name_decoders_.clear();
    quest_objective_decoders_.clear();
    mission_objective_decoders_.clear();
    cached_revision_ = 0;
}

void QuestTrackerWindow::SyncDecodeCache(const LiveQuestView& view)
{
    if (cached_revision_ == view.revision) return;
    cached_revision_ = view.revision;

    std::unordered_map<GW::Constants::QuestID, std::unique_ptr<GuiUtils::EncString>> next_names;
    std::unordered_map<ObjectiveKey, std::unique_ptr<GuiUtils::EncString>, ObjectiveKeyHash> next_objectives;
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> next_mission;

    for (const auto& quest : view.quests) {
        if (auto it = name_decoders_.find(quest.quest_id); it != name_decoders_.end()) {
            next_names.emplace(quest.quest_id, std::move(it->second));
        }
        for (size_t i = 0; i < quest.objectives.size(); ++i) {
            ObjectiveKey key{quest.quest_id, i};
            if (auto it = quest_objective_decoders_.find(key); it != quest_objective_decoders_.end()) {
                next_objectives.emplace(key, std::move(it->second));
            }
        }
    }
    for (const auto& objective : view.mission_objectives) {
        if (auto it = mission_objective_decoders_.find(objective.objective_id); it != mission_objective_decoders_.end()) {
            next_mission.emplace(objective.objective_id, std::move(it->second));
        }
    }

    name_decoders_ = std::move(next_names);
    quest_objective_decoders_ = std::move(next_objectives);
    mission_objective_decoders_ = std::move(next_mission);
}

GuiUtils::EncString& QuestTrackerWindow::NameDecoder(GW::Constants::QuestID quest_id, const std::wstring& encoded)
{
    auto& ptr = name_decoders_[quest_id];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

GuiUtils::EncString& QuestTrackerWindow::QuestObjectiveDecoder(
    GW::Constants::QuestID quest_id, size_t index, const std::wstring& encoded)
{
    ObjectiveKey key{quest_id, index};
    auto& ptr = quest_objective_decoders_[key];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

GuiUtils::EncString& QuestTrackerWindow::MissionObjectiveDecoder(uint32_t objective_id, const std::wstring& encoded)
{
    auto& ptr = mission_objective_decoders_[objective_id];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

void QuestTrackerWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(320.f, 400.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }

    const auto snap = observation_.AcquireSnapshot();
    if (!snap || snap->loading || !snap->world_ready) {
        ImGui::TextUnformatted("Loading...");
        ImGui::End();
        return;
    }

    SyncDecodeCache(*snap);

    if (ImGui::CollapsingHeader("Data for Tyrian Wayfarer")) {
        ImGui::PushTextWrapPos();
        ImGui::TextUnformatted(
            "This window is the progress bridge for Tyrian Wayfarer (not only a live quest list).");
        ImGui::Spacing();
        ImGui::TextUnformatted("Always recorded while Quest Tracker is loaded:");
        ImGui::BulletText("Quest log, objectives, and append-only history");
        ImGui::BulletText("Missions, journey milestones, professions, level/XP/SP/factions");
        ImGui::BulletText("Hall of Monuments (async; may lag the live session)");
        ImGui::TextDisabled("Quest disappearance is never treated as confirmed completion.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Export Contract v1:");
        ImGui::BulletText("QuestProgress/exports/…_contract_v1_<UTC>_<key>.json");
        ImGui::BulletText("Also updates QuestProgress/quest_progress_contract_v1.json (latest)");
        ImGui::BulletText("Skipped when progress is unchanged since the last export");
        ImGui::Spacing();
        ImGui::TextUnformatted("Optional (not imported by Wayfarer beta yet):");
        ImGui::BulletText("Drops — enable Item Settings → Drop Tracking Enabled");
        ImGui::BulletText("Completion — character_completion.json from Completion window");
        ImGui::BulletText("Account inventory / Objective Timer runs — separate Toolbox files");
        ImGui::Spacing();
        ImGui::TextUnformatted("In Tyrian Wayfarer: Settings → GWToolbox quest progress → import the JSON.");
        ImGui::PopTextWrapPos();
        ImGui::Separator();
    }

    if (ImGui::Button("Export Contract v1")) {
        ExportContractV1();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(new file only if progress changed)");
    ImGui::Separator();

    ImGui::TextUnformatted("Quest log");
    ImGui::Separator();

    const bool allow_select = !terminating_ && !snap->mission_mode;

    if (snap->quests.empty()) {
        ImGui::TextDisabled("No quests in log");
    }
    else {
        for (const auto& quest : snap->quests) {
            const bool is_active = !snap->mission_mode && quest.quest_id == snap->active_quest_id;
            auto& name_decoder = NameDecoder(quest.quest_id, quest.name_encoded);
            const char* name = name_decoder.string().c_str();
            if (!name || !*name) {
                name = name_decoder.IsDecoding() ? "..." : "(unnamed quest)";
            }

            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(quest.quest_id)));
            const ImVec2 row_pos = ImGui::GetCursorScreenPos();
            const float row_width = ImGui::GetContentRegionAvail().x;
            const float row_height = ImGui::GetTextLineHeightWithSpacing();

            if (allow_select) {
                if (ImGui::InvisibleButton("##quest_row", ImVec2(row_width, row_height))) {
                    if (!is_active) {
                        const auto quest_id = quest.quest_id;
                        EnqueueSetActiveQuest(quest_id);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to set active quest");
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                ImGui::SetCursorScreenPos(row_pos);
            }

            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_ACTIVE);
            }
            ImGui::TextUnformatted(name);
            if (is_active) {
                ImGui::PopStyleColor();
            }

            if (quest.in_log_completed) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_READY);
                ImGui::TextUnformatted("[ready]");
                ImGui::PopStyleColor();
            }

            if (allow_select) {
                ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_height));
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Active quest details");
    ImGui::Separator();

    if (snap->mission_mode) {
        ImGui::TextDisabled("Mission mode active — see mission objectives below");
    }
    else if (snap->active_quest_id == GW::Constants::QuestID::None) {
        ImGui::TextDisabled("No active quest selected");
    }
    else {
        const OwnedQuestEntry* active = nullptr;
        for (const auto& quest : snap->quests) {
            if (quest.quest_id == snap->active_quest_id) {
                active = &quest;
                break;
            }
        }
        if (!active) {
            ImGui::TextDisabled("Active quest not present in log");
        }
        else if (active->objectives_missing) {
            ImGui::TextDisabled("Objectives pending...");
        }
        else if (active->objectives.empty()) {
            ImGui::TextDisabled("No objectives");
        }
        else {
            for (size_t i = 0; i < active->objectives.size(); ++i) {
                const auto& objective = active->objectives[i];
                auto& decoder = QuestObjectiveDecoder(active->quest_id, i, objective.encoded);
                const char* text = decoder.string().c_str();
                if (!text || !*text) {
                    text = decoder.IsDecoding() ? "..." : "(objective)";
                }
                if (objective.completed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
                }
                ImGui::Bullet();
                ImGui::TextUnformatted(text);
                if (objective.completed) {
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    if (snap->mission_mode) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Mission objectives");
        ImGui::Separator();

        if (snap->mission_objectives.empty()) {
            ImGui::TextDisabled("No mission objectives");
        }
        else {
            for (const auto& objective : snap->mission_objectives) {
                auto& decoder = MissionObjectiveDecoder(objective.objective_id, objective.enc);
                const char* text = decoder.string().c_str();
                if (!text || !*text) {
                    text = decoder.IsDecoding() ? "..." : "(objective)";
                }
                if (objective.completed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
                }
                ImGui::Bullet();
                ImGui::TextUnformatted(text);
                if (objective.completed) {
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    ImGui::End();
}

void QuestTrackerWindow::ExportContractV1()
{
    if (progress_.identity().kind != QuestProgress::IdentityKind::Persistent
        || progress_.identity().character_key.empty()) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL,
            L"Quest Tracker: Contract export requires a logged-in character with a persistent identity");
        return;
    }

    progress_.Flush(false);

    const auto character = progress_.BuildExportCharacterSnapshot();
    if (!character) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: Contract export failed (invalid character identity)");
        return;
    }

    QuestProgress::AccountProgressStore export_store;
    export_store.account_key = progress_.account_store().account_key;
    export_store.store_format = QuestProgress::kStoreFormatId;
    export_store.store_version = {QuestProgress::kStoreFormatMajor, QuestProgress::kStoreFormatMinor};
    export_store.characters.emplace(character->character_key, *character);

    QuestProgress::ContractExportOptions fingerprint_options;
    fingerprint_options.exported_at_utc = kContractContentFingerprintExportedAt;
    fingerprint_options.bound_character_key = character->character_key;
    const auto fingerprint_export =
        QuestProgress::ExportAccountStoreToContractV1(export_store, fingerprint_options);
    if (fingerprint_export.status != QuestProgress::ContractExportStatus::Ok) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: Contract export failed");
        return;
    }
    const auto content_fingerprint = Sha256Hex(fingerprint_export.utf8_json);
    if (content_fingerprint.empty()) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: Contract export fingerprint failed");
        return;
    }

    const auto folder = Resources::GetPath(L"QuestProgress");
    const auto exports_folder = folder / L"exports";
    Resources::EnsureFolderExists(folder);
    Resources::EnsureFolderExists(exports_folder);
    const auto sidecar = ContractExportSidecarPath(exports_folder, character->character_key);
    const auto previous_fingerprint = TextUtils::trim(ReadTextFile(sidecar));
    if (!previous_fingerprint.empty() && previous_fingerprint == content_fingerprint) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL,
            L"Quest Tracker: Contract unchanged since last export — skipped (no new file)");
        return;
    }

    const auto exported_at = QuestProgress::FormatCanonicalUtc(std::chrono::system_clock::now());
    QuestProgress::ContractExportOptions options;
    options.exported_at_utc = exported_at;
    options.producer_version = GWTOOLBOXDLL_VERSION;
    options.bound_character_key = character->character_key;
    const auto exported = QuestProgress::ExportAccountStoreToContractV1(export_store, options);
    if (exported.status != QuestProgress::ContractExportStatus::Ok) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: Contract export failed");
        return;
    }

    const auto key_hash = Sha256Hex(character->character_key);
    const auto short_key = key_hash.size() >= 8 ? key_hash.substr(0, 8) : key_hash;
    const auto stamped_name = std::format(
        "quest_progress_contract_v1_{}_{}.json",
        CompactUtcForFilename(exported_at),
        short_key);
    const auto stamped_location = exports_folder / stamped_name;
    const auto latest_location = folder / L"quest_progress_contract_v1.json";

    if (!WriteTextFile(stamped_location, exported.utf8_json)
        || !WriteTextFile(latest_location, exported.utf8_json)
        || !WriteTextFile(sidecar, content_fingerprint)) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: could not write Contract export file");
        return;
    }

    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const auto message = stamped_location.wstring();
    constexpr size_t max_len = _countof(file_location_wc) - 1;
    for (size_t i = 0; i < message.length(); i++) {
        if (!message[i]) {
            break;
        }
        if (message[i] == L'\\') {
            file_location_wc[msg_len++] = message[i];
        }
        if (msg_len >= max_len) {
            break;
        }
        file_location_wc[msg_len++] = message[i];
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(
        chat_message,
        _countof(chat_message),
        L"Quest progress Contract v1 exported to <a=1>\x200C%s</a> (%zu quests)",
        file_location_wc,
        exported.diagnostics.quests_exported);
    WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}
