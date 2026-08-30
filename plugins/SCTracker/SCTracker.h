#pragma once

#include <ToolboxPlugin.h>
#include <PluginUtils.h>

#include <GWCA/Utilities/Hook.h>

// unique_ptr<AsyncRestClient> below needs the complete type: ~SCTracker() is defaulted inline in
// this header, so unique_ptr's deleter is instantiated here too, not just where AsyncRestClient is used.
#include <RestClient.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GW::Constants {
    enum class SkillID : uint32_t;
}

// Client-side data collection for a GW1 speedclear run tracker (Go backend, Postgres, React frontend):
// this plugin is the thing that actually runs inside the game and feeds that backend. GWToolboxdll's
// own Objective Timer already records per-run timing/objective data (runs/ObjectiveTimerRuns_*.json),
// but has no concept of party composition or how a run ended - this plugin fills that gap without
// requiring any changes to GWToolboxdll itself:
//
//   - Captures who was in the party (players/heroes/henchmen + professions) and how each tracked
//     explorable-area run ended (wipe/resign/completed/unknown), keyed by UTC start time so it lines up with
//     GWToolboxdll's own ObjectiveTimerRuns_*.json entries for the same run.
//   - Only for instances GWToolboxdll's ObjectiveTimerWindow actually tracks (kTrackedMapIds) - random
//     missions/vanquishes/etc. are skipped entirely, since they'd never correlate with anything.
//   - Periodically (see PendingSyncEntry) reads both its own local PartyLog_*.json and GWToolboxdll's
//     ObjectiveTimerRuns_*.json - the durable, network-independent source of truth - and publishes the
//     combined party+objective payload for each run to the backend, machine-key authenticated. Only
//     advances the persisted watermark on confirmed success, so a slow GWToolboxdll write, a network
//     blip, or the game closing mid-publish just gets retried/caught up on a later sweep instead of
//     losing data or double-reporting a run.
class SCTracker : public ToolboxPlugin {
public:
    SCTracker() = default;
    ~SCTracker() override = default;

    const char* Name() const override { return "SCTracker"; }

    [[nodiscard]] bool HasSettings() const override { return true; }
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void Terminate() override;
    void Update(float delta) override;
    // Renders the post-run vote popup only (show_vote_popup gates it) - everything else about this
    // plugin is background data collection with no always-on window.
    void Draw(IDirect3DDevice9*) override;
    // PluginModule::Draw only calls Draw() at all when this returns a non-null pointer to a true
    // bool (GWToolboxdll/Modules/PluginModule.cpp) - without an override here (base default is
    // nullptr), Draw()/DrawVotePopup() never runs, so the popup can never render regardless of
    // show_vote_popup. Aliasing that same flag means the host only calls Draw() when there's
    // actually something to show, and ImGui::Begin's close button (which writes through this same
    // pointer) naturally stops it being called again once dismissed. Returning nullptr while
    // can_report_failures is false also hides the plugin list's manual "Visible" checkbox
    // (PluginModule.cpp's DrawSettings, `if (plugin->instance->GetVisiblePtr())`) - otherwise an
    // unpermitted user could still tick that box, and though DrawVotePopup's own permission check
    // means nothing would actually render, the checkbox would sit there checked and inert.
    [[nodiscard]] bool* GetVisiblePtr() override { return can_report_failures ? &show_vote_popup : nullptr; }
    // Destroying an in-flight publish_request/submit_request blocks (joins the background HTTP
    // thread); deferring unload until both are done avoids freezing the host UI on plugin disable.
    bool CanTerminate() override
    {
        return (!publish_request || publish_request->IsCompleted())
            && (!submit_request || submit_request->IsCompleted())
            && (!permission_request || permission_request->IsCompleted())
            && (!version_check_request || version_check_request->IsCompleted());
    }

