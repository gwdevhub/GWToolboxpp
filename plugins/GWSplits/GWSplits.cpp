#include <GWSplits.h>

#include <ConditionIO.h>
#include <InstanceInfo.h>
#include <QuestInfo.h>
#include <enumUtils.h>
#include <Keys.h>

#include <BackupManager.h>
#include <PluginUtils.h>
#include <Utils/FontLoader.h>
#include <io.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Hook.h>

#include <ImGuiCppWrapper.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static GWSplits instance;
    return &instance;
}

namespace {
    GW::HookEntry InstanceLoadStart_Entry;
    GW::HookEntry InstanceTimer_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry DungeonReward_Entry;
    GW::HookEntry DoACompleteZone_Entry;
    GW::HookEntry ChatCmd_HookEntry;

    static void onDisplayDialogDecoded(void* instancePtr, const wchar_t* decoded)
    {
        if (!decoded || !instancePtr) return;
        const auto gwsplits = reinterpret_cast<GWSplits*>(instancePtr);

        gwsplits->handleTrigger(Trigger::DisplayDialog, [&](const Split& s){ return !s.triggerData.message.empty() && WStringToString(decoded).contains(s.triggerData.message); });
    }
    
    bool isValid(const std::chrono::steady_clock::time_point& time)
    {
        return time.time_since_epoch().count();
    }

    std::chrono::steady_clock::time_point now()
    {
        return std::chrono::steady_clock::now();
    }

    constexpr int earlyResultTimeMs = 10'000;
    constexpr long currentVersion = 11;
    constexpr auto grey = ImVec4{0.7f, 0.7f, 0.7f, 1.f};
    constexpr auto gold = ImVec4{0.95686f, 0.7882f, 0.f, 1.f};
    constexpr auto white = ImVec4{1.f, 1.f, 1.f, 1.f};
    constexpr auto currentSplitColor = ImVec4{0.3725f, 0.3961f, 0.7529f, 0.6f};
    const wchar_t* settingsFolder = nullptr; // Folder loaded from. Used for PB saving button

    std::chrono::steady_clock::time_point instanceStart;

    std::chrono::steady_clock::time_point getInstanceStart()
    {
        if (!isValid(instanceStart)) {
            instanceStart = now() - std::chrono::milliseconds(GW::Map::GetInstanceTime());
        }

        return instanceStart;
    }

    bool checkConditions(const std::vector<ConditionPtr>& conditions, bool allowEmpty)
    {
        return (allowEmpty || !conditions.empty()) && std::ranges::all_of(conditions, [&](const auto& cond) { return cond->check(); });
    }

