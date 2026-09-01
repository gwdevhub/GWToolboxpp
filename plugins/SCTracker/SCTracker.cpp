#include "SCTracker.h"
#include "PluginVersion.generated.h" // kPluginVersion - see cmake/gwtoolboxdll_plugins.cmake
// CI: rebuild to exercise the GCS plugin-publish path now that GCP_SA_KEY / GCP_PLUGIN_BUCKET are set.

#include <Path.h> // Core: PathGetDocumentsPath / PathGetComputerName

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/ItemIDs.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h> // full HeroInfo definition; PartyMgr.h only forward-declares it
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
// GWCA/Constants/UIMessages.h has no include guard - don't include it directly. UIMgr.h (which does
// have one) already pulls it in internally; including both causes its content to be pasted twice in
// this TU, which corrupts parsing for the rest of that file and shows up as bogus "undeclared
// identifier"/"undefined type" errors for symbols defined later in it.
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <glaze/glaze.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

// Mirrors the shape written to disk; kept separate from the live SCTracker::PartyMember only in
// name, not in fields. Needs external linkage (i.e. can't live in an anonymous namespace) — glaze's
// reflection generates a stable name per type and errors (C7631) on internal-linkage types.
struct LogEntry {
    uint32_t utc_start = 0;
    uint32_t map_id = 0;
    std::string character_name;
    // "wipe", "resign", "completed", or "unknown". Set at run end (OnGameSrvTransfer) from local
    // party/wipe/dhuum_completed signals; ProcessSync later upgrades "resign"/"unknown" to
    // "completed" as a fallback, for the rare case dhuum_completed itself missed it (e.g. a player
    // who joined after Dhuum was already dead), once the matched GWToolboxdll objective data
    // confirms the run actually finished.
    std::string end_reason;
    std::vector<SCTracker::PartyMember> party_members;
};

// Mirrors GWToolboxdll's ObjectiveTimerWindow::Objective::Serialized / ObjectiveSet::Serialized shape
// (Windows/ObjectiveTimerWindow.h) closely enough to read its runs/ObjectiveTimerRuns_*.json - can't
// include that header directly (internal to GWToolboxdll, not part of the exported plugin surface, and
// pulls in unrelated heavy deps like uWebSockets). Also needs external linkage, same reason as LogEntry.
struct RemoteObjective {
    std::string name;
    uint32_t status = 0; // 0=NotStarted, 1=Started, 2=Completed, 3=Failed
    uint32_t start = 0;
    uint32_t done = 0;
    std::optional<uint32_t> indent;
    std::optional<uint32_t> duration;
};
struct RemoteObjectiveSet {
    std::string name;
    uint32_t instance_start = 0;
    uint32_t utc_start = 0;
    std::vector<RemoteObjective> objectives;
    std::optional<uint32_t> duration;
};

// Body for the backend publish request. objective is always populated by the time this is
// constructed - ProcessSync drops (rather than publishes) any run that never gets a matching
// GWToolboxdll objective entry, since party-only data can never be leaderboard-eligible anyway.
struct PublishPayload {
    LogEntry party;
    RemoteObjectiveSet objective;
};

// Only the fields ProcessSync needs from a successful /upload-run response body - run_id (absent
// when the upload was silently dropped, e.g. an outdated plugin build) is what a pending vote
// (failure or MVP) is waiting to correlate against before it can actually submit - see
// SCTracker::pending_vote_run_id. Needs external linkage, same reason as LogEntry.
struct UploadRunResponseDto {
    std::optional<int64_t> run_id;
    bool created = false;
};

// Body for both POST /report-run-failure and POST /report-run-mvp - identical shape; the endpoint
// path (see SCTracker::FireVoteSubmit) is what tells the backend which kind this is. Needs
// external linkage, same reason as LogEntry.
struct ReportVotePayload {
    int64_t run_id = 0;
    std::vector<std::string> roles;
};

// Response body for GET /can-report-run-failure. Needs external linkage, same reason as LogEntry.
struct CanReportFailureResponseDto {
    bool can_report_failures = false;
};

// Response body for GET /plugin-version. Needs external linkage, same reason as LogEntry.
struct PluginVersionResponseDto {
    int version = 0;
    std::string compiled_at;
};

namespace {
    constexpr const char* kBaseUrl = "https://gwsctracker.com";
    constexpr const char* kUploadRunsPath = "upload-run";
    constexpr const char* kReportFailurePath = "report-run-failure";
    constexpr const char* kReportMvpPath = "report-run-mvp";
    constexpr const char* kCanReportFailurePath = "can-report-run-failure";
    constexpr const char* kPluginVersionPath = "plugin-version";
    constexpr int kHttpStatusUpgradeRequired = 426;

    // Static role vocabulary shared by both vote kinds (see SCTracker::PostRunVoteKind) - blame a
    // role for a failed run (Failure, multi-select checkboxes - several roles can share blame for
    // the same wipe), or credit exactly one role for a successful run (Mvp, single-select radio
    // buttons - see DrawVotePopup). Mirrors the backend's RoleDerivation output exactly: T1-T3 from
    // the plugin's own role_hint and the rest from server-side profession-combo derivation for the
    // Underworld trapper model; "Ranger"/"Derv" for the Fissure of Woe duo (role = primary
    // profession, RoleModel.PRIMARY_PROFESSION). DrawVotePopup only offers the entries that can
    // actually validate for the run's map (see VoteRoleVisibleForMap). For a failure vote, "Nobody"
    // (no player at fault - e.g. a disconnect, lag spike, or bad luck) records a run_failure_reasons
    // row with no run_participant attached - see FailureReportService.submit on the backend.
    constexpr std::array<const char*, 13> kVoteRoles = {
        "T1", "T2", "T3", "T4", "LT", "Spiker", "Derv", "SoS", "Necro", "RangerNecro", "Emo", "Ranger", "Nobody",
    };
    // "Nobody" must stay last in kVoteRoles - DrawVotePopup uses this index to enforce mutual
    // exclusivity between it and every other reason for a Failure vote (checking one clears the
    // other(s)); an Mvp vote is single-select across all roles already, so exclusivity there is
    // automatic and doesn't need this index at all.
    constexpr size_t kNobodyVoteRoleIndex = kVoteRoles.size() - 1;
    constexpr uint64_t kSyncScanIntervalMs = 5 * 60 * 1000;      // rescan local files for new entries
    constexpr uint64_t kObjectiveGiveUpTimeoutMs = 10 * 60 * 1000; // publish without a matched objective past this
    constexpr uint64_t kRetryBackoffMs = 60 * 1000;               // wait this long before retrying a failed publish
    constexpr uint64_t kVoteWindowMs = 60 * 1000; // how long the post-run vote popup stays open
    constexpr uint32_t kDeathTrackingGraceSec = 60; // ignore deaths in the first minute of the instance

    // Marks Dhuum's agent turning hostile (GAME_SMSG_AGENT_UPDATE_ALLEGIANCE). Same signal
    // ObjectiveTimerWindow::AddUWObjectiveSet() uses to start its "Dhuum" objective
    // (GWToolboxdll/Windows/ObjectiveTimerWindow.cpp) - mirrored here rather than read from that
    // module, since plugins can't include GWToolboxdll's internal headers.
    constexpr uint32_t kDhuumHostileAllegianceBits = 0x6D6F6E31;

    // Native mission-objective id for UW's "Dhuum" objective (GAME_SMSG_MISSION_OBJECTIVE_COMPLETE).
    // Same id ObjectiveTimerWindow::AddUWObjectiveSet() uses to end its own "Dhuum" objective
    // (GWToolboxdll/Windows/ObjectiveTimerWindow.cpp) - mirrored here for the same reason as
    // kDhuumHostileAllegianceBits above.
    constexpr uint32_t kDhuumObjectiveId = 157;

    // Instances GWToolboxdll's ObjectiveTimerWindow tracks AND the backend accepts (map_configs).
    // Re-expand from ObjectiveTimerWindow::AddObjectiveSet()'s switch (more elite areas, dungeons,
    // ToPK) as the backend gains map_configs rows for them.
    const std::unordered_set<uint32_t> kTrackedMapIds = {
        static_cast<uint32_t>(GW::Constants::MapID::The_Underworld),
        static_cast<uint32_t>(GW::Constants::MapID::The_Fissure_of_Woe),
    };

    // Whether the backend has a map_configs row for this (tracked map, real-player count): the
    // Underworld is 8-man; the Fissure of Woe has a config for every party size 1-8. A run whose
    // CountRealPlayers matches no config is never published and never opens a vote (ProcessSync).
    bool IsAcceptablePartySize(const uint32_t map_id, const uint32_t real_player_count)
    {
        switch (static_cast<GW::Constants::MapID>(map_id)) {
            case GW::Constants::MapID::The_Fissure_of_Woe:
                return real_player_count >= 1 && real_player_count <= 8;
            default: // The_Underworld and any future 8-man-only tracked area
                return real_player_count == 8;
        }
    }

    // Whether a (map, real-player count) run has a role model at all. The Underworld trapper team
    // and the Fissure of Woe *duo* do (T1-T3 / Ranger-Derv); every other FoW size has no fixed
    // composition (map_configs.role_model = NULL), so those runs get no post-run failure/MVP vote
    // - there are no roles to blame or credit. Keep in sync with the backend's map_configs.
    bool MapSizeHasRoles(const uint32_t map_id, const uint32_t real_player_count)
    {
        if (static_cast<GW::Constants::MapID>(map_id) == GW::Constants::MapID::The_Fissure_of_Woe) {
            return real_player_count == 2;
        }
        return true; // The_Underworld
    }

