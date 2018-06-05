#include "ObjectiveTimerWindow.h"

#include <GWCA\Constants\QuestIDs.h>
#include <GWCA\Constants\Constants.h>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include "GuiUtils.h"
#include "GWToolbox.h"

#include <Modules\Resources.h>

#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

namespace {
    enum DoA_ObjId : DWORD {
        Foundry = 0x273F,
        Veil,
        Gloom,
        City
    };
    uint32_t doa_get_next(uint32_t id) {
        switch (id) {
        case Foundry: return City;
        case City: return Veil;
        case Veil: return Gloom;
        case Gloom: return Foundry;
        }
        return 0;
    }
}

void ObjectiveTimerWindow::Initialize() {
	ToolboxWindow::Initialize();

    // when we are entering an explorable (doa/uw/fow), create a new objective set
    // in case of doa, also detect where we are starting
    GW::StoC::AddCallback<GW::Packet::StoC::InstanceLoadFile>(
        [this](GW::Packet::StoC::InstanceLoadFile *packet) -> bool {
        switch (packet->map_fileID) {
        case 219215: AddDoAObjectiveSet(packet->spawn_point); break;
        case 63058: AddFoWObjectiveSet(); break;
        case 63059: AddUWObjectiveSet(); break;
        default:
            break;
        }
        return false;
    });

	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveAdd>(
	[this](GW::Packet::StoC::ObjectiveAdd* packet) -> bool {
		Objective *obj = GetCurrentObjective(packet->objective_id);
        if (obj) obj->SetStarted();
        return false;
	});
	
	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveDone>(
	[this](GW::Packet::StoC::ObjectiveDone* packet) -> bool {
		Objective *obj = GetCurrentObjective(packet->objective_id);
        if (obj) obj->SetDone();
        return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::DoACompleteZone>(
	[this](GW::Packet::StoC::DoACompleteZone* packet) -> bool {
		if (packet->message[0] != 0x8101) return false;

		uint32_t id = packet->message[1];
		Objective *obj = GetCurrentObjective(id);
        if (obj) obj->SetDone();
		uint32_t next_id = doa_get_next(id);
		Objective *next = GetCurrentObjective(next_id);
        if (next && !next->IsStarted()) obj->SetStarted();
		return false;
	});
}

void ObjectiveTimerWindow::AddDoAObjectiveSet(GW::Vector2f spawn) {
    printf("%f, %f\n", spawn.x, spawn.y);
    static const GW::Vector2f area_spawns[] = {
        { -10514, 15231 },  // foundry
        { -18575, -8833 },  // city
        { 364, -10445 },    // veil
        { 16034, 1244 },    // gloom
    };
    const GW::Vector2f mallyx_spawn(-3931, -6214);

    const int n_areas = 4;
    double best_dist = spawn.DistanceTo(mallyx_spawn);
    int area = -1;
    for (int i = 0; i < n_areas; ++i) {
        float dist = spawn.DistanceTo(area_spawns[i]);
        if (best_dist > dist) {
            best_dist = dist;
            area = i;
        }
    }
    if (area == -1) return; // we're doing mallyx, not doa!

    Objective* objs[n_areas];
    objs[0] = new Objective(0x273F, "Foundry");
    objs[1] = new Objective(0x2742, "City");
    objs[2] = new Objective(0x2740, "Veil");
    objs[3] = new Objective(0x2741, "Gloom");

    ObjectiveSet* os = new ObjectiveSet("Domain of Anguish");
    for (int i = 0; i < n_areas; ++i) {
        os->objectives.push_back(objs[(area + i) % n_areas]);
    }
    os->objectives.front()->SetStarted(0);
    objective_sets.push_back(os);
}
void ObjectiveTimerWindow::AddFoWObjectiveSet() {
    ObjectiveSet* os = new ObjectiveSet("Fissure of Woe");
    os->objectives.push_back(new Objective(309, "ToC"));
    os->objectives.push_back(new Objective(310, "Wailing Lord"));
    os->objectives.push_back(new Objective(311, "Griffons"));
    os->objectives.push_back(new Objective(312, "Defend"));
    os->objectives.push_back(new Objective(313, "Camp"));
    os->objectives.push_back(new Objective(314, "Menzies"));
    os->objectives.push_back(new Objective(315, "Restore"));
    os->objectives.push_back(new Objective(316, "Khobay"));
    os->objectives.push_back(new Objective(317, "ToS"));
    os->objectives.push_back(new Objective(318, "Burning Forest"));
    os->objectives.push_back(new Objective(319, "The Hunt"));
    objective_sets.push_back(os);
}
void ObjectiveTimerWindow::AddUWObjectiveSet() {
    ObjectiveSet* os = new ObjectiveSet("The Underworld");
    os->objectives.push_back(new Objective(146, "Chamber"));
    os->objectives.push_back(new Objective(147, "Restore"));
    os->objectives.push_back(new Objective(148, "Escort"));
    os->objectives.push_back(new Objective(149, "UWG"));
    os->objectives.push_back(new Objective(150, "Vale"));
    os->objectives.push_back(new Objective(151, "Waste"));
    os->objectives.push_back(new Objective(152, "Pits"));
    os->objectives.push_back(new Objective(153, "Planes"));
    os->objectives.push_back(new Objective(154, "Mnts"));
    os->objectives.push_back(new Objective(155, "Pools"));
    os->objectives.push_back(new Objective(157, "Dhuum"));
    objective_sets.push_back(os);
}

void ObjectiveTimerWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

        if (objective_sets.empty()) {
            ImGui::Text("Enter DoA, FoW, or UW to begin");
        } else {
            for (ObjectiveSet* os : objective_sets) {
                os->Draw();
            }
        }
	}
	ImGui::End();
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::GetCurrentObjective(uint32_t obj_id) {
    if (objective_sets.empty()) return nullptr;

    for (Objective* objective : objective_sets.back()->objectives) {
        if (objective->id == obj_id) {
            return objective;
        }
    }
    return nullptr;

	//Objective *objectives;
	//uint32_t id;
	//if (309 <= obj_id && obj_id <= 319) {
	//	// fow
	//	id = obj_id - 309;
	//	if (id >= countof(obj_fow))
	//		return nullptr;
	//	objectives = obj_fow;
	//} else if (146 <= obj_id && obj_id <= 157) {
	//	// uw
	//	id = obj_id - 146;
	//	if (id >= countof(obj_uw))
	//		return nullptr;
	//	objectives = obj_uw;
	//} else if (0x273F <= obj_id && obj_id <= 0x2742) {
	//	id = obj_id - 0x273F;
	//	if (id >= countof(obj_doa))
	//		return nullptr;
	//	objectives = obj_doa;
	//} else {
	//	return nullptr;
	//}
	//Objective *obj = objectives + id;
	//if (obj_id != obj->id) {
	//	fprintf(stderr, "Objective id mismatch (expected: %lu, received: %lu)\n", obj->id, obj_id);
	//	return nullptr;
	//}
	//return obj;
}

void ObjectiveTimerWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
}

void ObjectiveTimerWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
}

ObjectiveTimerWindow::Objective::Objective(uint32_t _id, const char* _name) {
    id = _id;
    name = _name;
    start = -1;
    done = -1;
    duration = -1;
    strncpy(cached_done, "--:--", sizeof(cached_done));
    strncpy(cached_start, "--:--", sizeof(cached_start));
    strncpy(cached_duration, "--:--", sizeof(cached_duration));
}
void ObjectiveTimerWindow::Objective::SetStarted(DWORD start_time) {
    if (start_time == -1) {
        start = GW::Map::GetInstanceTime();
    } else {
        start = start_time;
    }
    PrintTime(cached_start, sizeof(cached_start), start);
}
void ObjectiveTimerWindow::Objective::SetDone() {
    done = GW::Map::GetInstanceTime();
    PrintTime(cached_done, sizeof(cached_done), done);
    duration = done - start;
    PrintTime(cached_duration, sizeof(cached_duration), duration);
}
void ObjectiveTimerWindow::Objective::PrintTime(char* buf, size_t size, DWORD time) {
    DWORD sec = time / 1000;
    snprintf(buf, size, "%02d:%02d.%1d",
        (sec / 60), sec % 60, (time / 100) % 10);
}

namespace {
    float GetGridItemX(int i) {
        const int n_columns = 4;
        const auto& style = ImGui::GetStyle();
        return style.WindowPadding.x + (i * ImGui::GetWindowContentRegionWidth() / n_columns);
    }
}
void ObjectiveTimerWindow::ObjectiveSet::Draw() {
    if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetCursorPosX(GetGridItemX(1));
        ImGui::Text("Started");
        ImGui::SameLine(GetGridItemX(2));
        ImGui::Text("Completed");
        ImGui::SameLine(GetGridItemX(3));
        ImGui::Text("Duration");

        for (Objective* objective : objectives) {
            objective->Draw();
        }
    }
}

namespace {
    float GetGridItemWidth() {
        const int n_columns = 4;
        return (ImGui::GetWindowContentRegionWidth()
            - (ImGui::GetStyle().ItemInnerSpacing.x * (n_columns - 1))) / n_columns;
    }
}
void ObjectiveTimerWindow::Objective::Draw() {
    if (ImGui::Button(name, ImVec2(GetGridItemWidth(), 0))) {
        char buf[256];
        snprintf(buf, 256, "[%s] ~ Started: %s ~ Completed: %s ~ Duration: %s", 
            name, cached_start, cached_done, cached_duration);
        GW::Chat::SendChat('#', buf);
    }
    ImGui::PushItemWidth(GetGridItemWidth());
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::InputText("##start", cached_start, sizeof(cached_start), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::InputText("##done", cached_done, sizeof(cached_done), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    if (start == -1) {
        char buf[16];
        strncpy(buf, "--:--", sizeof(buf));
        ImGui::InputText("##duration", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    } else if (duration == -1) {
        char buf[16];
        PrintTime(buf, sizeof(buf), GW::Map::GetInstanceTime() - start);
        ImGui::InputText("##duration", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    } else {
        ImGui::InputText("##duration", cached_duration, sizeof(cached_duration), ImGuiInputTextFlags_ReadOnly);
    }
}