    enum class ToStringStyle 
    {
        MinutesSecondsCentiseconds,
        SecondsCentiseconds,
        SecondsMilliseconds
    };
    std::string timeToString(int time, ToStringStyle style = ToStringStyle::MinutesSecondsCentiseconds)
    {
        const auto sign = time < 0 ? "-" : (style == ToStringStyle::MinutesSecondsCentiseconds ? " " : "+");
        time = std::abs(time);
        const auto minutes = time / 60'000;
        auto seconds = (time - (60'000 * minutes)) / 1'000;
        auto milliseconds = time % 1000;

        if (style != ToStringStyle::SecondsMilliseconds) 
        {
            auto centiSeconds = (int)std::round(milliseconds / 100.f);
            seconds += centiSeconds / 10;
            centiSeconds = centiSeconds % 10;
            milliseconds = 100 * centiSeconds;
        }

        std::string result = sign;
        if (minutes > 0 || style == ToStringStyle::MinutesSecondsCentiseconds) {
            result += std::to_string(minutes) + ":";
        }
        if (seconds < 10 && (style == ToStringStyle::MinutesSecondsCentiseconds || minutes > 0)) {
            result += "0";
        }
        
        if (style != ToStringStyle::SecondsMilliseconds)
            result += std::to_string(seconds) + "." + std::to_string(milliseconds / 100);
        else
            result += std::to_string(seconds) + "." + std::to_string(milliseconds);

        while (result.size() < 6)
            result = " " + result;
        return result;
    }

    int stringToTime(std::string string)
    {
        std::optional<int> minutes;
        std::optional<int> seconds;
        std::optional<int> ms;

        if (const auto minuteSep = string.find(":"); minuteSep != std::string::npos) {
            try {
                minutes = std::stoi(string.substr(0, minuteSep));
            } catch (...) {}
            string = string.substr(minuteSep + 1, string.size());
        }
        if (const auto secondsSep = string.find("."); secondsSep != std::string::npos) {
            try {
                seconds = std::stoi(string.substr(0, secondsSep));
            } catch (...) {}

            string = string.substr(secondsSep + 1, string.size());
        }
        try {
            ms = std::stoi(string.substr(0, string.size()));
        } catch (...) {}

        auto result = 0;
        if (minutes.has_value()) {
            result += *minutes * 60'000;
        }
        if (seconds.has_value()) {
            result += *seconds * 1'000;
        }
        if (ms.has_value()) {
            if (seconds.has_value()) {
                while (*ms > 999) {
                    *ms /= 10;
                }
                while (*ms > 0 && *ms < 100) {
                    *ms *= 10;
                }
                result += *ms;
            }
            else {
                // No "." seperator found. Reinterpret as seconds
                result += *ms * 1000;
            }
        }

        return result;
    }

    bool drawConditionSetSelector(std::vector<ConditionPtr>& conditions)
    {
        bool hasEditedSettings = false;
        using ConditionIt = decltype(conditions.begin());
        std::optional<ConditionIt> conditionToDelete = std::nullopt;
        std::optional<std::pair<ConditionIt, ConditionIt>> conditionsToSwap = std::nullopt;
        for (auto it = conditions.begin(); it < conditions.end(); ++it) {
            ImGui::PushID(it - conditions.begin() + 1);
            if (ImGui::Button("X", ImVec2(20, 0))) {
                conditionToDelete = it;
            }
            ImGui::SameLine();
            if (ImGui::Button("^", ImVec2(20, 0))) {
                if (it != conditions.begin()) conditionsToSwap = {it - 1, it};
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(20, 0))) {
                if (it + 1 != conditions.end()) conditionsToSwap = {it, it + 1};
            }
            ImGui::SameLine();
            hasEditedSettings |= (*it)->drawSettings();
            ImGui::PopID();
        }
        if (conditionToDelete.has_value()) conditions.erase(conditionToDelete.value());
        if (conditionsToSwap.has_value()) std::swap(*conditionsToSwap->first, *conditionsToSwap->second);
        // Add condition
        if (auto newCondition = drawConditionSelector(200.f)) 
        {
            conditions.push_back(std::move(newCondition));
        }
        return hasEditedSettings;
    }
    void rightAlignedText(const std::string& s)
    {
        auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(s.c_str()).x - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (posX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(posX);
        ImGui::TextUnformatted(s.c_str()); // Fixed: ImGui::Text treated the string as a format string; '%' in output caused UB.
    }
    auto mix(ImVec4 a, ImVec4 b, float aNess)
    {
        const auto mixColor = [aNess](float a, float b) {
            return std::sqrt((aNess * a * a + (1 - aNess) * b * b) / 2);
        };
        return ImGui::ColorConvertFloat4ToU32(ImVec4(mixColor(a.x, b.x), mixColor(a.y, b.y), mixColor(a.z, b.z), 255));
    }
    auto getTimerColor(int timeDiff, bool isPB)
    {
        constexpr auto offWhite = 220.f / 255.f;
        constexpr auto lime  = ImVec4{offWhite, 1.f, offWhite, 1.f};
        constexpr auto green = ImVec4{0.1f,     1.f, 0.f,      1.f};
        
        constexpr auto lightRed = ImVec4{1.f, offWhite, offWhite, 1.f};
        constexpr auto red      = ImVec4{1.f, 0.1f,     0.1f,     1.f};

        if (isPB) 
        {
            return ImGui::ColorConvertFloat4ToU32(gold);
        }
        if (timeDiff == 0) 
        {
            return ImGui::ColorConvertFloat4ToU32(white);
        }
        if (timeDiff < 0) 
        {
            timeDiff = std::max(timeDiff, -earlyResultTimeMs);
            const auto greenNess = float(-timeDiff) / earlyResultTimeMs;
            return mix(green, lime, greenNess);
        }
        timeDiff = std::min(timeDiff, earlyResultTimeMs);
        const auto redNess = float(timeDiff) / earlyResultTimeMs;
        return mix(lightRed, red, 1.f - redNess); // Fixed: a dead 'result' variable and a duplicate mix() call were here.
    }

    std::string serialize(const Split& split)
    {
        OutputStream stream;
        stream << 'S';

        writeStringWithSpaces(stream, split.name);
        stream << split.trackedTime;
        stream << split.pbSegmentTime;

        stream << split.trigger;
        switch (split.trigger) 
        {
            case Trigger::None:
            case Trigger::DungeonReward:
                break;
            case Trigger::DoaZoneComplete:
                stream << split.triggerData.doaZone;
                break;
            case Trigger::DisplayDialog:
                writeStringWithSpaces(stream, split.triggerData.message);
                break;
        }
        
        stream.writeSeparator();

        for (const auto& condition : split.conditions) {
            condition->serialize(stream);
            stream.writeSeparator();
        }

        return stream.str();
    }
    std::optional<Split> deserializeSplit(InputStream& stream)
    {
        Split result;

        result.name = readStringWithSpaces(stream);
        stream >> result.trackedTime;
        stream >> result.pbSegmentTime;

        stream >> result.trigger;
        switch (result.trigger) 
        {
            case Trigger::None:
            case Trigger::DungeonReward:
                break;
            case Trigger::DoaZoneComplete:
                stream >> result.triggerData.doaZone;
                break;
            case Trigger::DisplayDialog:
                result.triggerData.message = readStringWithSpaces(stream);
                break;
        }

        stream.proceedPastSeparator();

        while (stream.peek() == 'C') {
            stream.get();
            if (auto cond = readCondition(stream)) 
                result.conditions.push_back(cond);
            stream.proceedPastSeparator();
        }

        return result;
    }

    std::string serialize(const Run& run)
    {
        OutputStream stream;

        stream << 'R';
        writeStringWithSpaces(stream, run.name);
        writeStringWithSpaces(stream, run.topText);

        stream.writeSeparator();

        for (const auto& split : run.splits) {
            stream << serialize(split);
            stream.writeSeparator();
        }

        stream << 'T';
        for (const auto& condition : run.trackConditions) {
            condition->serialize(stream);
            stream.writeSeparator();
        }

        return stream.str();
    }
    std::optional<Run> deserializeRun(InputStream& stream)
    {
        Run result;

        result.name = readStringWithSpaces(stream);
        result.topText = readStringWithSpaces(stream);

        stream.proceedPastSeparator();

        while (stream.peek() == 'S') 
        {
            stream.get();
            if (auto split = deserializeSplit(stream)) 
                result.splits.push_back(*split);
            stream.proceedPastSeparator();
        }

        if (stream.peek() == 'T')
        {
            stream.get();
            while (stream.peek() == 'C') {
                stream.get();

                if (auto cond = readCondition(stream)) {
                    result.trackConditions.push_back(cond);
                }
                stream.proceedPastSeparator();
            }
        }

        return result;
    }
    void removePBs(Run& run) 
    {
        for (Split& split : run.splits) 
        {
            split.pbSegmentTime = 0;
            split.displayPBTime = "0:00.0";
        }
    }

    std::string replacePlaceholders(std::string_view string, std::unordered_map<std::string, std::string>&& variables)
    {
        auto result = std::string{string};

        for (const auto& [name, value] : variables) {
            while (true) {
                const auto index = result.find("$" + name);
                if (index == std::string::npos) break;
                result.replace(index, name.size() + 1, value);
            }
        }

        return result;
    }
} // namespace

bool GWSplits::drawSplits(std::vector<Split>& splits)
{
    bool hasEditedSettings = false;

    using SplitIt = decltype(splits.begin());
    std::optional<SplitIt> splitToDelete = std::nullopt;
    std::optional<std::pair<SplitIt, SplitIt>> splitsToSwap = std::nullopt;

    for (auto splitIt = splits.begin(); splitIt < splits.end(); ++splitIt) {
        ImGui::PushID(splitIt - splits.begin());
        const auto treeHeader = splitIt->name + "###0";
        const auto treeOpen = ImGui::TreeNodeEx(treeHeader.c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding);

        const auto offset = 126.f + (treeOpen ? 0.f : 21.f);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - offset);
        ImGui::PushItemWidth(70.f);
        if (ImGui::InputText("##TrackedTime", &splitIt->displayTrackedTime)) {
            splitIt->trackedTime = stringToTime(splitIt->displayTrackedTime);
            splitIt->displayTrackedTime = timeToString(splitIt->trackedTime);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20, 0))) {
            splitToDelete = splitIt;
        }
        ImGui::SameLine();
        if (ImGui::Button("^", ImVec2(20, 0)) && splitIt != splits.begin()) {
            splitsToSwap = {splitIt - 1, splitIt};
        }
        ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(20, 0)) && splitIt + 1 != splits.end()) {
            splitsToSwap = {splitIt, splitIt + 1};
        }

        if (treeOpen) {
            ImGui::PushID(0);
            hasEditedSettings |= drawConditionSetSelector(splitIt->conditions);
            ImGui::SameLine();
            drawTriggerSelector(splitIt->trigger, splitIt->triggerData, 100.f);
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::PushItemWidth(200.f);
            ImGui::InputText("Name", &splitIt->name);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(100.f);
            if (ImGui::InputText("Segment PB", &splitIt->displayPBTime)) {
                splitIt->pbSegmentTime = stringToTime(splitIt->displayPBTime);
                splitIt->displayPBTime = timeToString(splitIt->pbSegmentTime);
                if (currentRun) // Update sum of best in case we're editing the current run
                    sumOfBest = std::accumulate(currentRun->splits.begin(), currentRun->splits.end(), 0, [](int sum, const Split& s) { return sum + s.pbSegmentTime; });
            }
            ImGui::PopItemWidth();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (splitToDelete.has_value()) splits.erase(splitToDelete.value());
    if (splitsToSwap.has_value()) std::swap(*splitsToSwap->first, *splitsToSwap->second);

    return hasEditedSettings;
}
bool GWSplits::drawRuns()
{
    bool hasEditedSettings = false;

    using RunIt = decltype(runs.begin());
    std::optional<RunIt> runToDelete = std::nullopt;
    std::optional<std::pair<RunIt, RunIt>> runsToSwap = std::nullopt;

    for (auto runIt = runs.begin(); runIt < runs.end(); ++runIt) {
        ImGui::PushID(runIt - runs.begin());
        ImGui::PushStyleColor(ImGuiCol_Header, {100.f / 255, 100.f / 255, 100.f / 255, 0.5});

        const auto header = (*runIt)->name + "###0";
        const auto treeOpen = ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - (treeOpen ? 127.f : 148.f));
        if (ImGui::Button("X", ImVec2(20, 0))) {
            runToDelete = runIt;
        }
        ImGui::SameLine();
        if (ImGui::Button("^", ImVec2(20, 0)) && runIt != runs.begin()) {
            runsToSwap = {runIt - 1, runIt};
        }
        ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(20, 0)) && runIt + 1 != runs.end()) {
            runsToSwap = {runIt, runIt + 1};
        }

        if (treeOpen) {
            if (ImGui::TreeNodeEx("Track", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                hasEditedSettings |= drawConditionSetSelector((*runIt)->trackConditions);

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Splits", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                hasEditedSettings |= drawSplits((*runIt)->splits);
                if (ImGui::Button("Add split", ImVec2(100, 0))) 
                {
                    (*runIt)->splits.push_back({});
                }

                ImGui::TreePop();
            }

            if (ImGui::Button("Copy run", ImVec2(100, 0))) 
            {
                if (const auto encoded = encodeString(std::to_string(currentVersion) + " " + serialize(**runIt))) {
                    logMessage("Copy run " + (*runIt)->name + " to clipboard", Name());
                    ImGui::SetClipboardText(encoded->c_str());
                }
            }
            ImGui::SameLine();
            const auto boxWidth = ImGui::GetContentRegionAvail().x / 2 - 60.f;
            ImGui::PushItemWidth(boxWidth);
            ImGui::InputText("Name", &(*runIt)->name);
            ImGui::SameLine();
            ImGui::InputText("Top text", &(*runIt)->topText);
            ImGui::PopItemWidth();

            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (runToDelete.has_value()) runs.erase(runToDelete.value());
    if (runsToSwap.has_value()) std::swap(*runsToSwap->first, *runsToSwap->second);

    return hasEditedSettings;
}

void GWSplits::Update(float diff)
{
    ToolboxUIPlugin::Update(diff);

    if (!isCurrentRunTracked) 
    {
        for (auto& run : runs) {
            if (checkConditions(run->trackConditions, false)) {
                if (currentRun != run) {
                    run->startTime = getInstanceStart();
                }
                currentRun = run;
                isCurrentRunTracked = true;
                refreshDisabledKeys();
                break;
            }
        }
    }

    if (!currentRun || currentRun->splits.empty() || !isCurrentRunTracked)
    {
        return;
    }

    QuestInfo::getInstance().update();

    if (sumOfBest == 0) 
    {
        sumOfBest = std::accumulate(currentRun->splits.begin(), currentRun->splits.end(), 0, [](int sum, const Split& s) {
            return sum + s.pbSegmentTime;
        });
    }

    auto& currentSplits = currentRun->splits;
    auto currentSplitIt = std::ranges::find_if(currentSplits, [](const Split& split) { return !split.completed; });

    if (currentSplitIt != currentSplits.end()) 
    {
        runTime = getRunTime();
        segmentTime = runTime - segmentStart;
        if (currentSplitIt == currentSplits.begin()) 
        {
            bestPossibleTime = sumOfBest;
        }
        else 
        {
            const auto lastSegmentTime = std::prev(currentSplitIt)->currentTime;
            const auto bestCurrentSegmentTime = std::max(currentSplitIt->pbSegmentTime, segmentTime);
            bestPossibleTime = std::accumulate(std::next(currentSplitIt), currentSplits.end(), lastSegmentTime + bestCurrentSegmentTime, [](int sum, const Split& s) { return sum + s.pbSegmentTime; });
        }
    }
    else
    {
        runTime = std::prev(currentSplitIt)->currentTime;
        segmentTime = std::prev(currentSplitIt)->currentTime - (currentSplits.size() > 1 ? std::prev(currentSplitIt, 2)->currentTime : 0);
        bestPossibleTime = runTime;
    }
    
    // Fixed: was firing for all trigger types; trigger-based splits must only complete on their packet callback, not here.
    if (currentSplitIt == currentSplits.end() || currentSplitIt->trigger != Trigger::None || !checkConditions(currentSplitIt->conditions, false)) return;
    completeSplit(currentSplitIt);
}

void GWSplits::resetRun()
{
    currentRun = nullptr;
    isCurrentRunTracked = false;

    segmentStart = 0;
    runTime = 0;           // Fixed: these were not reset, so stale values persisted into the next run
    segmentTime = 0;       // and the sumOfBest lazy-init guard (if sumOfBest == 0) skipped re-initialisation.
    bestPossibleTime = 0;
    sumOfBest = 0;
    lastSegmentColor = ImGui::ColorConvertFloat4ToU32(white);
    lastSegmentGain = 0;

    for (auto& run : runs) {
        run->startTime = {};
        for (auto& split : run->splits) {
            split.isPB = false;
            split.completed = false;
        }
    }

    refreshDisabledKeys();
}

int GWSplits::getRunTime()
{
    if (!currentRun)
    {
        return 0;
    }

    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(now() - currentRun->startTime).count();
}

void GWSplits::completeSplit(std::vector<Split>::iterator currentSplitIt)
{
    currentSplitIt->completed = true;

    currentSplitIt->currentTime = getRunTime();

    const auto finishedSegmentTime = currentSplitIt->currentTime - segmentStart;
    if (currentSplitIt->pbSegmentTime == 0 || finishedSegmentTime < currentSplitIt->pbSegmentTime) {
        currentSplitIt->isPB = true;
        currentSplitIt->pbSegmentTime = finishedSegmentTime;
        currentSplitIt->displayPBTime = timeToString(finishedSegmentTime);
    }

    const auto lastSegmentDiff = currentSplitIt == currentRun->splits.begin() ? 0 : std::prev(currentSplitIt)->currentTime - std::prev(currentSplitIt)->trackedTime;
    lastSegmentGain = currentSplitIt->currentTime - currentSplitIt->trackedTime - lastSegmentDiff;
    sumOfBest = std::accumulate(currentRun->splits.begin(), currentRun->splits.end(), 0, [](int sum, const Split& s) {
        return sum + s.pbSegmentTime;
    });

    segmentStart = currentSplitIt->currentTime;
    refreshDisabledKeys();
}

void GWSplits::refreshDisabledKeys()
{
    std::unordered_set<Hotkey> newDisabledKeys;

    if (const auto run = currentRun) 
    {
        if (!isCurrentRunTracked) 
        {
            disabledKeys = std::move(newDisabledKeys);
            return;
        }
        const auto activeSplit = std::ranges::find_if(run->splits, [](const Split& s){return !s.completed;});
        if (activeSplit != run->splits.end()) 
        {
            for (const auto& condition : activeSplit->conditions) 
            {
                if (!condition) continue;
                for (const auto& key : condition->disabledKeys())
                    newDisabledKeys.insert(key);
            }
        }
    }
    else 
    {
        for (const auto& r : runs) 
        {
            if (!r) continue;
            for (const auto& condition : r->trackConditions) 
            {
                if (!condition) continue;
                for (const auto& key : condition->disabledKeys())
                    newDisabledKeys.insert(key);
            }
        }
    }

    disabledKeys = std::move(newDisabledKeys);
}

void GWSplits::Draw(IDirect3DDevice9* pDevice)
{
    UNREFERENCED_PARAMETER(pDevice);
    if (!GetVisiblePtr() || !*GetVisiblePtr() || !currentRun)
        return;
    auto& currentSplits = currentRun->splits;
    auto currentSplitIt = std::ranges::find_if(currentSplits, [](const Split& split) { return !split.completed; });

    ImGui::SetNextWindowSize(ImVec2(100, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::PushFont(nullptr, static_cast<float>(FontLoader::font_sizes[fontSizeIndex]));
        if (settingsFolder && !isCurrentRunTracked)
        {
            if (std::ranges::any_of(currentSplits, &Split::isPB)) {
                if (ImGui::Button("Save new PBs", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    for (auto& split : currentSplits) split.isPB = false;
                    SaveSettings(settingsFolder);
                }
            }
            if (!currentSplits.empty() && std::ranges::all_of(currentSplits, &Split::completed) && (currentSplits[currentSplits.size() - 1].trackedTime == 0 || runTime < currentSplits[currentSplits.size() - 1].trackedTime)) {
                if (ImGui::Button("Use as reference & save", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    for (auto& split : currentSplits) 
                    {
                        split.isPB = false;
                        split.trackedTime = split.currentTime;
                        split.displayTrackedTime = timeToString(split.trackedTime);
                    }
                    SaveSettings(settingsFolder); 
                }
            }
        }

        if (!currentRun->topText.empty()) {
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(currentRun->topText.c_str()).x) * 0.5f);
            ImGui::TextUnformatted(currentRun->topText.c_str()); // Fixed: ImGui::Text with user-controlled string caused format-string UB on '%'.
            ImGui::Separator();
        }
        if (ImGui::BeginTable("tablename", 3, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Tracked", ImGuiTableColumnFlags_WidthFixed);

            int currentSplitIndex = currentSplitIt - currentSplits.begin();

            auto displaySplits = std::span(currentSplits);

            if (isCurrentRunTracked && totalSplits > 0 && totalSplits < (int) currentSplits.size()) {
                int total = (int) currentSplits.size();

                int end = std::min(currentSplitIndex + std::max(upcomingSplits, 0), total - 1);
                int upcomingCount = end - currentSplitIndex;
                int start = currentSplitIndex - (totalSplits - 1 - upcomingCount);

                displaySplits = displaySplits.subspan(std::max(start, 0), std::min(totalSplits, total));
            }

            // Fixed: was always 0, causing the first visible split's $duration to equal its absolute run time when the window was scrolled.
            int previousSplitTime = displaySplits.data() > currentSplits.data()
                ? std::prev(displaySplits.data())->currentTime
                : 0;

            for (const auto& split : displaySplits) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (split.completed) {
                    ImGui::PushID(&split);
                    ImGui::Selectable("##ping_split###ID", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Click to send to team chat");
                    }

                    if (ImGui::IsItemClicked()) {
                        std::unordered_map<std::string, std::string> variables = {
                            {"name", split.name},
                            {"time", timeToString(split.currentTime)},
                            {"duration", timeToString(split.currentTime - previousSplitTime)},
                            {"diff", timeToString(split.currentTime - split.trackedTime, ToStringStyle::SecondsCentiseconds)},
                            {"reference", timeToString(split.trackedTime)},
                            {"pb", timeToString(split.pbSegmentTime)},
                        };
                        auto result = replacePlaceholders(splitMessage, std::move(variables));
                        GW::Chat::SendChat('#', result.c_str());
                    }
                }

                // Fixed: dereferencing end() when all splits are complete was UB.
                if (currentSplitIt != currentSplits.end() && &split == &*currentSplitIt) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(currentSplitColor));
                }

                ImGui::TableSetColumnIndex(0);
                ImGui::SameLine();
                ImGui::TextUnformatted(split.name.c_str()); // Fixed: same format-string UB as topText.
                ImGui::TableNextColumn();

                if (split.completed) {
                    const auto timeDiff = split.currentTime - split.trackedTime;
                    ImGui::PushStyleColor(ImGuiCol_Text, getTimerColor(timeDiff, split.isPB));
                    ImGui::TextUnformatted(timeToString(timeDiff, ToStringStyle::SecondsCentiseconds).c_str());
                    ImGui::PopStyleColor();
                }
                else if (isCurrentRunTracked && currentSplitIt != currentSplits.end() && &split == &*currentSplitIt) { // Fixed: same end() dereference UB.
                    const auto currentTime = getRunTime();
                    const auto timeDiff = currentTime - split.trackedTime;
                    if (timeDiff > -earlyResultTimeMs) {
                        // There is a bug in the time returned by MapMgr, returning the time of the previous instance for the first few frames. 
                        // For old instances, this would always color the runtime as red for very behind without this check.
                        if (timeDiff < earlyResultTimeMs) 
                        {
                            lastSegmentColor = getTimerColor(timeDiff, false);
                        }
                        ImGui::PushStyleColor(ImGuiCol_Text, getTimerColor(timeDiff, split.isPB));
                        ImGui::TextUnformatted(timeToString(timeDiff, ToStringStyle::SecondsCentiseconds).c_str());
                        ImGui::PopStyleColor();
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(timeToString(split.trackedTime).c_str());

                if (split.completed) {
                    ImGui::PopID();
                }

                previousSplitTime = split.currentTime;
            }
            ImGui::EndTable();
        }

        if (showRunTime || showSegmentTime)
            ImGui::Separator();
        if (showRunTime)
        {
            ImGui::PushFont(nullptr, static_cast<float>(FontLoader::font_sizes[fontSizeIndex + 2]));
            ImGui::PushStyleColor(ImGuiCol_Text, lastSegmentColor);
            rightAlignedText(timeToString(runTime));
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        if (showSegmentTime)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(grey));
            rightAlignedText(timeToString(segmentTime));
            ImGui::PopStyleColor();
        }

        if (showBestPossibleTime || showSumOfBest || showLastSegment)
            ImGui::Separator();
        if (showBestPossibleTime)
        {
            
            ImGui::Text("Best possible time:");
            ImGui::SameLine();
            rightAlignedText(timeToString(bestPossibleTime));
        }
        if (showSumOfBest)
        {
            ImGui::Text("Sum of best segments:");
            ImGui::SameLine();
            rightAlignedText(timeToString(sumOfBest));
        }
        if (showLastSegment)
        {
            ImGui::Text("Last segment:");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, getTimerColor(lastSegmentGain, false));
            rightAlignedText(timeToString(lastSegmentGain, ToStringStyle::SecondsMilliseconds));
            ImGui::PopStyleColor();
        }
        ImGui::PopFont();
    }
    ImGui::End();
}