    // Whether this map's run mechanics revolve around the UW Dhuum fight. Gates the death-cutoff
    // latch (dhuum_started), the post-Dhuum gambling ritual, and the dhuum_completed end-reason
    // shortcut - none of which have a Fissure of Woe analogue. FoW run completion falls back to
    // ProcessSync's map-agnostic IsRunCompleted (objectives.back().status == Completed).
    bool MapHasDhuumMechanics(const uint32_t map_id)
    {
        return static_cast<GW::Constants::MapID>(map_id) == GW::Constants::MapID::The_Underworld;
    }

    // Which kVoteRoles entries the post-run popup offers for a run on map_id. The Underworld shows
    // all of them (the plugin can't see the server's profession-combo derivation, so it can't
    // pre-filter). The only FoW run that opens a vote is the duo (MapSizeHasRoles), whose role
    // model only ever yields Ranger/Derv, so only those (plus "Nobody") can validate server-side -
    // showing the rest would just be dead buttons.
    bool VoteRoleVisibleForMap(const uint32_t map_id, const size_t role_index)
    {
        if (static_cast<GW::Constants::MapID>(map_id) != GW::Constants::MapID::The_Fissure_of_Woe) {
            return true; // Underworld: can't pre-filter the server's profession-combo derivation
        }
        const std::string_view role = kVoteRoles[role_index];
        return role == "Ranger" || role == "Derv" || role == "Nobody";
    }

    // Encoded item-identity token for "Ghastly Summoning Stone" (GW::Constants::ItemID::GhastlyStone,
    // model_id 32557) - GW's fixed per-template control-code sequence for this item, matching
    // regardless of client display language, same technique as kResignedPrefix (below) and
    // GWToolboxdll/Modules/ChatFilter.cpp's rare_item_names/encoded_ashes_names. Confirmed via live
    // capture (2026-08-26) to be invariant across every message template this feature cares about -
    // drop and pickup, singular and plural alike (0x7F0/0x7F2/0x7F6/0x7FC) - so it's matched as a raw
    // substring anywhere in the message (see MessageContainsGhastlySummoningStone) rather than at a
    // fixed segment position: that position itself shifts when the quantity parameter below is
    // present, which is what a first attempt at fixed-position matching got wrong.
    constexpr wchar_t kGhastlySummoningStoneItemToken[] = L"\x8102\x5cc3";

    // Mirrors GWToolboxdll/Modules/ChatFilter.cpp's identically-named helpers (verbatim logic) -
    // reimplemented here since plugins can't include GWToolboxdll-internal headers. Used to parse the
    // "<player> drops/picks up <item>"-family chat messages for gambling-stone attribution (both drop
    // and pickup - see OnWriteToChatLog).
    size_t GetSegmentLength(const wchar_t* encoded_segment)
    {
        if (!(encoded_segment && *encoded_segment > 0x100)) {
            return 0;
        }
        size_t length = 0;
        do {
            length++;
        } while (*encoded_segment++ & 0x8000);
        return length;
    }

    const wchar_t* GetSegment(const wchar_t* encoded_string, const wchar_t identifier, size_t* segment_length = nullptr)
    {
        if (!encoded_string) {
            return nullptr;
        }
        const auto found = wcschr(encoded_string, identifier);
        if (!found) {
            return nullptr;
        }
        const wchar_t* segment = found + 1;
        if (segment_length) {
            *segment_length = GetSegmentLength(segment);
        }
        return segment;
    }

    const wchar_t* GetFirstSegment(const wchar_t* encoded_string, size_t* segment_length = nullptr)
    {
        return GetSegment(encoded_string, 0x10a, segment_length);
    }

    bool IsPlayerNameToken(const wchar_t* encoded_string)
    {
        return encoded_string && wcsncmp(encoded_string, L"\xba9\x107", 2) == 0;
    }

    // True if kGhastlySummoningStoneItemToken appears anywhere in the message - see that constant's
    // own comment for why this is a raw substring scan rather than a fixed-position segment match.
    bool MessageContainsGhastlySummoningStone(const wchar_t* message)
    {
        if (!message) {
            return false;
        }
        constexpr size_t token_len = std::size(kGhastlySummoningStoneItemToken) - 1;
        for (const wchar_t* p = message; *p != 0; ++p) {
            if (wcsncmp(p, kGhastlySummoningStoneItemToken, token_len) == 0) {
                return true;
            }
        }
        return false;
    }

    // Reads a drop/pickup message's own quantity directly out of its embedded 0x101-tagged numeric
    // parameter - the same small-integer encoding GWToolboxdll/Modules/ChatFilter.cpp's
    // GetNumericSegment uses elsewhere (value = codepoint - 0x100). Confirmed via live capture
    // (2026-08-26): this parameter is entirely absent for a quantity-1 event and present as 0x100+N
    // for N>1 (e.g. 0x103 decodes to 3 for a 3-stack drop) - defaults to 1 to match that observed
    // singular-omission behavior. Reading this directly from each event's own message is what lets
    // drop and pickup both get an exact quantity without needing to correlate against a separate
    // ItemGeneral sighting (a stone dropped and later picked up minutes apart, possibly by a different
    // party member, has no other shared key to correlate the two events by anyway).
    uint32_t GetGamblingStoneQuantity(const wchar_t* message)
    {
        const wchar_t* found = GetSegment(message, 0x101);
        if (!found || *found < 0x100) {
            return 1;
        }
        return static_cast<uint32_t>(*found) - 0x100;
    }

    std::filesystem::path GetRunsFolder()
    {
        std::filesystem::path computer_name;
        std::filesystem::path docs;
        if (!PathGetComputerName(computer_name) || !PathGetDocumentsPath(docs, L"GWToolboxpp")) {
            return {};
        }
        return docs / computer_name / L"runs";
    }

    // Own log file, separate from GWToolboxdll's log.txt (which plugins can't write to - it's not part
    // of the exported surface, and Debug builds hold it open without shared-write access). Lives next to
    // the runs/ folder rather than in it, since it isn't run data.
    void AppendLog(const std::string& line)
    {
        const auto runs_folder = GetRunsFolder();
        if (runs_folder.empty()) {
            return;
        }
        std::ofstream out{runs_folder.parent_path() / L"SCTracker.log", std::ios::app};
        if (!out.is_open()) {
            return;
        }
        const time_t now = time(nullptr);
        char ts[32] = "";
        if (const tm* timeinfo = gmtime(&now)) {
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", timeinfo);
        }
        out << '[' << ts << "] " << line << '\n';
    }