    struct PartyMember {
        std::string name;
        uint32_t primary = 0;   // GW::Constants::Profession
        uint32_t secondary = 0; // GW::Constants::Profession
        bool is_player = false;
        bool is_hero = false;
        bool is_henchman = false;
        uint32_t deaths = 0; // count of alive->dead transitions seen for this member during the run
        // English names of every kTrackedSkillNameSet skill the LOCAL PLAYER (this run's uploader) has
        // used at least once during the run, if they're themselves Ranger/Assassin (2/7) - see
        // OnSkillUsed. Always empty on every other party member's entry: tracking anyone else's skill
        // usage this way is unreliable (only ever observed when they're within compass range of the
        // uploader), so the client doesn't attempt it at all. Deduped.
        std::vector<std::string> role_skills;
        // "t1"/"t2"/"t3" once role_skills satisfies one of kRoleCombos, else "unknown". Set once and
        // never overwritten afterward. Like role_skills, only ever non-"unknown" on the uploader's own
        // entry - this is the uploader's own role, self-determined from their own skill usage, never a
        // guess about someone else.
        std::string role_hint = "unknown";
        struct ItemDropCount {
            uint32_t id = 0; // model_id (GW::Constants::ItemID) of a kTrackedItems entry
            uint32_t count = 0;
        };
        // One entry per kTrackedItems model_id reserved for this member at least once during the run.
        // Reflects initial loot reservation (GAME_SMSG_ITEM_UPDATE_OWNER), not confirmed pickup - see
        // OnItemUpdateOwner.
        std::vector<ItemDropCount> item_drops;
        // Net Ghastly Summoning Stone (GW::Constants::ItemID::GhastlyStone, model_id 32557; confirmed
        // via live capture 2026-08-26) count for this member: -1 per stone they manually dropped, +1
        // per stone they picked up (quantity-weighted on both sides), summed across the run.
        // std::nullopt (serializes as JSON null) means this member never participated at all -
        // deliberately distinct from a real 0 (e.g. dropped one stone and later picked their own back
        // up uncontested - a legitimate zero-sum outcome for an actual participant). Both drop and
        // pickup are resolved entirely from their own chat message in OnWriteToChatLog (item identity
        // via a raw substring match, exact quantity via an embedded numeric parameter - see
        // MessageContainsGhastlySummoningStone/GetGamblingStoneQuantity in SCTracker.cpp) rather than
        // via GAME_SMSG_ITEM_UPDATE_OWNER: that packet was confirmed via live testing to not fire at
        // all for a self-pickup of a self-dropped item, so OnItemUpdateOwner is not involved here.
        std::optional<int32_t> gambling_stone_net;
    };

private:
    // Only tracks instances GWToolboxdll's own ObjectiveTimerWindow would create an ObjectiveSet for
    // (see kTrackedMapIds) - skips capture/hooks entirely for everything else (random missions,
    // vanquishes, etc.), so ProcessSync never has to hold/wait out an entry that can never find a
    // matching objective log. DoA is deliberately excluded: it's not in ObjectiveTimerWindow's map_id switch at
    // all (it's gated on a different packet's map_fileID, since DoA shares its map_id with the solo
    // Mallyx mission) - out of scope here per explicit instruction.
    void OnInstanceLoadInfo(uint32_t map_id, bool is_explorable);
    void OnGameSrvTransfer();
    void OnPartyDefeated();
    void OnWriteToChatLog(const wchar_t* message);
    void OnUpdateAgentState(uint32_t agent_id, uint32_t state);
    void OnAgentUpdateAllegiance(uint32_t agent_id, uint32_t allegiance_bits);
    void OnObjectiveDone(uint32_t objective_id);
    void OnSkillUsed(uint32_t agent_id, GW::Constants::SkillID skill_id);
    void FlushPendingRoleSkills();
    void ProcessTrackedSkillUse(const std::string& skill_name);
    void OnItemGeneral(uint32_t item_id, uint32_t model_id);
    void OnItemUpdateOwner(uint32_t item_id, uint32_t owner_agent_id);
    // Single choke point for both the drop and pickup sides of the gambling-stone ritual, both
    // resolved entirely from their own chat message - see PartyMember::gambling_stone_net and
    // OnWriteToChatLog.
    void AddGamblingStoneDelta(size_t party_index, int32_t delta);
    void CaptureParty();
    void WriteLogEntry(uint32_t utc_start, uint32_t map_id, const std::string& character_name,
                        const std::string& end_reason, const std::vector<PartyMember>& members);