void GWSplits::DrawSettings()
{
    constexpr const char* fontSizeNames[] = {"Very small", "Small", "Medium", "Large"};

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    ImVec2 pos(0, 0);
    if (const auto window = ImGui::FindWindowByName(Name())) {
        pos = window->Pos;
    }

    if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
        ImGui::SetWindowPos(Name(), pos);
    }
    ImGui::Checkbox("Lock Position", &lock_move);
    ImGui::SameLine();
    ImGui::Checkbox("Lock Size", &lock_size);

    ImGui::Text("Display: ");
    ImGui::Indent();
    ImGui::PushItemWidth(150.f);
    ImGui::DragInt("Displayed splits", &totalSplits, 1, 0, 0);
    ImGui::SameLine();
    ImGui::ShowHelp("Set to 0 to show all splits");
    if (totalSplits > 0) 
    {
        ImGui::DragInt("Upcoming splits", &upcomingSplits, 1, 0, totalSplits);
    }

    ImGui::Combo("Text size", &fontSizeIndex, fontSizeNames, 4);

    ImGui::PopItemWidth();
    ImGui::InputText("Split message", &splitMessage);
    ImGui::SameLine();

    ImGui::ShowHelp("This message is shown in team chat when clicking on a completed split.\n"
                    "The following placeholders will be replaced:\n"
                    "\t$name: Name of the split\n"
                    "\t$time: Completion time of the split\n"
                    "\t$duration: Duration of the split\n"
                    "\t$diff: Difference between completion time and reference time\n"
                    "\t$reference: Time of the configured reference run\n"
                    "\t$pb: Personal best split duration"
    );

    ImGui::Unindent();

    ImGui::Text("Show: ");
    ImGui::Indent();
    ImGui::Checkbox("Run timer", &showRunTime);
    ImGui::SameLine();
    ImGui::Checkbox("Segment timer", &showSegmentTime);
    ImGui::SameLine();
    ImGui::Checkbox("Best possible time", &showBestPossibleTime);
    ImGui::SameLine();
    ImGui::Checkbox("Sum of best", &showSumOfBest);
    ImGui::SameLine();
    ImGui::Checkbox("Last segment", &showLastSegment);
    ImGui::Unindent();

    if (drawRuns()) 
    {
        refreshDisabledKeys();
    }
    if (ImGui::Button("Add Run", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0)))
    {
        runs.push_back(std::make_shared<Run>());
    }
    ImGui::SameLine();
    if (ImGui::Button("Import from clipboard", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (const auto clipboardContent = ImGui::GetClipboardText()) {
            if (const auto combined = decodeString(clipboardContent)) {
                InputStream stream{combined.value()};

                int version = 0;
                if (stream) stream >> version;
                if (version != currentVersion) return;
                if (!stream || stream.get() != 'R') return;

                if (auto importedRun = deserializeRun(stream)) 
                {
                    removePBs(*importedRun);
                    for (auto& split : importedRun->splits) {
                        split.displayPBTime = timeToString(split.pbSegmentTime);
                        split.displayTrackedTime = timeToString(split.trackedTime);
                    }
                    runs.push_back(std::make_shared<Run>(std::move(*importedRun)));
                }
            }
        }
    }

    ImGui::Text("Version 2.0.0");
}

