#pragma once

#include <GWCA/Constants/Maps.h>

#include <ToolboxWindow.h>
#include <Windows/Splits/SplitsProfile.h>

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class GoalClock;
struct GoalList;
struct ComparisonSplits;
struct RecentRun;

inline constexpr int kProfileCount = 3;
// int, not enum: also index arrays + settings.
inline constexpr int kProfileManual = 0;
inline constexpr int kProfileRunning = 1;
inline constexpr int kProfileSC = 2;

class SplitsWindow : public ToolboxWindow {
    SplitsWindow() = default;
    ~SplitsWindow() override = default;

public:
    static SplitsWindow& Instance()
    {
        static SplitsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Splits"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }
    [[nodiscard]] const char* Description() const override { return "Speedrun timer that splits automatically on in-game goals"; }

    void Initialize() override;
    void Terminate() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettingsInternal() override;
    void DrawHelp() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    void StartRun();
    void ResetRun();
    void TriggerManualSplit();
    void SwitchProfile(int idx);
    // Last session's paused run, offered in this profile until resumed, discarded or replaced by a new run. nullptr = none.
    [[nodiscard]] const char* PausedRunName() const;
    [[nodiscard]] double PausedRunTime() const;
    void ResumePausedRun();
    void DiscardPausedRun();
    [[nodiscard]] const char* StartDisabledReason() const;
    [[nodiscard]] const char* SplitDisabledReason() const;

    // Shared by all profiles on purpose.
    Color& ColorCompleted();
    Color& ColorActive();
    Color& ColorRealTime();
    Color& ColorGameTime();
    Color& ColorPbAhead();
    Color& ColorPbBehind();

    [[nodiscard]] SplitsProfile& ActiveProfile();
    [[nodiscard]] const SplitsProfile& ActiveProfile() const;
    [[nodiscard]] int ActiveProfileIdx() const;
    [[nodiscard]] std::array<SplitsProfile, kProfileCount>& Profiles();

    [[nodiscard]] const GoalClock& Clock() const;
    [[nodiscard]] GoalList* List();
    // Auto-reset profiles: the finished run as it looked at its end, shown until the next attempt starts. nullptr otherwise.
    [[nodiscard]] const GoalList* LastRunList() const;
    [[nodiscard]] const GoalClock& LastRunClock() const;
    [[nodiscard]] const ComparisonSplits& LastRunComparison() const;
    [[nodiscard]] bool LastRunFailed() const;
    [[nodiscard]] double TotalPausedReal() const;

    void NewActiveList(const char* name);
    // clear_preset=false for auto-saves: only user Save turn preset into plain list.
    void SaveActiveList(bool clear_preset = true);
    void LoadActiveList(const std::wstring& path);
    // keep_preset: SC auto-detect only. Set before LoadPB so history read from Defaults\.
    void SetActiveList(GoalList list, bool keep_preset = false);
    [[nodiscard]] const std::vector<std::pair<std::string, std::wstring>>& GetSavedLists() const;
    [[nodiscard]] const std::unordered_map<GW::Constants::MapID, std::vector<std::pair<std::string, std::wstring>>>& GetSavedListsByArea() const;

    [[nodiscard]] const ComparisonSplits& ActiveComparison() const;
    // For an attempt shown in Run History: DoA's PB is laid out in that attempt's own rotation.
    [[nodiscard]] ComparisonSplits ComparisonFor(const GoalList& list, SplitsProfile::ComparisonMode mode) const;
    [[nodiscard]] const ComparisonSplits& Comparison(SplitsProfile::ComparisonMode mode) const;

    [[nodiscard]] const std::vector<RecentRun>& RecentRuns() const;
    [[nodiscard]] int HistoryGeneration() const;
    [[nodiscard]] int PBAttemptNumber(bool game) const;
    [[nodiscard]] double PBTotal(bool game) const;
    [[nodiscard]] const std::vector<int>& BestSegRealAttempt() const;
    [[nodiscard]] const std::vector<int>& BestSegGameAttempt() const;

    [[nodiscard]] std::vector<RecentRun> LoadFullRunHistory() const;
    // Renumbers: reload LoadFullRunHistory() after.
    void DeleteRunsFromHistory(const std::vector<int>& attempt_numbers);
    void ClearRunHistory();

    void DrawNuzlockeSection();
    [[nodiscard]] bool NuzlockePointsEnabled() const;
    [[nodiscard]] int NuzlockeTotalPoints() const;

private:
    // Bound through the Hotkeys window's Send Chat action, like /pcons.
    static void CHAT_CMD_FUNC(CmdSplits);

    [[nodiscard]] bool NuzlockeDeathTrackerEnabled() const;
    [[nodiscard]] std::filesystem::path ActiveSplitsFolder() const;
    [[nodiscard]] std::filesystem::path ActiveRunsFolder() const;
    [[nodiscard]] std::filesystem::path RunHistoryFilePath() const;
    [[nodiscard]] int ActiveGoalCount() const;
    [[nodiscard]] int CompletedGoalCount() const;

    void SaveResumeState();
    void DeleteResumeState();
    void SaveCompletedRun();
    void FailRun();
    void SaveRunToHistory(bool failed);
    void LoadPB(bool refresh_comparisons = true);
    void UpdateReferenceIfPB();
    void ResetRunFlags();
    // ResetRun minus LiveSplit, Nuzlocke and the held finished run: after a finish those stay until the next attempt.
    void ClearRun();
    // by_hand: the Start button, so RTA counts from the press rather than the instance's creation.
    void BeginRun(bool by_hand);
    // Order matter: Attach setup (starts_immediately) need list filled.
    void ReplaceActiveList(const std::function<void()>& populate);
    void LoadProfileLastList();

    // Kept apart from GoalEngine switch on purpose: firing and clock policy separate.
    void ApplyTimerPolicy(bool just_entered_map);
    // Never mid-run. Swaps to the entered area's list; a list for the same area is kept. DoA first: rotation not map-driven.
    void ApplySCAutoLoadPreset(bool just_entered_map);
    void StartDoAZone();
    [[nodiscard]] bool InDoAHub() const;
    // A Running attempt only begins in the route's first area: elsewhere a step would start a run that fails on a wrong turn.
    [[nodiscard]] bool AtRouteStart() const;
    // Skip unless list has MobKill: full agent walk not free.
    void RefreshMobNameCache();

    void SendLiveSplit(const char* command);
    // Per frame, acts only when enabled/port differ from what was last applied, so a busy port isn't retried every frame.
    void ApplyLiveSplitSettings();
    // One split per newly completed goal, so LiveSplit never falls behind (manual splits, several goals in one tick).
    void SyncLiveSplitSplits();
    // LiveSplit ignores splits while paused; tracked so a sync can briefly resume around them.
    void SetLiveSplitPaused(bool paused);
};