    // PluginUtils::EncString has no safe way to detach from a pending GW::UI::AsyncDecodeStr callback
    // before destruction (unlike GWToolboxdll's internal GuiUtils::EncString, which has AbandonDecode()).
    // So a new run's capture can only start once every in-flight EncString from the previous one has
    // finished decoding — restart_requested + next_* stage the new run until that's safe.
    bool restart_requested = false;
    uint32_t next_utc_start = 0;
    uint32_t next_map_id = 0;
    std::string next_character_name;

    bool active_capture = false;
    uint32_t pending_utc_start = 0;
    uint32_t pending_map_id = 0;
    std::string pending_character_name;
    std::vector<PartyMember> party_members;
    std::vector<std::unique_ptr<PluginUtils::EncString>> party_member_enc_names;

    // Death tracking for the in-progress run: agent_id -> index into party_members, populated as each
    // member is captured (agent_id is known at that point even before capture fully completes). Not
    // serialized itself - only the resulting PartyMember::deaths counts are. currently_dead is parallel
    // to party_members and used purely to detect the alive->dead edge (AgentState can repeat/re-send the
    // same bit), so a later resurrection doesn't get double-counted and a second death after that does.
    std::unordered_map<uint32_t, size_t> agent_id_to_party_index;
    std::vector<bool> party_member_currently_dead;

    // skill_id -> decoded English name, populated lazily (once per distinct skill_id the local player
    // ever uses, not per cast) so role tracking matches by name instead of the numeric SkillID enum
    // ordinal, which shifts whenever GWCA's Skills.h header gains/loses an entry anywhere earlier in
    // the list. Persists across runs deliberately (skill names don't change mid-session) rather than
    // being reset in OnInstanceLoadInfo.
    std::unordered_map<uint32_t, std::unique_ptr<PluginUtils::EncString>> skill_name_cache;
    struct PendingRoleSkillEvent {
        uint32_t skill_id = 0; // key into skill_name_cache
    };
    // OnSkillUsed calls for a skill_id whose name hadn't finished decoding yet when it fired; drained
    // by FlushPendingRoleSkills (called from Update) once its cache lookup is ready. Always about the
    // local player - OnSkillUsed only ever queues an event after confirming that.
    std::vector<PendingRoleSkillEvent> pending_role_skill_events;

    // item_id -> model_id for tracked-item drops seen via ItemGeneral but not yet resolved to an
    // owner. Erased once OnItemUpdateOwner counts it (or the owner isn't a tracked party member), so a
    // later reservation reassignment for the same item_id isn't double-counted. Reset every run.
    std::unordered_map<uint32_t, uint32_t> tracked_item_id_to_model_id;

    // Run outcome tracking: reset on every run start (OnInstanceLoadInfo), finalized and logged when the
    // run ends (OnGameSrvTransfer). The actual write is deferred to run-end rather than capture-completion
    // so the log entry can record how the run finished.
    bool run_active = false;
    bool wipe_detected = false;
    std::unordered_set<uint32_t> resigned_login_numbers;
    // Set once Dhuum's agent spawns hostile (see OnAgentUpdateAllegiance) and left set for the rest
    // of the run. Deaths after this point are deliberate/expected (e.g. the Dhuum tank) rather than
    // run-ending mistakes, so OnUpdateAgentState stops incrementing PartyMember::deaths once this is set.
    bool dhuum_started = false;
    // Set once the native Dhuum mission-objective completes (see OnObjectiveDone) and left set for the
    // rest of the run. OnItemGeneral stops counting Glob of Ectoplasm drops into item_drops once this
    // is set - other tracked items are unaffected.
    bool dhuum_completed = false;