    // Same folder GWToolboxdll's Objective Timer writes ObjectiveTimerRuns_*.json into, so a single
    // cron job can watch one directory. Day bucket uses UTC, matching how GWToolboxdll buckets its own files.
    std::filesystem::path GetLogFilePath(const uint32_t utc_start)
    {
        const auto folder = GetRunsFolder();
        if (folder.empty()) {
            return {};
        }
        const time_t tt = utc_start;
        const tm* timeinfo = gmtime(&tt);
        if (!timeinfo) {
            return {};
        }
        wchar_t filename[40];
        swprintf(filename, _countof(filename), L"PartyLog_%04d-%02d-%02d.json",
                 timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
        return folder / filename;
    }

    // GWToolboxdll's own file for the same UTC day (ObjectiveTimerWindow::SaveRuns, same gmtime bucketing).
    std::filesystem::path GetObjectiveLogFilePath(const uint32_t utc_start)
    {
        const auto folder = GetRunsFolder();
        if (folder.empty()) {
            return {};
        }
        const time_t tt = utc_start;
        const tm* timeinfo = gmtime(&tt);
        if (!timeinfo) {
            return {};
        }
        wchar_t filename[48];
        swprintf(filename, _countof(filename), L"ObjectiveTimerRuns_%04d-%02d-%02d.json",
                 timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
        return folder / filename;
    }

    template <typename T>
    bool ReadJsonArray(const std::filesystem::path& path, std::vector<T>& out)
    {
        std::ifstream in{path};
        if (!in.is_open()) {
            return false;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        return !glz::read<opts>(out, ss.str());
    }

    // Looks for a GWToolboxdll objective run matching utc_start, in today's and yesterday's files
    // (a run can start just before UTC midnight and be written just after).
    //
    // Not an exact match: GWToolboxdll stamps its utc_start once InstanceLoadInfo, InstanceLoadFile,
    // AND InstanceTimer have all arrived, while we stamp ours as soon as InstanceLoadInfo alone
    // arrives - if those don't land in the same frame, time()'s 1-second resolution can put the two
    // timestamps on opposite sides of a second boundary. Match the closest candidate within a small
    // tolerance instead of requiring equality.
    bool TryReadMatchingObjectiveEntry(const uint32_t utc_start, RemoteObjectiveSet& out)
    {
        constexpr uint32_t kMatchToleranceSec = 2;

        std::vector<RemoteObjectiveSet> sets;
        for (const uint32_t candidate_ts : {utc_start, utc_start - 86400u}) {
            std::vector<RemoteObjectiveSet> day_sets;
            if (ReadJsonArray(GetObjectiveLogFilePath(candidate_ts), day_sets)) {
                for (auto& s : day_sets) {
                    sets.push_back(std::move(s));
                }
            }
        }

        int best_index = -1;
        uint32_t best_diff = kMatchToleranceSec + 1;
        for (size_t i = 0; i < sets.size(); i++) {
            const uint32_t diff = sets[i].utc_start > utc_start
                ? sets[i].utc_start - utc_start
                : utc_start - sets[i].utc_start;
            if (diff <= kMatchToleranceSec && diff < best_diff) {
                best_diff = diff;
                best_index = static_cast<int>(i);
            }
        }
        if (best_index < 0) {
            return false;
        }
        out = std::move(sets[best_index]);
        return true;
    }

    // Resigning to leave after finishing a run is the normal exit mechanism, not a failure - it's
    // indistinguishable from an actual give-up resign at classification time (OnGameSrvTransfer, which
    // fires before GWToolboxdll has even written the objective data that would tell us which one it
    // was). GWToolboxdll only marks an objective Failed from StopObjectives() (a wipe); under normal
    // play every objective ends up Completed, and each area's Add*ObjectiveSet() appends its final
    // objective (e.g. Dhuum for UW) last - so objectives.back().status == Completed is a reliable
    // "this run actually finished" signal once we have it, checked here (ProcessSync, once the
    // objective entry is matched) rather than at classification time.
    constexpr uint32_t kObjectiveStatusCompleted = 2;
    bool IsRunCompleted(const RemoteObjectiveSet& objective_set)
    {
        return !objective_set.objectives.empty() && objective_set.objectives.back().status == kObjectiveStatusCompleted;
    }

    // party_members.size() alone isn't enough to identify a real guild run - a solo player filling
    // the rest of an 8-slot party with heroes/henchmen still occupies every slot. Count only real
    // players (is_player == true), then check it against IsAcceptablePartySize for the map.
    uint32_t CountRealPlayers(const std::vector<SCTracker::PartyMember>& members)
    {
        return static_cast<uint32_t>(std::ranges::count_if(members, &SCTracker::PartyMember::is_player));
    }

    constexpr const char* kUnknownRole = "unknown";

    // The t1/t2/t3 role archetype is specifically Ranger-primary/Assassin-secondary (2/7) - a Ranger
    // primary with some other secondary (e.g. Ranger/Necromancer), or an Assassin primary, isn't part
    // of it, even if they incidentally use some of the same skills for unrelated reasons. Used to gate
    // role_skills/role_hint tracking in both OnSkillUsed and ProcessTrackedSkillUse - keep both in sync
    // with this.
    bool IsRoleEligible(const uint32_t primary, const uint32_t secondary)
    {
        return static_cast<GW::Constants::Profession>(primary) == GW::Constants::Profession::Ranger
            && static_cast<GW::Constants::Profession>(secondary) == GW::Constants::Profession::Assassin;
    }

    // Every skill relevant to t2/t3 (see kRoleCombos and OnSkillUsed), matched by decoded English name
    // rather than SkillID: the SkillID enum has no explicit per-entry numbering (Skills.h just lists
    // them in order), so its ordinals shift whenever GWCA's header gains or loses an entry anywhere
    // earlier in the list - a decoded name is stable regardless. Forced to English (see OnSkillUsed's
    // skill_name_cache population) rather than following the client's language setting. Populates
    // PartyMember::role_skills whenever the LOCAL PLAYER uses one of these, independent of whether a
    // role can actually be determined from it.
    const std::unordered_set<std::string> kTrackedSkillNameSet = {
        "Shadow of Haste",  "Shadow Walk",       "Winnowing",         "Finish Him!",
        "Recall",           "Radiation Field",   "Viper's Defense",   "Edge of Extinction",
        "Quickening Zephyr",
    };

    struct RoleCombo {
        const char* role;
        std::vector<std::string> required_skills; // ALL must appear in role_skills to satisfy this combo
    };

    // role_hint is set to the role of the first of these combos whose required_skills are all present
    // in the local player's role_skills (see OnSkillUsed) - i.e. this is always about the uploader's
    // own, reliably-observed skill usage, never a guess about someone else. t1 combos are listed first
    // so they're preferred if a member's skills happen to satisfy more than one role's combo at once.
    // Radiation Field alone is sufficient for t2; Viper's Defense is tracked (kTrackedSkillNameSet) but
    // doesn't itself factor into any combo below.
    const std::vector<RoleCombo> kRoleCombos = {
        {"t1", {"Shadow of Haste", "Shadow Walk"}},
        {"t1", {"Winnowing", "Finish Him!"}},
        {"t1", {"Shadow of Haste", "Recall"}},
        {"t2", {"Radiation Field"}},
        {"t3", {"Shadow of Haste", "Edge of Extinction"}},
        {"t3", {"Shadow of Haste", "Quickening Zephyr"}},
    };

    // model_ids counted into PartyMember::item_drops (see OnItemGeneral/OnItemUpdateOwner). model_id,
    // not item_id, identifies an item type: item_id is per-drop-instance and gets recycled
    // (GAME_SMSG_ITEM_REUSE_ID) - model_id is what GWCA's own item APIs (GetItemByModelId etc.,
    // ItemMgr.h) key on instead, and what's sent to the backend (PartyMember::ItemDropCount::id) - the
    // backend is expected to already have its own id -> display name mapping.
    const std::unordered_set<uint32_t> kTrackedItems = {
        GW::Constants::ItemID::GlobofEctoplasm,  // Glob of Ectoplasm
        GW::Constants::ItemID::VoltaicSpear,     // Voltaic Spear
        GW::Constants::ItemID::DSR,              // DSR
        GW::Constants::ItemID::EternalBlade,     // Eternal Blade
        GW::Constants::ItemID::MiniDhuum,        // Mini Dhuum
        GW::Constants::ItemID::MiniSmiteCrawler, // Miniature Smite Crawler
    };

    // Encoded prefix for the "<player> has resigned." system chat message. Not human-readable text -
    // it's GW's fixed per-template control-code sequence, so it matches regardless of the client's
    // display language (only the decoded text varies by language, not the encoded template id). Same
    // bytes GWToolboxdll's own ResignLogModule matches on.
    constexpr wchar_t kResignedPrefix[] = L"\x7BFF\xC9C4\xAEAA\x1B9B\x107";
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static SCTracker instance;
    return &instance;
}

void SCTracker::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    // Positive altitude: triggered after the packet has been processed by the game/GWCA.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_HookEntry,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* packet) {
            OnInstanceLoadInfo(packet->map_id, packet->is_explorable != 0);
        },
        1);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &GameSrvTransfer_HookEntry,
        [this](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) { OnGameSrvTransfer(); });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyDefeated>(
        &PartyDefeated_HookEntry,
        [this](GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) { OnPartyDefeated(); });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_HookEntry,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentState* packet) {
            OnUpdateAgentState(packet->agent_id, packet->state);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &AgentUpdateAllegiance_HookEntry,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* packet) {
            OnAgentUpdateAllegiance(packet->agent_id, packet->allegiance_bits);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &ObjectiveDone_HookEntry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
            OnObjectiveDone(packet->objective_id);
        });
    // Skill used on self / no target.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_HookEntry,
        [this](GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) {
            switch (packet->value_id) {
                case GW::Packet::StoC::GenericValueID::instant_skill_activated:
                case GW::Packet::StoC::GenericValueID::skill_activated:
                case GW::Packet::StoC::GenericValueID::skill_finished:
                case GW::Packet::StoC::GenericValueID::attack_skill_activated:
                case GW::Packet::StoC::GenericValueID::attack_skill_finished:
                    OnSkillUsed(packet->agent_id, static_cast<GW::Constants::SkillID>(packet->value));
                    break;
                default:
                    break;
            }
        });
    // Skill used on a target. Field names are misleading for these event ids: per GenericValueID's
    // own comments in StoC.h ("caster_id is victim and target_id is caster"), and confirmed by
    // PartyStatisticsWindow's SkillCallback (GWToolboxdll/Windows/PartyStatisticsWindow.cpp), the
    // actual caster is packet->target, not packet->caster.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(
        &GenericValueTarget_HookEntry,
        [this](GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* packet) {
            switch (packet->Value_id) {
                case GW::Packet::StoC::GenericValueID::instant_skill_activated:
                case GW::Packet::StoC::GenericValueID::skill_activated:
                case GW::Packet::StoC::GenericValueID::skill_finished:
                case GW::Packet::StoC::GenericValueID::attack_skill_activated:
                case GW::Packet::StoC::GenericValueID::attack_skill_finished:
                    OnSkillUsed(packet->target, static_cast<GW::Constants::SkillID>(packet->value));
                    break;
                default:
                    break;
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemGeneral>(
        &ItemGeneral_HookEntry, [this](GW::HookStatus*, const GW::Packet::StoC::ItemGeneral* packet) {
            OnItemGeneral(packet->item_id, packet->model_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemUpdateOwner>(
        &ItemUpdateOwner_HookEntry, [this](GW::HookStatus*, const GW::Packet::StoC::ItemUpdateOwner* packet) {
            OnItemUpdateOwner(packet->item_id, packet->owner_agent_id);
        });
    GW::UI::RegisterUIMessageCallback(
        &WriteToChatLog_HookEntry, GW::UI::UIMessage::kWriteToChatLog,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wParam, void*) {
            OnWriteToChatLog(static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wParam)->message);
        },
        0x8000);
}

void SCTracker::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&WriteToChatLog_HookEntry, GW::UI::UIMessage::kWriteToChatLog);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemUpdateOwner>(&ItemUpdateOwner_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemGeneral>(&ItemGeneral_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValue>(&GenericValueSelf_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&AgentUpdateAllegiance_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&AgentState_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyDefeated>(&PartyDefeated_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_HookEntry);
    ToolboxPlugin::Terminate();
}

void SCTracker::OnInstanceLoadInfo(const uint32_t map_id, const bool is_explorable)
{
    if (!is_explorable || !kTrackedMapIds.contains(map_id)) {
        return; // not an area ObjectiveTimerWindow tracks; skip capture entirely for this instance
    }
    next_utc_start = static_cast<uint32_t>(time(nullptr));
    next_map_id = map_id;
    next_character_name.clear();
    if (const GW::CharContext* cc = GW::GetCharContext()) {
        next_character_name = PluginUtils::WStringToString(cc->player_name);
    }
    restart_requested = true;

    run_active = true;
    wipe_detected = false;
    resigned_login_numbers.clear();
    dhuum_started = false;
    dhuum_completed = false;
    tracked_item_id_to_model_id.clear();
    // Not skill_name_cache itself (deliberately persists - see its declaration) - just events still
    // queued from the previous run. Decoding is near-instant in practice, but without this a very
    // late-finishing decode could otherwise attribute a previous run's skill use to this new one.
    pending_role_skill_events.clear();
}

void SCTracker::OnPartyDefeated()
{
    if (!run_active) {
        return;
    }
    wipe_detected = true;
}

void SCTracker::OnWriteToChatLog(const wchar_t* message)
{
    if (!run_active || !message) {
        return;
    }
    if (wmemcmp(message, kResignedPrefix, 5) == 0) {
        const std::wstring resigned_name = PluginUtils::GetPlayerNameFromEncodedString(message);
        if (resigned_name.empty()) {
            return;
        }
        const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
        if (!info) {
            return;
        }
        for (const auto& player : info->players) {
            const wchar_t* name_ptr = GW::PlayerMgr::GetPlayerName(player.login_number);
            if (!name_ptr) {
                continue;
            }
            if (PluginUtils::SanitizePlayerName(name_ptr) == resigned_name) {
                resigned_login_numbers.insert(player.login_number);
                return;
            }
        }
        return;
    }

    // Post-Dhuum gambling ritual (see PartyMember::gambling_stone_net) - Underworld-only, and only
    // after Dhuum is down. dhuum_completed can never latch off the Underworld (OnObjectiveDone is
    // map-gated), so the map check is redundant belt-and-suspenders, kept for clarity.
    if (!MapHasDhuumMechanics(pending_map_id) || !dhuum_completed) {
        return;
    }
    switch (message[0]) {
        case 0x7F0: { // monster/player X drops item Y - ChatFilter.cpp's same case
            if (!IsPlayerNameToken(GetFirstSegment(message))) {
                return; // monster drop, not a party member's manual drop
            }
            if (!MessageContainsGhastlySummoningStone(message)) {
                return;
            }
            const std::wstring dropper_name = PluginUtils::GetPlayerNameFromEncodedString(message);
            if (dropper_name.empty()) {
                return;
            }
            const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
            if (!info) {
                return;
            }
            const uint32_t quantity = GetGamblingStoneQuantity(message);
            for (const auto& player : info->players) {
                const wchar_t* name_ptr = GW::PlayerMgr::GetPlayerName(player.login_number);
                if (!name_ptr || PluginUtils::SanitizePlayerName(name_ptr) != dropper_name) {
                    continue;
                }
                const GW::Player* gwplayer = GW::PlayerMgr::GetPlayerByID(player.login_number);
                const auto member_it = gwplayer ? agent_id_to_party_index.find(gwplayer->agent_id)
                                                 : agent_id_to_party_index.end();
                if (member_it != agent_id_to_party_index.end()) {
                    AddGamblingStoneDelta(member_it->second, -static_cast<int32_t>(quantity));
                }
                return;
            }
            break;
        }
        case 0x7F2: { // you (local player) drop item Y
            if (!MessageContainsGhastlySummoningStone(message)) {
                return;
            }
            const auto member_it = agent_id_to_party_index.find(GW::Agents::GetControlledCharacterId());
            if (member_it != agent_id_to_party_index.end()) {
                AddGamblingStoneDelta(member_it->second, -static_cast<int32_t>(GetGamblingStoneQuantity(message)));
            }
            break;
        }
        case 0x7F6: { // player x picks up item y (note: item can be unassigned gold)
            if (!MessageContainsGhastlySummoningStone(message)) {
                return;
            }
            const std::wstring picker_name = PluginUtils::GetPlayerNameFromEncodedString(message);
            if (picker_name.empty()) {
                return;
            }
            const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
            if (!info) {
                return;
            }
            const uint32_t quantity = GetGamblingStoneQuantity(message);
            for (const auto& player : info->players) {
                const wchar_t* name_ptr = GW::PlayerMgr::GetPlayerName(player.login_number);
                if (!name_ptr || PluginUtils::SanitizePlayerName(name_ptr) != picker_name) {
                    continue;
                }
                const GW::Player* gwplayer = GW::PlayerMgr::GetPlayerByID(player.login_number);
                const auto member_it = gwplayer ? agent_id_to_party_index.find(gwplayer->agent_id)
                                                 : agent_id_to_party_index.end();
                if (member_it != agent_id_to_party_index.end()) {
                    AddGamblingStoneDelta(member_it->second, static_cast<int32_t>(quantity));
                }
                return;
            }
            break;
        }
        case 0x7FC: { // you pick up item y (note: item can be unassigned gold)
            if (!MessageContainsGhastlySummoningStone(message)) {
                return;
            }
            const auto member_it = agent_id_to_party_index.find(GW::Agents::GetControlledCharacterId());
            if (member_it != agent_id_to_party_index.end()) {
                AddGamblingStoneDelta(member_it->second, static_cast<int32_t>(GetGamblingStoneQuantity(message)));
            }
            break;
        }
        default:
            break;
    }
}

// GAME_SMSG_AGENT_UPDATE_EFFECTS; state bit 0x0010 is the agent's dead flag. Only counts the
// alive->dead edge (not every packet while already dead) and re-arms on the dead->alive edge (i.e. a
// resurrection), so a member who dies twice in the same run is counted twice. Deaths in the first
// minute of the instance (loading in, initial positioning, an early accidental pull) aren't counted -
// party_member_currently_dead is deliberately left unsynced during that window, same as dhuum_started:
// the first real edge evaluated after the grace period compares against whatever it defaulted to,
// which self-corrects rather than needing to be back-filled.
void SCTracker::OnUpdateAgentState(const uint32_t agent_id, const uint32_t state)
{
    if (!run_active || dhuum_started) {
        return;
    }
    if (static_cast<uint32_t>(time(nullptr)) - pending_utc_start < kDeathTrackingGraceSec) {
        return;
    }
    const auto it = agent_id_to_party_index.find(agent_id);
    if (it == agent_id_to_party_index.end()) {
        return;
    }
    const size_t idx = it->second;
    const bool now_dead = (state & 0x0010) != 0;
    if (now_dead == party_member_currently_dead[idx]) {
        return;
    }
    party_member_currently_dead[idx] = now_dead;
    if (now_dead) {
        party_members[idx].deaths++;
    }
}

// Fires on Dhuum's agent turning hostile (living->player_number doubles as the model id for NPC
// agents - see AgentIDs.h's ModelID namespace comment). Latches dhuum_started for the rest of the
// run; OnUpdateAgentState stops counting deaths once it's set, since deaths during/after the Dhuum
// fight (e.g. the tank) are expected, not run-ending mistakes.
void SCTracker::OnAgentUpdateAllegiance(const uint32_t agent_id, const uint32_t allegiance_bits)
{
    if (!run_active || dhuum_started || !MapHasDhuumMechanics(pending_map_id)
        || allegiance_bits != kDhuumHostileAllegianceBits) {
        return;
    }
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;
    if (living && living->player_number == static_cast<uint32_t>(GW::Constants::ModelID::UW::Dhuum)) {
        dhuum_started = true;
    }
}

// GAME_SMSG_MISSION_OBJECTIVE_COMPLETE - fires for any completed native mission objective, not just
// Dhuum's; only kDhuumObjectiveId is relevant here. Latches dhuum_completed for the rest of the run;
// OnItemGeneral stops counting Glob of Ectoplasm drops once it's set, and OnGameSrvTransfer uses it
// as the "run completed" shortcut. Underworld-only: elsewhere dhuum_completed stays false and run
// completion is decided by ProcessSync's map-agnostic IsRunCompleted fallback.
void SCTracker::OnObjectiveDone(const uint32_t objective_id)
{
    if (!run_active || !MapHasDhuumMechanics(pending_map_id) || objective_id != kDhuumObjectiveId) {
        return;
    }
    dhuum_completed = true;
}

// Role tracking is local-player-only now (see PartyMember::role_skills' comment) - bails immediately
// for any other agent, before touching skill_name_cache at all. For the local player, ensures a decode
// is in flight for skill_id (starting one via skill_name_cache if this is the first time it's been
// seen at all, this run or any previous one), then either processes immediately (already decoded, e.g.
// a repeat cast) or queues the event for FlushPendingRoleSkills to pick up once decoding finishes.
void SCTracker::OnSkillUsed(const uint32_t agent_id, const GW::Constants::SkillID skill_id)
{
    if (!run_active || skill_id == GW::Constants::SkillID::No_Skill) {
        return;
    }
    if (agent_id != GW::Agents::GetControlledCharacterId()) {
        return;
    }
    const auto member_it = agent_id_to_party_index.find(agent_id);
    if (member_it == agent_id_to_party_index.end()) {
        return;
    }
    const PartyMember& candidate = party_members[member_it->second];
    if (!IsRoleEligible(candidate.primary, candidate.secondary)) {
        return;
    }

    const auto id = static_cast<uint32_t>(skill_id);
    auto cache_it = skill_name_cache.find(id);
    if (cache_it == skill_name_cache.end()) {
        const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        auto enc = std::make_unique<PluginUtils::EncString>(skill_data ? skill_data->name : 0u);
        enc->language(GW::Constants::Language::English);
        enc->wstring(); // trigger decode
        cache_it = skill_name_cache.emplace(id, std::move(enc)).first;
    }
    if (cache_it->second->IsDecoding()) {
        pending_role_skill_events.push_back({.skill_id = id});
        return;
    }
    ProcessTrackedSkillUse(cache_it->second->string());
}

// Drains pending_role_skill_events, calling ProcessTrackedSkillUse for any whose skill_name_cache
// entry has finished decoding. Called from Update.
void SCTracker::FlushPendingRoleSkills()
{
    std::erase_if(pending_role_skill_events, [this](const PendingRoleSkillEvent& event) {
        const auto it = skill_name_cache.find(event.skill_id);
        if (it == skill_name_cache.end() || it->second->IsDecoding()) {
            return false; // shouldn't happen (the cache entry always exists by the time it's queued),
                           // but leave it queued rather than drop it if it somehow does
        }
        ProcessTrackedSkillUse(it->second->string());
        return true;
    });
}

// Re-resolves the local player's PartyMember entry (rather than being passed one, since OnSkillUsed
// already guaranteed agent_id == the controlled character before ever queuing this) and re-checks
// eligibility, since party composition/profession, in principle, could change between OnSkillUsed
// queuing this and it actually running. Records a kTrackedSkillNameSet hit into role_skills (deduped),
// then - only while role_hint is still "unknown" - checks kRoleCombos in order and locks in the role of
// the first fully-satisfied combo. Once role_hint is set it's never changed again for the rest of the
// run.
void SCTracker::ProcessTrackedSkillUse(const std::string& skill_name)
{
    if (!run_active || !kTrackedSkillNameSet.contains(skill_name)) {
        return;
    }
    const auto member_it = agent_id_to_party_index.find(GW::Agents::GetControlledCharacterId());
    if (member_it == agent_id_to_party_index.end()) {
        return;
    }
    PartyMember& member = party_members[member_it->second];
    if (!IsRoleEligible(member.primary, member.secondary)) {
        return;
    }
    if (std::ranges::contains(member.role_skills, skill_name)) {
        return; // already recorded - nothing new to (re-)evaluate
    }
    member.role_skills.push_back(skill_name);

    if (member.role_hint != kUnknownRole) {
        return; // already locked in
    }
    for (const auto& combo : kRoleCombos) {
        const bool satisfied = std::ranges::all_of(combo.required_skills, [&](const std::string& s) {
            return std::ranges::contains(member.role_skills, s);
        });
        if (satisfied) {
            member.role_hint = combo.role;
            break;
        }
    }
}

// GAME_SMSG_ITEM_GENERAL_INFO - fires for items as they're identified client-side (e.g. a drop
// landing). Caches item_id -> model_id for kTrackedItems hits (so OnItemUpdateOwner has something to
// resolve the item_id it gets to) - untracked items are never cached, keeping this bounded to however
// many tracked-item drops are in flight at once. Glob of Ectoplasm specifically stops being cached
// (and therefore counted) once dhuum_completed is set - other tracked items are unaffected. The
// gambling stone (PartyMember::gambling_stone_net) is deliberately NOT handled here or via
// OnItemUpdateOwner below - both drop and pickup are attributed entirely from their own chat message
// in OnWriteToChatLog instead (item identity and exact quantity are both readable directly from that
// message - see MessageContainsGhastlySummoningStone/GetGamblingStoneQuantity), since ItemUpdateOwner
// was confirmed via live testing (2026-08-26) to not fire at all for a self-pickup of a self-dropped
// item, and using both paths together would risk double-counting for any case where it does fire.
void SCTracker::OnItemGeneral(const uint32_t item_id, const uint32_t model_id)
{
    if (!run_active || !kTrackedItems.contains(model_id)) {
        return;
    }
    if (dhuum_completed && model_id == static_cast<uint32_t>(GW::Constants::ItemID::GlobofEctoplasm)) {
        return;
    }
    tracked_item_id_to_model_id[item_id] = model_id;
}

// GAME_SMSG_ITEM_UPDATE_OWNER - loot reservation, broadcast to the whole party (not just the
// recipient); also fires for a manually-dropped item's pickup, not just kill loot. Can re-fire for
// the same item_id if the reservation is reassigned (GWToolboxdll's ItemDrops module tracks this by
// updating an owner map in place, not counting) - only the first firing for a given tracked item_id
// is counted here, then the cache entry is erased so a later reassignment isn't double-counted.
// Reflects who it was reserved for, not confirmed pickup - another player's inventory contents beyond
// a reservation broadcast aren't visible to this client at all.
void SCTracker::OnItemUpdateOwner(const uint32_t item_id, const uint32_t owner_agent_id)
{
    if (!run_active) {
        return;
    }
    const auto model_it = tracked_item_id_to_model_id.find(item_id);
    if (model_it == tracked_item_id_to_model_id.end()) {
        return;
    }
    const uint32_t model_id = model_it->second;
    tracked_item_id_to_model_id.erase(model_it);

    const auto member_it = agent_id_to_party_index.find(owner_agent_id);
    if (member_it == agent_id_to_party_index.end()) {
        return;
    }

    auto& drops = party_members[member_it->second].item_drops;
    const auto drop_it = std::ranges::find(drops, model_id, &PartyMember::ItemDropCount::id);
    if (drop_it != drops.end()) {
        drop_it->count++;
    }
    else {
        drops.push_back({.id = model_id, .count = 1});
    }
}

// Single choke point for both the drop (OnWriteToChatLog, negative delta) and pickup
// (OnItemUpdateOwner, positive delta) paths - initializes gambling_stone_net from null to 0 on a
// member's first gambling event of the run (see the field's own comment for the null/0 distinction),
// then applies delta (already sign-adjusted and quantity-weighted by the caller - see
// OnWriteToChatLog, the sole caller for both drop and pickup). Guards on is_player here rather than at
// each call site - heroes/henchmen are reachable via agent_id_to_party_index (it indexes every party
// member) but can never actually perform the ritual.
void SCTracker::AddGamblingStoneDelta(const size_t party_index, const int32_t delta)
{
    PartyMember& member = party_members[party_index];
    if (!member.is_player) {
        return;
    }
    member.gambling_stone_net = member.gambling_stone_net.value_or(0) + delta;
}

void SCTracker::OnGameSrvTransfer()
{
    if (!run_active) {
        return;
    }
    run_active = false;

    if (restart_requested || active_capture || party_members.empty()) {
        return; // party capture never completed for this run; nothing worth logging
    }

    // Checked before wipe_detected: resign is the more specific signal (every connected player
    // individually confirmed via their own "has resigned" chat message), whereas PartyDefeated
    // appears to also fire when the whole party resigns, not just on an actual death-wipe.
    std::string end_reason = "unknown";
    if (const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo()) {
        bool any_connected = false;
        bool all_resigned = true;
        for (const auto& player : info->players) {
            if (!player.connected()) {
                continue;
            }
            any_connected = true;
            if (!resigned_login_numbers.contains(player.login_number)) {
                all_resigned = false;
                break;
            }
        }
        if (any_connected && all_resigned) {
            end_reason = "resign";
        }
    }
    if (end_reason == "unknown" && wipe_detected) {
        end_reason = "wipe";
    }
    // dhuum_completed is latched in real time off the native GAME_SMSG_MISSION_OBJECTIVE_COMPLETE
    // packet (see OnObjectiveDone) - the same signal GWToolboxdll uses to mark its own Dhuum
    // objective Completed, just observed here locally and immediately instead of from its
    // ObjectiveTimerRuns_*.json file, which isn't flushed to disk until the next map load. This
    // means "completed" is usually already known right now rather than only after ProcessSync's
    // later IsRunCompleted fallback (still needed for the rare case a player joined after Dhuum was
    // already dead and so never saw the packet themselves). Never overrides "wipe" - a genuine
    // death event stays notable even in the rare case it's right after a kill.
    if (end_reason != "wipe" && dhuum_completed) {
        end_reason = "completed";
    }
    WriteLogEntry(pending_utc_start, pending_map_id, pending_character_name, end_reason, party_members);
    last_queue_scan_tick = 0; // force ProcessSync to pick this up on the next tick, not the 5-minute cadence

    // Open the post-run vote immediately using this locally-known outcome, rather than waiting for
    // ProcessSync to publish (which needs GWToolboxdll's own delayed objective file) - see OpenVote.
    // Skipped for a role-less (map, size) like any non-duo FoW run - there are no roles to blame or credit.
    if (can_report_failures && !plugin_outdated
        && MapSizeHasRoles(pending_map_id, CountRealPlayers(party_members))) {
        if (end_reason == "wipe" || end_reason == "resign") {
            OpenVote(PostRunVoteKind::Failure, pending_map_id, pending_utc_start);
        }
        else if (end_reason == "completed") {
            OpenVote(PostRunVoteKind::Mvp, pending_map_id, pending_utc_start);
        }
        // "unknown" opens nothing.
    }
}

void SCTracker::Update(float)
{
    CaptureParty();
    FlushPendingRoleSkills();
    ProcessSync();
    ProcessPermissionCheck();
    ProcessVoteSubmit();
    ProcessVersionCheck();
    ProcessPendingOutdatedNotice();
}

void SCTracker::CaptureParty()
{
    if (restart_requested) {
        // Only safe to tear down the previous run's EncStrings once none are still mid-decode -
        // destroying one while GW::UI::AsyncDecodeStr's callback is still pending is a use-after-free.
        for (const auto& enc : party_member_enc_names) {
            if (enc->IsDecoding()) {
                return; // let the old capture's decodes finish before starting the new one
            }
        }
        party_members.clear();
        party_member_enc_names.clear();
        agent_id_to_party_index.clear();
        party_member_currently_dead.clear();
        pending_utc_start = next_utc_start;
        pending_map_id = next_map_id;
        pending_character_name = next_character_name;
        restart_requested = false;
        active_capture = true;
    }

    if (!active_capture) {
        return;
    }

    if (!party_member_enc_names.empty()) {
        // Waiting on names queued last pass to finish decoding.
        for (const auto& enc : party_member_enc_names) {
            if (enc->IsDecoding()) {
                return;
            }
        }
        for (size_t i = 0; i < party_members.size(); i++) {
            party_members[i].name = party_member_enc_names[i]->string();
        }
        party_member_enc_names.clear();
        active_capture = false;
        return; // logged at run end (OnGameSrvTransfer), once the outcome is known
    }

    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (!info) {
        return; // not yet available; retry next tick
    }

    // primary/secondary come from the party/player/hero roster data, not the agent - the roster is
    // populated as soon as party membership syncs, whereas another real player's in-world agent can
    // take a moment longer to spawn/load. Reading it off the agent here raced that load and silently
    // left late-loading members at primary=secondary=0 (i.e. indistinguishable from Profession::None).
    const auto add_member = [&](const uint32_t agent_id, const wchar_t* enc_name, const uint32_t primary,
                                 const uint32_t secondary, const bool is_player, const bool is_hero,
                                 const bool is_henchman) {
        party_members.push_back(SCTracker::PartyMember{
            .primary = primary,
            .secondary = secondary,
            .is_player = is_player,
            .is_hero = is_hero,
            .is_henchman = is_henchman
        });
        agent_id_to_party_index[agent_id] = party_members.size() - 1;
        party_member_currently_dead.push_back(false);
        // NB: Player may have left the game, meaning GW::Agents::GetAgentEncName(agent_id) would fail
        // because the agent is gone. Pass enc_name for real players instead.
        auto enc = std::make_unique<PluginUtils::EncString>();
        enc->reset(enc_name ? enc_name : GW::Agents::GetAgentEncName(agent_id));
        enc->wstring(); // trigger decode
        party_member_enc_names.push_back(std::move(enc));
    };

    for (const auto& player : info->players) {
        if (const GW::Player* gwplayer = GW::PlayerMgr::GetPlayerByID(player.login_number)) {
            add_member(gwplayer->agent_id, gwplayer->name_enc, gwplayer->primary, gwplayer->secondary,
                       true, false, false);
        }
    }
    for (const auto& hero : info->heroes) {
        uint32_t primary = 0;
        uint32_t secondary = 0;
        if (const GW::HeroInfo* hero_info = GW::PartyMgr::GetHeroInfo(hero.hero_id)) {
            primary = static_cast<uint32_t>(hero_info->primary);
            secondary = static_cast<uint32_t>(hero_info->secondary);
        }
        add_member(hero.agent_id, nullptr, primary, secondary, false, true, false);
    }
    for (const auto& hench : info->henchmen) {
        // HenchmanPartyMember exposes only a single profession field - no secondary.
        add_member(hench.agent_id, nullptr, static_cast<uint32_t>(hench.profession), 0, false, false, true);
    }

    if (party_members.empty()) {
        active_capture = false; // no party info found; nothing to log
    }
}

// Takes explicit parameters (rather than reading pending_* member state) so ProcessSync can also call
// this to correct an already-written entry's end_reason once objective data reveals the true outcome
// (see the completed-run override in ProcessSync), not just OnGameSrvTransfer for the live capture.
void SCTracker::WriteLogEntry(const uint32_t utc_start, const uint32_t map_id, const std::string& character_name,
                                 const std::string& end_reason, const std::vector<PartyMember>& members)
{
    if (members.empty()) {
        return;
    }
    const auto path = GetLogFilePath(utc_start);
    if (path.empty()) {
        return;
    }

    try {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::vector<LogEntry> entries;
        if (std::ifstream in{path}; in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            constexpr glz::opts opts{.error_on_unknown_keys = false};
            if (const auto read_ec = glz::read<opts>(entries, ss.str()); read_ec) {
                entries.clear(); // don't trust partially-parsed data on error
            }
        }

        // Replace any earlier entry for the same run (e.g. a district hop re-firing InstanceLoadInfo,
        // or ProcessSync correcting a previously-written end_reason).
        std::erase_if(entries, [&](const LogEntry& e) {
            return e.utc_start == utc_start && e.character_name == character_name;
        });
        entries.push_back(LogEntry{
            .utc_start = utc_start,
            .map_id = map_id,
            .character_name = character_name,
            .end_reason = end_reason,
            .party_members = members,
        });

        std::ofstream out{path};
        if (out.is_open()) {
            out << glz::write_json(entries).value_or(std::string{});
            last_written_utc_start = utc_start;
        }
    } catch (const std::exception&) {
        // Best-effort logging; nothing to do if the runs folder is unwritable.
    }
}

std::optional<SCTracker::PendingSyncEntry> SCTracker::FindNextPendingEntry()
{
    // Only the single oldest not-yet-published entry is ever tracked (see pending_sync's
    // declaration), so there's nothing to dedupe against in memory - last_persisted_utc_start alone
    // determines what's still outstanding.
    std::optional<PendingSyncEntry> best;

    const uint32_t now_utc = static_cast<uint32_t>(time(nullptr));
    for (const uint32_t candidate_ts : {now_utc, now_utc - 86400u}) {
        std::vector<LogEntry> entries;
        if (!ReadJsonArray(GetLogFilePath(candidate_ts), entries)) {
            continue;
        }
        for (auto& e : entries) {
            if (e.utc_start > last_persisted_utc_start && (!best || e.utc_start < best->utc_start)) {
                best = PendingSyncEntry{
                    .utc_start = e.utc_start,
                    .map_id = e.map_id,
                    .character_name = std::move(e.character_name),
                    .end_reason = std::move(e.end_reason),
                    .party_members = std::move(e.party_members),
                    .first_seen_tick = GetTickCount64(),
                };
            }
        }
    }

    return best;
}

void SCTracker::ProcessSync()
{
    if (machine_key.empty()) {
        return; // publishing not configured; local PartyLog_*.json write is still the durable record
    }
    if (plugin_outdated) {
        return; // disabled until updated - see plugin_outdated's declaration
    }

    const uint64_t now = GetTickCount64();

    if (publish_request) {
        if (!publish_request->IsCompleted()) {
            return; // in flight
        }
        if (publish_request->IsSuccessful()) {
            last_persisted_utc_start = publishing_utc_start;
            if (!settings_folder.empty()) {
                SaveSettings(settings_folder.c_str()); // persist the watermark now, not on the host's cadence
            }
            if (pending_sync && pending_sync->utc_start == publishing_utc_start) {
                // Correlate the server-assigned run_id if a vote is pending for this exact run
                // (opened at run-end in OnGameSrvTransfer) - the vote itself was already opened
                // with the right kind/content back then, this just unblocks its eventual submit.
                if (pending_vote_utc_start != 0 && pending_vote_utc_start == pending_sync->utc_start && !vote_run_id_known) {
                    UploadRunResponseDto response;
                    constexpr glz::opts opts{.error_on_unknown_keys = false};
                    if (!glz::read<opts>(response, publish_request->GetContent()) && response.run_id) {
                        pending_vote_run_id = *response.run_id;
                        vote_run_id_known = true;
                        if (vote_pending_submit) {
                            FireVoteSubmit(); // already clicked Submit before run_id was known - send now
                        }
                    }
                    else {
                        // No run_id ever coming for this run (parse failure, or the upload was
                        // silently dropped - see UploadRunResponseDto's comment). Nothing will ever
                        // resolve this vote; reset now rather than leave it stuck blocking every
                        // future vote via OpenVote's guard.
                        AppendLog(std::format("Vote for run {} can never resolve: publish succeeded with no run_id",
                                               pending_sync->utc_start));
                        ResetVoteState();
                    }
                }
                pending_sync = FindNextPendingEntry();
            }
        }
        else {
            last_publish_attempt_tick = now; // back off before retrying a failed publish
            if (publish_request->GetStatusCode() == kHttpStatusUpgradeRequired) {
                plugin_outdated = true;
                if (!version_check_request) {
                    RequestLatestPluginVersion(); // refresh the exact version number for the DrawSettings message
                }
            }
            std::string body = publish_request->GetContent();
            if (body.size() > 200) {
                body.resize(200);
            }
            AppendLog(std::format("Publish failed for run {}: status={} http_code={} body={}",
                                   publishing_utc_start, publish_request->GetStatusStr(),
                                   publish_request->GetStatusCode(), body));
        }
        publish_request.reset();
    }

    if (now - last_publish_attempt_tick < kRetryBackoffMs) {
        return;
    }

    // Only rescans for a new entry when none is currently held: processing is always strictly
    // oldest-first by utc_start, and a scalar already holds the minimum unresolved entry, so
    // rescanning while one is already pending can't change what gets processed next.
    if (!pending_sync && now - last_queue_scan_tick >= kSyncScanIntervalMs) {
        last_queue_scan_tick = now;
        pending_sync = FindNextPendingEntry();
    }
    if (!pending_sync) {
        return;
    }

    // Drain every disqualified entry up front, so a stuck entry (e.g. one that will never find a
    // matching objective) doesn't block whatever comes after it from ever being evaluated - the loop
    // only stops at an entry that's either ready to publish or still within its give-up window.
    // Resolving a disqualified entry immediately searches for the next oldest one via
    // FindNextPendingEntry(), so several already-known-disqualified runs still drain within one
    // ProcessSync() tick instead of one per scan interval. Only a real-player party matching one of
    // the map's backend map_configs sizes is meaningful for the leaderboard (8 for the Underworld,
    // any of 1-8 for the Fissure of Woe - IsAcceptablePartySize; note heroes/henchmen never count
    // toward the real-player total), and a run with no matching GWToolboxdll
    // objective entry can never be leaderboard-eligible anyway. Both cases mark the entry synced
    // instead of retrying it forever - which also cancels any pending vote (CancelPendingVoteIfMatching).
    bool advanced_watermark = false;
    RemoteObjectiveSet objective_set;
    bool have_objective = false;
    while (pending_sync) {
        auto& front = *pending_sync;
        if (!IsAcceptablePartySize(front.map_id, CountRealPlayers(front.party_members))) {
            last_persisted_utc_start = front.utc_start;
            CancelPendingVoteIfMatching(front.utc_start); // before front is invalidated below
            last_queue_scan_tick = now;
            pending_sync = FindNextPendingEntry();
            advanced_watermark = true;
            continue;
        }
        have_objective = TryReadMatchingObjectiveEntry(front.utc_start, objective_set);
        if (have_objective) {
            break; // ready to publish
        }
        if ((now - front.first_seen_tick) < kObjectiveGiveUpTimeoutMs) {
            break; // still within the window; wait for GWToolboxdll's own file to catch up
        }
        // No matching GWToolboxdll objective entry ever showed up - drop this run rather than publish
        // party-only data (no objective timing means it can never be leaderboard-eligible anyway).
        AppendLog(std::format("Dropping run {} (map {}): no matching objective entry after give-up timeout",
                               front.utc_start, front.map_id));
        last_persisted_utc_start = front.utc_start;
        CancelPendingVoteIfMatching(front.utc_start); // before front is invalidated below
        last_queue_scan_tick = now;
        pending_sync = FindNextPendingEntry();
        advanced_watermark = true;
    }
    if (advanced_watermark && !settings_folder.empty()) {
        SaveSettings(settings_folder.c_str()); // persist the advanced watermark now, not on the host's cadence
    }
    if (!pending_sync || !have_objective) {
        return;
    }

    auto& next = *pending_sync;

    // Now that we have the objective data, correct a resign/unknown classification if the run actually
    // finished (e.g. resigning right after killing Dhuum shouldn't read as giving up). Leave "wipe" as
    // reported - a genuine death event stays notable even in the rare case it's right after a kill.
    // Also corrects the local PartyLog_*.json entry, not just the published payload. This is now a
    // rare fallback for the case OnGameSrvTransfer's own dhuum_completed check missed (e.g. a player
    // who joined after Dhuum was already dead) - the common case is already classified "completed" at
    // run end.
    if (next.end_reason != "wipe" && next.end_reason != "completed" && IsRunCompleted(objective_set)) {
        CancelPendingVoteIfMatching(next.utc_start); // unconditional - discard a stale Failure vote;
                                                      // it may have been opened under different
                                                      // permission/outdated state than now, so cancel
                                                      // regardless of that
        next.end_reason = "completed";
        WriteLogEntry(next.utc_start, next.map_id, next.character_name, next.end_reason, next.party_members);
        if (can_report_failures && !plugin_outdated
            && MapSizeHasRoles(next.map_id, CountRealPlayers(next.party_members))) {
            OpenVote(PostRunVoteKind::Mvp, next.map_id, next.utc_start); // late but real - better late than never
        }
    }

    PublishPayload payload{
        .party = LogEntry{
            .utc_start = next.utc_start,
            .map_id = next.map_id,
            .character_name = next.character_name,
            .end_reason = next.end_reason,
            .party_members = next.party_members,
        },
        .objective = std::move(objective_set),
    };

    std::string url;
    ComposeUrl(url, kBaseUrl, kUploadRunsPath);

    publish_request = std::make_unique<AsyncRestClient>();
    publish_request->SetUrl(url.c_str());
    publish_request->SetMethod(HttpMethod::Post);
    publish_request->SetHeader("Content-Type", "application/json");
    publish_request->SetHeader("X-Machine-Key", machine_key.c_str());
    publish_request->SetHeader("X-Plugin-Version", std::to_string(kPluginVersion).c_str());
    publish_request->SetPostContent(glz::write_json(payload).value_or(std::string{}), ContentFlag::Copy);
    publish_request->SetTimeoutSec(10);
    publish_request->SetConnectTimeoutSec(5);
    publish_request->SetVerifyPeer(true);
    publish_request->SetVerifyHost(true);
    publishing_utc_start = next.utc_start;
    publish_request->ExecuteAsync();
}

// Fired once from LoadSettings, right after machine_key loads. can_report_failures defaults false
// and stays false (the safe default - none of the post-run voting logic runs) unless/until this
// completes successfully with a true response.
void SCTracker::RequestReportPermission()
{
    can_report_failures = false;
    if (machine_key.empty()) {
        return;
    }

    std::string url;
    ComposeUrl(url, kBaseUrl, kCanReportFailurePath);

    permission_request = std::make_unique<AsyncRestClient>();
    permission_request->SetUrl(url.c_str());
    permission_request->SetMethod(HttpMethod::Get);
    permission_request->SetHeader("X-Machine-Key", machine_key.c_str());
    permission_request->SetHeader("X-Plugin-Version", std::to_string(kPluginVersion).c_str());
    permission_request->SetTimeoutSec(10);
    permission_request->SetConnectTimeoutSec(5);
    permission_request->SetVerifyPeer(true);
    permission_request->SetVerifyHost(true);
    permission_request->ExecuteAsync();
}

// Polls permission_request completion (called from Update). Any non-success outcome (network
// error, invalid/revoked key, malformed body) just leaves can_report_failures at its false default.
void SCTracker::ProcessPermissionCheck()
{
    if (!permission_request || !permission_request->IsCompleted()) {
        return;
    }
    if (permission_request->IsSuccessful()) {
        CanReportFailureResponseDto response;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(response, permission_request->GetContent())) {
            can_report_failures = response.can_report_failures;
        }
    }
    else if (permission_request->GetStatusCode() == kHttpStatusUpgradeRequired) {
        plugin_outdated = true;
        if (!version_check_request) {
            RequestLatestPluginVersion();
        }
    }
    permission_request.reset();
}

// Opens the vote popup for utc_start's run. Only blocks on a vote the user already committed to
// (clicked Submit) that's still awaiting its run_id - an ignored/uncommitted vote's
// pending_vote_utc_start can never reach here nonzero for a DIFFERENT run, since DrawVotePopup's
// auto-close fully resets uncommitted votes (see its comment) rather than leaving stale state that
// would block every future vote forever.
void SCTracker::OpenVote(const PostRunVoteKind kind, const uint32_t map_id, const uint32_t utc_start)
{
    if (vote_pending_submit && !vote_run_id_known && pending_vote_utc_start != 0 && pending_vote_utc_start != utc_start) {
        AppendLog(std::format("Skipped opening a vote for run {}: a committed vote for run {} is still awaiting its run_id",
                               utc_start, pending_vote_utc_start));
        return;
    }
    pending_vote_kind = kind;
    pending_vote_utc_start = utc_start;
    pending_vote_map_id = map_id;
    pending_vote_run_id = 0;
    vote_run_id_known = false;
    vote_pending_submit = false;
    show_vote_popup = true;
    vote_role_checked.fill(false);
    vote_submit_error.clear();
    vote_popup_opened_tick = GetTickCount64();
}

void SCTracker::ResetVoteState()
{
    show_vote_popup = false;
    pending_vote_kind = PostRunVoteKind::None;
    pending_vote_utc_start = 0;
    pending_vote_map_id = 0;
    pending_vote_run_id = 0;
    vote_run_id_known = false;
    vote_pending_submit = false;
    vote_role_checked.fill(false);
    vote_submit_error.clear();
    vote_popup_opened_tick = 0;
}

// No-op unless a vote is actually pending for utc_start - this run either turned out not to be a
// real failure (resign-that-actually-completed correction), or will never get a run_id (wrong-size
// party skip / give-up-timeout drop).
void SCTracker::CancelPendingVoteIfMatching(const uint32_t utc_start)
{
    if (pending_vote_utc_start == utc_start && pending_vote_utc_start != 0) {
        ResetVoteState();
    }
}

// Builds and fires submit_request from pending_vote_run_id/vote_role_checked - shared by
// DrawVotePopup's Submit button (fired immediately when vote_run_id_known) and ProcessSync's
// publish-success handler (fired once the run_id arrives, if vote_pending_submit was already set).
// Called before vote_run_id_known too (from the Submit button): in that case it just records the
// commitment and returns, and ProcessSync calls it again once the run_id resolves to actually send.
void SCTracker::FireVoteSubmit()
{
    vote_pending_submit = true; // idempotent
    if (!vote_run_id_known) {
        return; // ProcessSync will call this again once the run_id resolves
    }
    ReportVotePayload payload{.run_id = pending_vote_run_id};
    for (size_t i = 0; i < kVoteRoles.size(); i++) {
        if (vote_role_checked[i]) {
            payload.roles.emplace_back(kVoteRoles[i]);
        }
    }

    std::string url;
    ComposeUrl(url, kBaseUrl, pending_vote_kind == PostRunVoteKind::Mvp ? kReportMvpPath : kReportFailurePath);

    submit_request = std::make_unique<AsyncRestClient>();
    submit_request->SetUrl(url.c_str());
    submit_request->SetMethod(HttpMethod::Post);
    submit_request->SetHeader("Content-Type", "application/json");
    submit_request->SetHeader("X-Machine-Key", machine_key.c_str());
    submit_request->SetHeader("X-Plugin-Version", std::to_string(kPluginVersion).c_str());
    submit_request->SetPostContent(glz::write_json(payload).value_or(std::string{}), ContentFlag::Copy);
    submit_request->SetTimeoutSec(10);
    submit_request->SetConnectTimeoutSec(5);
    submit_request->SetVerifyPeer(true);
    submit_request->SetVerifyHost(true);
    vote_submit_error.clear();
    submit_request->ExecuteAsync();
}

// Polls submit_request completion (called from Update, same as ProcessSync polls publish_request).
// On success the popup closes; on failure the truncated response body is kept on screen and
// vote_pending_submit is cleared so the user can see why and retry (re-clicking Submit) before the
// vote window closes.
void SCTracker::ProcessVoteSubmit()
{
    if (!submit_request || !submit_request->IsCompleted()) {
        return;
    }
    if (submit_request->IsSuccessful()) {
        ResetVoteState();
    }
    else {
        if (submit_request->GetStatusCode() == kHttpStatusUpgradeRequired) {
            plugin_outdated = true;
            if (!version_check_request) {
                RequestLatestPluginVersion();
            }
        }
        std::string body = submit_request->GetContent();
        if (body.size() > 200) {
            body.resize(200);
        }
        vote_pending_submit = false; // unlock the popup's controls so the user can retry
        vote_submit_error = std::format("Submit failed: status={} http_code={} body={}",
                                         submit_request->GetStatusStr(), submit_request->GetStatusCode(), body);
        AppendLog(std::format("Vote submit failed for run_id {}: {}", pending_vote_run_id, vote_submit_error));
    }
    submit_request.reset();
}

// Fired once from LoadSettings (no machine key needed - GET /plugin-version is public) and again,
// on demand, from the 426 handlers above if a reactive check fires before this build's own copy has
// ever completed successfully - guarded by "if (!version_check_request)" at each call site so it
// never stomps one already in flight.
void SCTracker::RequestLatestPluginVersion()
{
    std::string url;
    ComposeUrl(url, kBaseUrl, kPluginVersionPath);

    version_check_request = std::make_unique<AsyncRestClient>();
    version_check_request->SetUrl(url.c_str());
    version_check_request->SetMethod(HttpMethod::Get);
    version_check_request->SetTimeoutSec(10);
    version_check_request->SetConnectTimeoutSec(5);
    version_check_request->SetVerifyPeer(true);
    version_check_request->SetVerifyHost(true);
    version_check_request->ExecuteAsync();
}

// Polls version_check_request completion (called from Update). Only ever sets plugin_outdated to
// true, never back to false within the same session - once flagged, it stays flagged until the host
// restarts the plugin with an updated build (there's no code path that clears it mid-session).
void SCTracker::ProcessVersionCheck()
{
    if (!version_check_request || !version_check_request->IsCompleted()) {
        return;
    }
    if (version_check_request->IsSuccessful()) {
        PluginVersionResponseDto response;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(response, version_check_request->GetContent())) {
            latest_known_plugin_version = response.version;
            if (kPluginVersion < response.version) {
                NotifyPluginOutdated();
            }
        }
    }
    version_check_request.reset();
}

void SCTracker::NotifyPluginOutdated()
{
    if (plugin_outdated) {
        return; // already notified this session
    }
    plugin_outdated = true;
    pending_outdated_chat_notice = true;
}

void SCTracker::ProcessPendingOutdatedNotice()
{
    if (!pending_outdated_chat_notice || !GW::Map::GetIsMapLoaded()) {
        return;
    }
    pending_outdated_chat_notice = false;
    // CHANNEL_WARNING renders as a big center-screen system banner, not a chat-box line - use one of
    // the GWCA-reserved channels instead (same as GWToolboxdll's own plugin-detected notice in
    // PluginModule.cpp) so this shows up as normal, scrollable chat text.
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2,
                         L"<c=#FF0000>SCTracker is out of date - syncing and vote reporting are disabled "
                         "until you redownload from gwsctracker.com/account.</c>",
                         L"SCTracker", false);
}

