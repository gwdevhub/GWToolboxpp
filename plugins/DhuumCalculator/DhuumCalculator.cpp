#include "DhuumCalculator.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Context/CharContext.h>

#include <numeric>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static DhuumCalculator instance;
    return &instance;
}

namespace
{
    struct DhuumStatus {
        float hp;
        float rest;
        std::chrono::steady_clock::time_point time;
    };
    std::deque<DhuumStatus> history;
    const GW::AgentLiving* dhuum = nullptr;

    const GW::AgentLiving* findDhuum() 
    {
        const auto agents = GW::Agents::GetAgentArray();
        if (!agents) return nullptr;
        for (const auto* agent : *agents) {
            if (!agent || !agent->GetIsLivingType()) 
                continue;
            const auto living = agent->GetAsAgentLiving();
            if (living->player_number == GW::Constants::ModelID::UW::Dhuum) 
                return living;
        }
        return nullptr;
    }

    int64_t diff(const std::chrono::steady_clock::time_point& a, const std::chrono::steady_clock::time_point& b) 
    {
        return int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(a - b).count());
    }

    std::deque<int64_t> damagePredictions;
    std::deque<int64_t> restPredictions;
    std::deque<int64_t> missingDamagePredictions;
    template<size_t smoothingLevel, typename T>
    T smooth(T value, std::deque<T>& queue) 
    {
        queue.push_back(value);
        if (queue.size() > smoothingLevel) queue.pop_front();
        return std::accumulate(queue.begin(), queue.end(), T{0}, std::plus<T>()) / queue.size();
    }

    std::string formatTime(int64_t timeInMs) 
    {
        const auto minutes = timeInMs / 60'000u;
        const auto seconds = (timeInMs % 60'000u) / 1'000u;
        return std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
    }
} // namespace

void DhuumCalculator::Update(float delay)
{
    ToolboxUIPlugin::Update(delay);

    dhuum = findDhuum();
    const auto context = GW::GetCharContext();
    if (!dhuum || !context || !context->progress_bar || context->progress_bar->progress < 0.01 || dhuum->max_hp < 80'000.f || dhuum->max_hp > 81'000.f) 
    {
        damagePredictions.clear();
        restPredictions.clear();
        history.clear();
        damageFinishPrediction = restFinishPrecition = 0;
        missingDamagePrediction = 0;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto instanceTime = GW::Map::GetInstanceTime();

    if (history.size() && diff(now, history.front().time) < 250) 
    {
        return;
    }

    history.push_back({dhuum->hp - 0.25f, 1.f - context->progress_bar->progress, now});
    while (history.size() && diff(now, history.front().time) > 48'000)
    {
        history.pop_front();
    }

    const auto dpms = (history.back().hp - history.front().hp) / diff(history.back().time, history.front().time);
    const auto rpms = (history.back().rest - history.front().rest) / diff(history.back().time, history.front().time);

    const auto msTillDamageDone = (int64_t)std::abs(history.back().hp / dpms);
    const auto msTillRestDone = (int64_t)std::abs(history.back().rest / rpms);
    const auto missingDamage = int64_t((0.75f * dhuum->max_hp) * (history.back().hp - std::abs(msTillRestDone * dpms)));

    damageFinishPrediction = smooth<16>(msTillDamageDone + instanceTime, damagePredictions);
    restFinishPrecition = smooth<16>(msTillRestDone + instanceTime, restPredictions);
    missingDamagePrediction = smooth<32>(missingDamage, missingDamagePredictions);
}

void DhuumCalculator::Draw(IDirect3DDevice9* pDevice)
{
    UNREFERENCED_PARAMETER(pDevice);
    if (!GetVisiblePtr() || !*GetVisiblePtr()) return;

    const auto context = GW::GetCharContext();
    if (!context || !context->progress_bar) return;
    
    ImGui::SetNextWindowSize(ImVec2(100, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) 
    {
        if (dhuum) 
        {
            if (dhuum->hp < 0.25f) {
                const auto missingRests = (int)std::ceil((1.f - context->progress_bar->progress) / 0.1f);
                ImGui::Text("Damage done. Missing rests: %i", missingRests);
            }
            else if (context->progress_bar->progress >= 0.999f) {
                const auto missingFuries = (int)std::ceil((dhuum->hp - 0.25f) * dhuum->max_hp / 250);
                ImGui::Text("Rest done. Missing furies: %i", missingFuries);
            }
            else {
                ImGui::Text("Rest finish time: %s", formatTime(restFinishPrecition).c_str());
                ImGui::Text("Damage finish time: %s", formatTime(damageFinishPrediction).c_str());
                if (missingDamagePrediction > 0) {
                    ImGui::Text("Missing furies: %i", missingDamagePrediction / 250);
                }
                else {
                    ImGui::Text("Overkilling by: %ik", -missingDamagePrediction / 1000);
                }
            }
        }
        else {
            ImGui::Text("Dhuum not found");
        }
    }
    ImGui::End();
}

void DhuumCalculator::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();

    ImGui::Text("Version 1.0.3");
}