    uint32_t last_written_utc_start = 0; // for DrawSettings status display only

    GW::HookEntry InstanceLoadInfo_HookEntry;
    GW::HookEntry GameSrvTransfer_HookEntry;
    GW::HookEntry PartyDefeated_HookEntry;
    GW::HookEntry WriteToChatLog_HookEntry;
    GW::HookEntry AgentState_HookEntry;
    GW::HookEntry AgentUpdateAllegiance_HookEntry;
    GW::HookEntry GenericValueSelf_HookEntry;
    GW::HookEntry GenericValueTarget_HookEntry;
    GW::HookEntry ItemGeneral_HookEntry;
    GW::HookEntry ItemUpdateOwner_HookEntry;
    GW::HookEntry ObjectiveDone_HookEntry;

    // --- Backend sync ---
    void ProcessSync();

    struct PendingSyncEntry {
        uint32_t utc_start = 0;
        uint32_t map_id = 0;
        std::string character_name;
        std::string end_reason;
        std::vector<PartyMember> party_members;
        // GetTickCount64() when this run was first discovered by FindNextPendingEntry; set once and
        // never touched again while held - this is what makes the give-up-waiting timeout correct.
        uint64_t first_seen_tick = 0;
    };
    // Returns the single oldest not-yet-published run across today's/yesterday's PartyLog_*.json
    // files (or nullopt), i.e. the next candidate for pending_sync. Called when pending_sync is
    // empty (the periodic scan gate in ProcessSync) or immediately after resolving the previously-
    // held entry, so several already-known-disqualified runs can still drain within one
    // ProcessSync() tick instead of one per scan interval.
    std::optional<PendingSyncEntry> FindNextPendingEntry();

    std::string machine_key;
    char machine_key_buf[128] = "";

    uint32_t last_persisted_utc_start = 0; // persisted setting; watermark, only advances on confirmed publish
    std::wstring settings_folder;          // cached from LoadSettings/SaveSettings so a successful publish
                                            // can persist the advanced watermark immediately, not just on
                                            // whatever cadence the host calls SaveSettings.

    // At most one run is ever unpublished at a time in practice (a run finishes every few minutes
    // and publish is fast), and processing is always strictly oldest-first by utc_start - so a
    // scalar holding the single oldest unresolved entry is sufficient; no backlog/ordering container
    // is needed.
    std::optional<PendingSyncEntry> pending_sync;
    std::unique_ptr<AsyncRestClient> publish_request;
    uint32_t publishing_utc_start = 0; // utc_start of the entry publish_request is currently sending
    uint64_t last_queue_scan_tick = 0;
    uint64_t last_publish_attempt_tick = 0; // backoff timer, only advanced on a failed publish

    // --- Post-run voting ---
    // Two vote kinds share one popup/trigger/defer/cancel pipeline: Failure (wipe/resign, blame a
    // role) and Mvp (completed, credit a role) - both use the same role vocabulary/UI mechanics
    // (kVoteRoles in the .cpp). Popup opens immediately at run-end from OnGameSrvTransfer's locally-
    // known end_reason (not from ProcessSync/GWToolboxdll's delayed file) - see OpenVote. Submission
    // is deferred until the server-assigned run_id is known (see ProcessSync's correlation block and
    // FireVoteSubmit), since run_id doesn't exist until the run publishes successfully.
    //
    // can_report_failures gates all of it, same as before - deliberately NOT renamed even though it
    // now gates two vote kinds (reusing the existing can-report-run-failure permission check, no new
    // permission endpoint).
    void RequestReportPermission();  // fires permission_request; called once from LoadSettings
    void ProcessPermissionCheck();   // polls permission_request completion; called from Update
    void DrawVotePopup();
    void ProcessVoteSubmit(); // polls submit_request completion; called from Update

    bool can_report_failures = false;
    std::unique_ptr<AsyncRestClient> permission_request;

    enum class PostRunVoteKind : uint8_t { None, Failure, Mvp };

