#include "stdafx.h"

#include <Windows/QuestTrackerWindow.h>
#include <Defines.h>
#include <Modules/QuestProgressContractExporter.h>
#include <Modules/QuestProgressLive.h>
#include <Modules/Resources.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <Utils/FontLoader.h>

#include <chrono>
#include <fstream>

namespace {
    constexpr ImU32 TEXT_COLOR_COMPLETED = 0xffbbbbbb;
    constexpr ImU32 TEXT_COLOR_ACTIVE = 0xff00ff00;
    constexpr ImU32 TEXT_COLOR_READY = 0xff66ccff;
    constexpr auto custom_marker_quest_id = static_cast<GW::Constants::QuestID>(0x0000fdd);

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
                // Force a fresh identity-stamped observation; do not reuse pre-bind LiveQuestView.
                observation_.RequestFullRefresh();
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
            }
        }
    }

    progress_.Tick(steady_now, wall_now);
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

    if (ImGui::Button("Export Contract v1")) {
        ExportContractV1();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(Codex import)");
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

    // Flush dirty progress before reading the in-memory account store.
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

    QuestProgress::ContractExportOptions options;
    options.exported_at_utc = QuestProgress::FormatCanonicalUtc(std::chrono::system_clock::now());
    options.producer_version = GWTOOLBOXDLL_VERSION;
    options.bound_character_key = character->character_key;

    const auto exported = QuestProgress::ExportAccountStoreToContractV1(export_store, options);
    if (exported.status != QuestProgress::ContractExportStatus::Ok) {
        WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: Contract export failed");
        return;
    }

    const auto folder = Resources::GetPath(L"QuestProgress");
    Resources::EnsureFolderExists(folder);
    const auto file_location = folder / L"quest_progress_contract_v1.json";

    {
        std::ofstream out(file_location, std::ios::binary | std::ios::trunc);
        if (!out) {
            WriteChat(GW::Chat::CHANNEL_GLOBAL, L"Quest Tracker: could not write Contract export file");
            return;
        }
        out << exported.utf8_json;
    }

    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const auto message = file_location.wstring();
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