void GWSplits::loadFromIniFile(const wchar_t* filePath)
{
    ini.LoadFile(filePath);
    runs.clear();
    [[maybe_unused]] const long savedVersion = ini.GetLongValue(Name(), "version", 11);
    totalSplits = ini.GetLongValue(Name(), "totalSplits", 0);
    upcomingSplits = ini.GetLongValue(Name(), "upcomingSplits", 0);
    showRunTime = ini.GetBoolValue(Name(), "showRunTime", true);
    showSegmentTime = ini.GetBoolValue(Name(), "showSegmentTime", true);
    showBestPossibleTime = ini.GetBoolValue(Name(), "showBestPossibleTime", true);
    showSumOfBest = ini.GetBoolValue(Name(), "showSumOfBest", true);
    showLastSegment = ini.GetBoolValue(Name(), "showLastSegment", true);
    fontSizeIndex = std::clamp((int)ini.GetLongValue(Name(), "fontSize", 2), 0, 3); // Fixed: unclamped value from a corrupt ini caused out-of-bounds access on font_sizes[fontSizeIndex + 2].
    splitMessage = ini.GetValue(Name(), "splitMessage", "[$name] ~ Time: $time ~ Duration: $duration");

    if (std::string read = ini.GetValue(Name(), "runs", ""); !read.empty()) {
        const auto decoded = decodeString(std::move(read));
        if (!decoded) return;
        InputStream stream(decoded.value());
        while (stream && stream.peek() == 'R') {
            stream.get();
            if (auto nextRun = deserializeRun(stream))
                runs.push_back(std::make_shared<Run>(std::move(*nextRun)));
            else
                break;
        }
    }

    for (auto& run : runs) {
        for (auto& split : run->splits) {
            split.displayPBTime = timeToString(split.pbSegmentTime);
            split.displayTrackedTime = timeToString(split.trackedTime);
        }
    }
}