    // Opens the popup for utc_start's run (on map_id, which decides which vote roles are offered -
    // see DrawVotePopup), unless a different run's ALREADY-COMMITTED vote is still awaiting its
    // run_id (see OpenVote's own comment) - an uncommitted/ignored vote can never reach that state,
    // since DrawVotePopup's auto-close does a full reset unless something was committed.
    void OpenVote(PostRunVoteKind kind, uint32_t map_id, uint32_t utc_start);
    void ResetVoteState();
    void CancelPendingVoteIfMatching(uint32_t utc_start);
    void FireVoteSubmit();

    bool show_vote_popup = false;
    PostRunVoteKind pending_vote_kind = PostRunVoteKind::None;
    uint32_t pending_vote_utc_start = 0; // 0 = no vote in flight; correlates ProcessSync's later
                                          // run_id lookup back to this vote (see OpenVote/ProcessSync)
    uint32_t pending_vote_map_id = 0;    // map the pending vote's run was on - decides which
                                          // kVoteRoles entries the popup offers (see DrawVotePopup)
    int64_t pending_vote_run_id = 0;
    bool vote_run_id_known = false; // explicit bool - 0 is a plausible db id, can't use as sentinel
    // True once "Submit Vote" is clicked, even before vote_run_id_known - FireVoteSubmit no-ops
    // (just sets this) until the run_id is known, then ProcessSync calls it again to actually send.
    // Locks the popup's controls (see DrawVotePopup) and, on a failed send, is reset back to false
    // (see ProcessVoteSubmit) so the existing retry-after-failure UX keeps working.
    bool vote_pending_submit = false;
    std::array<bool, 13> vote_role_checked{}; // parallel to kVoteRoles; shared by both vote kinds
    std::string vote_submit_error;            // non-empty renders as an inline error in the popup
    std::unique_ptr<AsyncRestClient> submit_request;
    uint64_t vote_popup_opened_tick = 0; // 0 = no vote window active (manually-opened popup)

    // --- Plugin version check ---
    // Two complementary mechanisms, both driven by the same plugin_outdated flag: proactively,
    // RequestLatestPluginVersion (fired once from LoadSettings, no machine key needed - public
    // endpoint) compares the server's declared latest version against this build's own kPluginVersion
    // constant before any sync/report attempt is even made. Reactively, every machine-key-
    // authenticated request (publish_request/submit_request/permission_request) now also sends its
    // own X-Plugin-Version header, and a 426 response from any of them (see their respective
    // completion handlers) sets plugin_outdated too - a backstop for the case where this build was
    // current when the proactive check ran but a newer one has shipped since. Once true,
    // plugin_outdated disables ProcessSync's publish attempt and the vote popup entirely
    // (not just a warning - see their respective gates) until the plugin is updated and restarted.
    void RequestLatestPluginVersion(); // fires version_check_request; called once from LoadSettings
    void ProcessVersionCheck();        // polls version_check_request completion; called from Update
    // Sets plugin_outdated (idempotent - a no-op if already set) and writes a one-time local chat
    // warning, since DrawSettings' warning text is easy to miss if the settings tab isn't open.
    void NotifyPluginOutdated();
    // Called from Update; writes the deferred chat message once GW::Map::GetIsMapLoaded() is true.
    // See pending_outdated_chat_notice's declaration for why this can't just happen inline.
    void ProcessPendingOutdatedNotice();

    bool plugin_outdated = false;
    // The very first version check can complete while still at the character-select/loading screen
    // (the HTTP request doesn't depend on game state), before the chat system exists to write into -
    // GW::Chat::WriteChat silently no-ops in that state. Set alongside plugin_outdated and cleared by
    // ProcessPendingOutdatedNotice once the map has actually loaded.
    bool pending_outdated_chat_notice = false;
    int latest_known_plugin_version = 0; // 0 until a successful check has actually reported one
    std::unique_ptr<AsyncRestClient> version_check_request;
};