void SCTracker::DrawVotePopup()
{
    // can_report_failures/plugin_outdated are re-checked here too (not just at the trigger sites
    // that set show_vote_popup) in case either changed server-side while the popup sat open.
    if (!show_vote_popup || !can_report_failures || plugin_outdated) {
        return;
    }

    // Auto-close kVoteWindowMs after a real trigger opened it - a vote submitted long after the run
    // in question is no longer useful. Only applies when the timer is actually running
    // (vote_popup_opened_tick != 0); a manually-opened popup (see its member comment) has no timer
    // to expire and stays open until Dismissed.
    const uint64_t now = GetTickCount64();
    const bool timer_active = vote_popup_opened_tick != 0 && (now - vote_popup_opened_tick) < kVoteWindowMs;
    if (vote_popup_opened_tick != 0 && !timer_active) {
        if (vote_pending_submit) {
            // Committed before the window closed - preserve everything else so ProcessSync can
            // still correlate the run_id and FireVoteSubmit can still send it later.
            show_vote_popup = false;
            vote_popup_opened_tick = 0;
        }
        else {
            ResetVoteState(); // nothing committed - nothing worth preserving
        }
        return;
    }

    const bool is_mvp = pending_vote_kind == PostRunVoteKind::Mvp;
    ImGui::SetNextWindowSize(ImVec2(420.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(is_mvp ? "SCTracker: MVP Vote" : "SCTracker: Run Failure", &show_vote_popup)) {
        if (is_mvp) {
            ImGui::TextWrapped("The most recent run completed. Vote for which role you believe performed best - "
                                "votes from everyone in the party who reports get combined server-side.");
        }
        else {
            ImGui::TextWrapped("The most recent run ended in a wipe or resign. Vote for which role(s) you believe "
                                "were at fault - votes from everyone in the party who reports get combined "
                                "server-side to determine the actual cause.");
        }
        if (timer_active) {
            const uint64_t remaining_sec = (kVoteWindowMs - (now - vote_popup_opened_tick)) / 1000;
            ImGui::Text("Voting closes in %llus", static_cast<unsigned long long>(remaining_sec));
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                                "No active vote - this popup was opened manually, not triggered by a run "
                                "ending. Voting is disabled.");
        }
        ImGui::Separator();

        const bool locked_in = vote_pending_submit;
        ImGui::BeginDisabled(locked_in || !timer_active);
        for (size_t i = 0; i < kVoteRoles.size(); i++) {
            // Only offer roles that can actually validate for this run's map - for a Fissure of Woe
            // duo that's just Ranger/Derv/Nobody (see VoteRoleVisibleForMap); a hidden role is
            // never checked, so FireVoteSubmit never sends it.
            if (!VoteRoleVisibleForMap(pending_vote_map_id, i)) {
                continue;
            }
            if (is_mvp) {
                // MVP credits exactly one role (including "Nobody" as "no standout") - a radio
                // button group rather than the Failure vote's checkboxes, which can blame several
                // roles for the same wipe at once.
                if (ImGui::RadioButton(kVoteRoles[i], vote_role_checked[i])) {
                    vote_role_checked.fill(false);
                    vote_role_checked[i] = true;
                }
            }
            // "Nobody" is mutually exclusive with every other reason: checking it clears the rest,
            // and checking any other reason clears it.
            else if (ImGui::Checkbox(kVoteRoles[i], &vote_role_checked[i]) && vote_role_checked[i]) {
                if (i == kNobodyVoteRoleIndex) {
                    for (size_t j = 0; j < vote_role_checked.size(); j++) {
                        if (j != i) {
                            vote_role_checked[j] = false;
                        }
                    }
                }
                else {
                    vote_role_checked[kNobodyVoteRoleIndex] = false;
                }
            }
        }

        if (ImGui::Button("Unselect All")) {
            vote_role_checked.fill(false);
        }
        ImGui::EndDisabled();

        if (locked_in && !vote_run_id_known) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                                "Vote recorded - will submit once this run finishes uploading.");
        }

        if (!vote_submit_error.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", vote_submit_error.c_str());
        }

        ImGui::Separator();
        ImGui::BeginDisabled(locked_in || !timer_active);
        if (ImGui::Button("Submit Vote")) {
            FireVoteSubmit();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) {
            ResetVoteState();
        }
    }
    ImGui::End();
}