void GWSplits::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    BackupManager::getInstance().initialize(folder);
    settingsFolder = folder;

    loadFromIniFile(GetSettingFile(folder).c_str());

    if (runs.empty() && BackupManager::getInstance().backupCount(PluginUtils::StringToWString(Name())) > 0) 
    {
        logMessage("No runs loaded, but automatic backups found. Automatic backups are stored in the settings folder.", Name());
    }
}

bool GWSplits::WndProc(const UINT Message, const WPARAM wParam, LPARAM lparam)
{
    if (GW::Chat::GetIsTyping() || GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) {
        return false;
    }
    Hotkey pressedKey{};
    switch (Message) {
        case WM_KEYDOWN:
            if (const auto isRepeated = (int)lparam & (1 << 30)) break;
            [[fallthrough]];
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            pressedKey.keyData = static_cast<int>(wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            if (LOWORD(wParam) & MK_MBUTTON) {
                pressedKey.keyData = VK_MBUTTON;
            }
            if (LOWORD(wParam) & MK_XBUTTON1) {
                pressedKey.keyData = VK_XBUTTON1;
            }
            if (LOWORD(wParam) & MK_XBUTTON2) {
                pressedKey.keyData = VK_XBUTTON2;
            }
            break;
        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
            // leave keydata to none, need to handle special case below
            break;
        case WM_MBUTTONDBLCLK:
            pressedKey.keyData = VK_MBUTTON;
            break;
        default:
            break;
    }

    if (!pressedKey.keyData) return false;

    switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: {
            if (GetKeyState(VK_CONTROL) < 0) {
                pressedKey.modifier |= ModKey_Control;
            }
            if (GetKeyState(VK_SHIFT) < 0) {
                pressedKey.modifier |= ModKey_Shift;
            }
            if (GetKeyState(VK_MENU) < 0) {
                pressedKey.modifier |= ModKey_Alt;
            }

            return disabledKeys.contains(pressedKey);
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:

        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
        default:
            return false;
    }
}

void GWSplits::SaveSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::SaveSettings(folder);
    ini.SetLongValue(Name(), "version", currentVersion);
    ini.SetLongValue(Name(), "totalSplits", totalSplits);
    ini.SetLongValue(Name(), "upcomingSplits", upcomingSplits);
    ini.SetBoolValue(Name(), "showRunTime", showRunTime);
    ini.SetBoolValue(Name(), "showSegmentTime", showSegmentTime);
    ini.SetBoolValue(Name(), "showBestPossibleTime", showBestPossibleTime);
    ini.SetBoolValue(Name(), "showSumOfBest", showSumOfBest);
    ini.SetBoolValue(Name(), "showLastSegment", showLastSegment);
    ini.SetLongValue(Name(), "fontSize", fontSizeIndex);
    ini.SetValue(Name(), "splitMessage", splitMessage.c_str());

    OutputStream stream;
    for (const auto& run : runs) {
        stream << serialize(*run);
    }

    if (const auto encoded = encodeString(stream.str())) {
        ini.SetValue(Name(), "runs", encoded->c_str());
    }
    
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
    if (runs.size())
        BackupManager::getInstance().save(PluginUtils::StringToWString(Name()), GetSettingFile(folder));
}

