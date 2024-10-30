#include <GWSplits.h>

#include <ConditionIO.h>
#include <InstanceInfo.h>
#include <enumUtils.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

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
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry DungeonReward_Entry;
    GW::HookEntry DoACompleteZone_Entry;

    static void onDisplayDialogDecoded(void* instancePtr, const wchar_t* decoded)
    {
        if (!decoded || !instancePtr) return;
        const auto gwsplits = reinterpret_cast<GWSplits*>(instancePtr);

        gwsplits->handleTrigger(Trigger::DisplayDialog, [&](const Split& s){ return !s.triggerData.message.empty() && WStringToString(decoded).contains(s.triggerData.message); });
    }

    constexpr int earlyResultTimeMs = 10'000;
    constexpr long currentVersion = 11;
    constexpr auto grey = ImVec4{0.7f, 0.7f, 0.7f, 1.f};
    constexpr auto gold = ImVec4{0.95686f, 0.7882f, 0.f, 1.f};
    constexpr auto white = ImVec4{1.f, 1.f, 1.f, 1.f};
    const wchar_t* settingsFolder = nullptr; // Folder loaded from. Used for PB saving button
    int getRunTime() 
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return 0;
        return (int)GW::Map::GetInstanceTime();
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

    void drawConditionSetSelector(std::vector<ConditionPtr>& conditions)
    {
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
            (*it)->drawSettings();
            ImGui::PopID();
        }
        if (conditionToDelete.has_value()) conditions.erase(conditionToDelete.value());
        if (conditionsToSwap.has_value()) std::swap(*conditionsToSwap->first, *conditionsToSwap->second);
        // Add condition
        if (auto newCondition = drawConditionSelector(200.f)) 
        {
            conditions.push_back(std::move(newCondition));
        }
    }
    void drawSplits(std::vector<Split>& splits)
    {
        using SplitIt = decltype(splits.begin());
        std::optional<SplitIt> splitToDelete = std::nullopt;
        std::optional<std::pair<SplitIt, SplitIt>> splitsToSwap = std::nullopt;

        for (auto splitIt = splits.begin(); splitIt < splits.end(); ++splitIt) {
            ImGui::PushID(splitIt - splits.begin());
            const auto treeHeader = splitIt->name + "###0";
            const auto treeOpen = ImGui::TreeNodeEx(treeHeader.c_str(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap);

            const auto offset = 126.f + (treeOpen ? 0.f : 21.f);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - offset);
            ImGui::PushItemWidth(70.f);
            if (ImGui::InputText("", &splitIt->displayTrackedTime)) {
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
                drawConditionSetSelector(splitIt->conditions);
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
                }
                ImGui::PopItemWidth();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (splitToDelete.has_value()) splits.erase(splitToDelete.value());
        if (splitsToSwap.has_value()) std::swap(*splitsToSwap->first, *splitsToSwap->second);
    }
    void rightAlignedText(const std::string& s)
    {
        auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(s.c_str()).x - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (posX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(posX);
        ImGui::Text(s.c_str());
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
        const auto result = ImGui::ColorConvertU32ToFloat4(mix(lightRed, red, 1.f - redNess));

        return mix(lightRed, red, 1.f - redNess);
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
} // namespace

void GWSplits::drawRuns()
{
    using RunIt = decltype(runs.begin());
    std::optional<RunIt> runToDelete = std::nullopt;
    std::optional<std::pair<RunIt, RunIt>> runsToSwap = std::nullopt;

    for (auto runIt = runs.begin(); runIt < runs.end(); ++runIt) {
        ImGui::PushID(runIt - runs.begin());
        ImGui::PushStyleColor(ImGuiCol_Header, {100.f / 255, 100.f / 255, 100.f / 255, 0.5});

        const auto header = (*runIt)->name + "###0";
        const auto treeOpen = ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - (treeOpen ? 127.f : 148.f));
        if (currentRun && currentRun == *runIt) 
        {
            if (ImGui::Button("Untrack", ImVec2(60, 0))) { currentRun = nullptr; *GetVisiblePtr() = false; }
        }
        else 
        {
            if (ImGui::Button("Track", ImVec2(60, 0))) { currentRun = *runIt; *GetVisiblePtr() = true; }
        }
        
        ImGui::SameLine();
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
            drawSplits((*runIt)->splits);
            if (ImGui::Button("Add split", ImVec2(100, 0))) 
            {
                (*runIt)->splits.push_back({});
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy run", ImVec2(100, 0))) 
            {
                if (const auto encoded = encodeString(std::to_string(currentVersion) + " " + serialize(**runIt))) {
                    logMessage("Copy run " + (*runIt)->name + " to clipboard");
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
}

void GWSplits::Update(float diff)
{
    ToolboxUIPlugin::Update(diff);

    if (!currentRun || currentRun->splits.empty() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
        return;
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
            const auto bestCurrentSegmentTime = std::max(currentSplitIt->pbSegmentTime, currentSplitIt->currentTime - lastSegmentTime);
            bestPossibleTime = std::accumulate(std::next(currentSplitIt), currentSplits.end(), lastSegmentTime + bestCurrentSegmentTime, [](int sum, const Split& s) { return sum + s.pbSegmentTime; });
        }
    }
    else 
    {
        runTime = std::prev(currentSplitIt)->currentTime;
        segmentTime = std::prev(currentSplitIt)->currentTime - (currentSplits.size() > 1 ? std::prev(currentSplitIt, 2)->currentTime : 0);
        bestPossibleTime = runTime;
    }

    if (currentSplitIt == currentSplits.end() || !checkConditions(currentSplitIt->conditions, false)) return;
    completeSplit(currentSplitIt);
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
        ImGui::PushFont(GetFont(fontSize));
        if (settingsFolder && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) 
        {
            if (std::ranges::any_of(currentSplits, &Split::isPB)) {
                if (ImGui::Button("Save new PBs", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    for (auto& split : currentSplits) split.isPB = false;
                    SaveSettings(settingsFolder);
                }
            }
            if (!currentSplits.empty() && std::ranges::all_of(currentSplits, &Split::completed) && runTime < currentSplits[currentSplits.size() - 1].trackedTime) {
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
            ImGui::Text(currentRun->topText.c_str());
            ImGui::Separator();
        }
        if (ImGui::BeginTable("tablename", 3, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Tracked", ImGuiTableColumnFlags_WidthFixed);

            int row = 0;

            for (const auto& split : currentSplits) {
                ImGui::TableNextRow();
                if (row == (currentSplitIt - currentSplits.begin())) 
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4{0.3725f, 0.3961f, 0.7529f, 0.6f}));
                ImGui::TableNextColumn();
                ImGui::Text(split.name.c_str());
                ImGui::TableNextColumn();

                if (split.completed) {
                    const auto timeDiff = split.currentTime - split.trackedTime;
                    ImGui::PushStyleColor(ImGuiCol_Text, getTimerColor(timeDiff, split.isPB));
                    ImGui::Text(timeToString(timeDiff, ToStringStyle::SecondsCentiseconds).c_str());
                    ImGui::PopStyleColor();
                }
                else if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && row == (currentSplitIt - currentSplits.begin())) {
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
                        ImGui::Text(timeToString(timeDiff, ToStringStyle::SecondsCentiseconds).c_str());
                        ImGui::PopStyleColor();
                    }
                }

                ImGui::TableNextColumn();
                ImGui::Text(timeToString(split.trackedTime).c_str());

                ++row;
            }
            ImGui::EndTable();
        }

        if (showRunTime || showSegmentTime)
            ImGui::Separator();
        if (showRunTime)
        {
            ImGui::PushFont(GetFont(GuiUtils::FontSize(int(fontSize) + 2)));
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

    ImGui::Checkbox("Lock Position", &lock_move);
    ImGui::SameLine();
    ImGui::Checkbox("Lock Size", &lock_size);
    ImGui::SameLine();
    ImGui::PushItemWidth(150.f);
    ImGui::Combo("Text size", reinterpret_cast<int*>(&fontSize), fontSizeNames, 4);
    ImGui::PopItemWidth();

    ImGui::Text("Show: ");
    ImGui::SameLine();
    ImGui::Checkbox("Run timer", &showRunTime);
    ImGui::SameLine();
    ImGui::Checkbox("Segment timer", &showSegmentTime);
    ImGui::SameLine();
    ImGui::Checkbox("Best possible time", &showBestPossibleTime);
    ImGui::SameLine();
    ImGui::Checkbox("Sum of best", &showSumOfBest);
    ImGui::SameLine();
    ImGui::Checkbox("Last segment", &showLastSegment);

    drawRuns();
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
                    runs.push_back(std::make_shared<Run>(std::move(*importedRun)));
                }
            }
        }
    }

    ImGui::Text("Version 1.1. For new releases, feature requests and bug reports check out");
    ImGui::SameLine();
    constexpr auto discordInviteLink = "https://discord.gg/ZpKzer4dK9";
    ImGui::TextColored(ImColor{102, 187, 238, 255}, discordInviteLink);
    if (ImGui::IsItemClicked()) {
        ShellExecute(nullptr, "open", discordInviteLink, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void GWSplits::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    settingsFolder = folder;

    [[maybe_unused]] const long savedVersion = ini.GetLongValue(Name(), "version", 11);
    showRunTime = ini.GetBoolValue(Name(), "showRunTime", true);
    showSegmentTime = ini.GetBoolValue(Name(), "showSegmentTime", true);
    showBestPossibleTime = ini.GetBoolValue(Name(), "showBestPossibleTime", true);
    showSumOfBest = ini.GetBoolValue(Name(), "showSumOfBest", true);
    showLastSegment = ini.GetBoolValue(Name(), "showLastSegment", true);
    fontSize = (GuiUtils::FontSize)ini.GetLongValue(Name(), "fontSize", 2);

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

    for (auto& run : runs) 
    {
        for (auto& split : run->splits) 
        {
            split.displayPBTime = timeToString(split.pbSegmentTime);
            split.displayTrackedTime = timeToString(split.trackedTime);
        }
    }
}

void GWSplits::SaveSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::SaveSettings(folder);
    ini.SetLongValue(Name(), "version", currentVersion);
    ini.SetBoolValue(Name(), "showRunTime", showRunTime);
    ini.SetBoolValue(Name(), "showSegmentTime", showSegmentTime);
    ini.SetBoolValue(Name(), "showBestPossibleTime", showBestPossibleTime);
    ini.SetBoolValue(Name(), "showSumOfBest", showSumOfBest);
    ini.SetBoolValue(Name(), "showLastSegment", showLastSegment);
    ini.SetLongValue(Name(), "fontSize", (long)fontSize);

    OutputStream stream;
    for (const auto& run : runs) {
        stream << serialize(*run);
    }

    if (const auto encoded = encodeString(stream.str())) {
        ini.SetValue(Name(), "runs", encoded->c_str());
    }
    
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

void GWSplits::handleTrigger(Trigger triggerType, std::function<bool(const Split&)> extraConditions)
{
    if (!currentRun) return;

    for (auto splitIt = currentRun->splits.begin(); splitIt != currentRun->splits.end(); ++splitIt) 
    {
        if (splitIt->completed) continue;
        if (splitIt->trigger != triggerType || !extraConditions(*splitIt)) return;

        completeSplit(splitIt);
    }
}

void GWSplits::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, fns, toolbox_dll);

    lastSegmentColor = ImGui::ColorConvertFloat4ToU32(white);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Entry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadInfo* pak) -> void 
    {
        if (!pak->is_explorable) return;
        
        segmentStart = 0;
        lastSegmentColor = ImGui::ColorConvertFloat4ToU32(white);
        lastSegmentGain = 0;
        
        for (auto& run : runs) 
        {
            for (auto& split : run->splits) 
            {
                split.isPB = false;
                split.completed = false;
                split.currentTime = 0;
            }
        }
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) {
        GW::UI::AsyncDecodeStr(packet->message, &onDisplayDialogDecoded, this);
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
        handleTrigger(Trigger::DungeonReward);
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(&DoACompleteZone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* packet) {
        const auto doaZone = (DoaZone)packet->message[1];
        handleTrigger(Trigger::DungeonReward, [&](const Split& s){ return s.triggerData.doaZone == doaZone; });
    });

    InstanceInfo::getInstance().initialize();
    srand((unsigned int)time(NULL));
}

void GWSplits::SignalTerminate()
{
    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::DoACompleteZone>(&DoACompleteZone_Entry);
    ToolboxUIPlugin::SignalTerminate();
}