void SCTracker::Draw(IDirect3DDevice9*)
{
    DrawVotePopup();
}

void SCTracker::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    settings_folder = folder;
    LoadSetting("machine_key", machine_key);
    LoadSetting("last_persisted_utc_start", last_persisted_utc_start);
    PluginUtils::StrCopy(machine_key_buf, machine_key.c_str(), sizeof(machine_key_buf));
    RequestLatestPluginVersion(); // no machine key needed - public endpoint, checked before anything else
    RequestReportPermission();
}

void SCTracker::SaveSettings(const wchar_t* folder)
{
    settings_folder = folder;
    SaveSetting("machine_key", machine_key);
    SaveSetting("last_persisted_utc_start", last_persisted_utc_start);
    ToolboxPlugin::SaveSettings(folder);
}

void SCTracker::DrawSettings()
{
    if (plugin_outdated) {
        if (latest_known_plugin_version > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                "This SCTracker build is out of date (yours: %d, latest: %d). Syncing and "
                                "vote reporting are disabled until you redownload from gwsctracker.com/account.",
                                kPluginVersion, latest_known_plugin_version);
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                "This SCTracker build is out of date. Syncing and vote reporting are "
                                "disabled until you redownload from gwsctracker.com/account.");
        }
        ImGui::Separator();
    }
    else if (latest_known_plugin_version > 0) {
        // Only claim up-to-date once a version check has actually succeeded (latest_known_plugin_version
        // stays 0 otherwise, e.g. the request is still in flight or failed) - see ProcessVersionCheck.
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "SCTracker is up to date (version %d).", kPluginVersion);
        ImGui::Separator();
    }

    ImGui::TextWrapped("Logs party composition and run outcome for each speedclear run.");
    if (last_written_utc_start) {
        std::string time_str;
        PluginUtils::TimeToString(last_written_utc_start, time_str);
        ImGui::Text("Last run logged: %s", time_str.c_str());
    }

    ImGui::Separator();
    ImGui::TextWrapped("Syncs runs to gwsctracker.com. Leave the key blank to log locally only.");
    if (ImGui::InputText("Machine Key", machine_key_buf, sizeof(machine_key_buf), ImGuiInputTextFlags_Password)) {
        machine_key = machine_key_buf;
    }
    ImGui::Text("Sync: %s", pending_sync ? "1 pending" : "idle");
    if (last_persisted_utc_start) {
        std::string time_str;
        PluginUtils::TimeToString(last_persisted_utc_start, time_str);
        ImGui::Text("Last synced run: %s", time_str.c_str());
    }
    if (!machine_key.empty()) {
        ImGui::Text("Vote reporting: %s", can_report_failures ? "enabled" : "not permitted for this key");
    }
}