void GWSplits::handleTrigger(Trigger triggerType, std::function<bool(const Split&)> extraConditions)
{
    if (!currentRun) return;

    for (auto splitIt = currentRun->splits.begin(); splitIt != currentRun->splits.end(); ++splitIt) 
    {
        if (splitIt->completed) continue;
        if (splitIt->trigger != triggerType || !extraConditions(*splitIt)) return;

        completeSplit(splitIt);
        return; // Fixed: without this, the loop continued and chain-completed all subsequent trigger-type splits in a single packet callback.
    }
}

void GWSplits::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, fns, toolbox_dll);

    lastSegmentColor = ImGui::ColorConvertFloat4ToU32(white);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadStart>(&InstanceLoadStart_Entry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadStart*) -> void {
        if (!isCurrentRunTracked) {
            resetRun();
        }

        instanceStart = now();
        isCurrentRunTracked = false;
    });

    // Fixed: was &InstanceLoadStart_Entry — sharing one HookEntry across two registrations caused the second call to overwrite the first, silently dropping the InstanceLoadStart subscription.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceTimer>(&InstanceTimer_Entry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceTimer* packet) -> void {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        {
            // This is meant to correct slowloading but doesn't really work in outposts since the instance is active for as long as someone is in it
            // skipping time correction for outposts shouldn't be a huge issue since runs don't usually start when entering an outpost
            return;
        }

        const auto correctedInstanceStart = now() - std::chrono::milliseconds(packet->instance_time);

        if (currentRun && isCurrentRunTracked && currentRun->startTime == instanceStart) {
            currentRun->startTime = correctedInstanceStart;

            // TODO correct already completed splits - I'd expect this to be very rare so maybe not bother?
            // alternatively store completionTime for splits and calculate the rest on the fly
        }
        instanceStart = correctedInstanceStart;
    });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) {
        GW::UI::AsyncDecodeStr(packet->message, &onDisplayDialogDecoded, this);
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
        handleTrigger(Trigger::DungeonReward);
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(&DoACompleteZone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* packet) {
        const auto doaZone = (DoaZone)packet->message[1];
        handleTrigger(Trigger::DoaZoneComplete, [&](const Split& s){ return s.triggerData.doaZone == doaZone; });
    });

    /*GW::Chat::CreateCommand(L"restore", [](GW::HookStatus* status, const wchar_t*, const int argc, const LPWSTR* argv) {
        const auto instance = static_cast<GWSplits*>(ToolboxPluginInstance());
        if (!instance || argc < 2) {
            status->blocked = false;
            return;
        }
        const auto arg1 = PluginUtils::ToLower(argv[1]);
        const auto pluginName = PluginUtils::StringToWString(instance->Name());

        std::filesystem::path iniToLoad;
        if (arg1 != PluginUtils::ToLower(pluginName)) {
            status->blocked = false;
            return;
        }
        if (argc < 3 || PluginUtils::ToLower(argv[2]) == L"recent")
        {
            logMessage("Restore most recent backup", instance->Name());
            iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Latest);
        }
        else if (PluginUtils::ToLower(argv[2]) == L"largest")
        {
            logMessage("Restore largest backup", instance->Name());
            iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Largest);
        }
        else if (PluginUtils::ToLower(argv[2]) == L"list")
        {
            logMessage("Available backups:", instance->Name());
            const auto paths = BackupManager::getInstance().list(pluginName);
            for (const auto& path : paths) 
            {
                const auto name = path.filename().string().substr(0, 1);
                const auto time = std::format("{:%Y-%m-%d %H:%M}", std::filesystem::last_write_time(path));
                const auto size = std::filesystem::file_size(path);
                logMessage(std::format("Backup {}, Last change {}, File size {}", name, time, size), instance->Name());
            }
        }
        else if (PluginUtils::ToLower(argv[2]) == L"help")
        {
            logMessage("Type \"/restore " + std::string{instance->Name()} + " recent\" to restore the most recent backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " largest\" to restore the largest backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " list\" to show the available backups", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " $NUMBER\" to restore a specific backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " help\" to show this menu", instance->Name());
        }
        else {
            try 
            {
                const auto index = std::stoi(argv[2]);
                logMessage("Restore backup " + std::to_string(index), instance->Name());
                iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Index, index);
            }
            catch (...) {
                status->blocked = false;
                return;
            }
        }
        if (!iniToLoad.empty()) 
        {
            instance->loadFromIniFile(iniToLoad.c_str());
        }
    });*/

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"resetrun", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        const auto instance = static_cast<GWSplits*>(ToolboxPluginInstance());
        if (!instance) return;

        instance->resetRun();
        instanceStart = now();
    });

    InstanceInfo::getInstance().initialize();
    QuestInfo::getInstance().initialize();
    srand((unsigned int)time(NULL));
}

void GWSplits::SignalTerminate()
{
    ToolboxUIPlugin::SignalTerminate();

    // Fixed: was RemovePostCallback<InstanceLoadInfo> for both — wrong removal function and wrong packet type; callbacks were never removed.
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadStart>(&InstanceLoadStart_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceTimer>(&InstanceTimer_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);
    // Fixed: was RemovePostCallback — wrong table for callbacks registered with RegisterPacketCallback; callbacks were never removed.
    GW::StoC::RemoveCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DoACompleteZone>(&DoACompleteZone_Entry);
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry, L"resetrun");

    InstanceInfo::getInstance().terminate();
    QuestInfo::getInstance().terminate();
}
