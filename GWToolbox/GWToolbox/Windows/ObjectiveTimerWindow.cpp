#include "ObjectiveTimerWindow.h"

#include <imgui_internal.h>

#include <GWCA\Constants\QuestIDs.h>
#include <GWCA\Constants\Constants.h>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\UIMgr.h>

#include "GuiUtils.h"
#include "GWToolbox.h"

#include <Modules\Resources.h>

#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

#define TIME_UNKNOWN -1
unsigned int ObjectiveTimerWindow::ObjectiveSet::cur_ui_id = 0;

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

    void PrintTime(char* buf, size_t size, DWORD time, bool show_ms = true) {
        if (time == TIME_UNKNOWN) {
            strncpy(buf, "--:--", size);
        } else {
            DWORD sec = time / 1000;
            if (show_ms) {
                snprintf(buf, size, "%02d:%02d.%1d",
                    (sec / 60), sec % 60, (time / 100) % 10);
            } else {
                snprintf(buf, size, "%02d:%02d", (sec / 60), sec % 60);
            }
        }
    }

	void AsyncGetMapName(char *buffer, size_t n) {
		static wchar_t enc_str[16];
		GW::AreaInfo& info = GW::Map::GetCurrentMapInfo();
		if (!GW::UI::UInt32ToEncStr(info.NameID, enc_str, n)) {
			buffer[0] = 0;
			return;
		}
		GW::UI::AsyncDecodeStr(enc_str, buffer, n);
	}

    float GetGridItemWidth() {
        const int n_columns = 4;
        return (ImGui::GetWindowContentRegionWidth()
            - (ImGui::GetStyle().ItemInnerSpacing.x * (n_columns - 1))) / n_columns;
    }

    float GetGridItemX(int i) {
        const int n_columns = 4;
        const auto& style = ImGui::GetStyle();
        return style.WindowPadding.x + (i * ImGui::GetWindowContentRegionWidth() / n_columns);
    }
}

void ObjectiveTimerWindow::Initialize() {
	ToolboxWindow::Initialize();

	GW::StoC::AddCallback<GW::Packet::StoC::PartyDefeated>(
	[this](GW::Packet::StoC::PartyDefeated *packet) -> bool {
		if (!objective_sets.empty()) {
			ObjectiveSet *os = objective_sets.back();
			os->StopObjectives();
		}
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::GameSrvTransfer>(
	[this](GW::Packet::StoC::GameSrvTransfer *packet) -> bool {
		if (!objective_sets.empty()) {
			ObjectiveSet *os = objective_sets.back();
			os->StopObjectives();
		}
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::InstanceLoadFile>(
	[this](GW::Packet::StoC::InstanceLoadFile *packet) -> bool {
		// We would want to have a default type that can handle objective by using name in Guild Wars
		// The only thing we miss is how to determine wether this map has a mission objectives.
		// We could use packet 187, but this can be a little bit hairy to do. Ask Ziox for more info.
		switch (packet->map_fileID) {
		case 219215: AddDoAObjectiveSet(packet->spawn_point); break;
		case 63058:  AddFoWObjectiveSet(); break;
        case 63059:  AddUWObjectiveSet(); break;
        default: 
            if (!objective_sets.empty()) {
                objective_sets.back()->active = false;
            }
		}
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveAdd>(
	[this](GW::Packet::StoC::ObjectiveAdd *packet) -> bool {
		// type 12 is the "title" of the mission objective, should we ignore it or have a "title" objective ?
		Objective *obj = GetCurrentObjective(packet->objective_id);
		if (obj) return false;
		ObjectiveSet *os = objective_sets.back();
		os->objectives.emplace_back(packet->objective_id);
		obj = &os->objectives.back();
		GW::UI::AsyncDecodeStr(packet->name, obj->name, sizeof(obj->name));
		// If the name isn't "???" we consider that the objective started
		if (wcsncmp(packet->name, L"\x8102\x3236", 2))
			obj->SetStarted();
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveUpdateName>(
	[this](GW::Packet::StoC::ObjectiveUpdateName* packet) -> bool {
		Objective *obj = GetCurrentObjective(packet->objective_id);
        if (obj) obj->SetStarted();
        return false;
	});
	
	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveDone>(
	[this](GW::Packet::StoC::ObjectiveDone* packet) -> bool {
		Objective *obj = GetCurrentObjective(packet->objective_id);
        if (obj) {
            obj->SetDone();
            objective_sets.back()->CheckSetDone();
        }
        return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::DoACompleteZone>(
	[this](GW::Packet::StoC::DoACompleteZone* packet) -> bool {
		if (packet->message[0] != 0x8101) return false;

		uint32_t id = packet->message[1];
		Objective *obj = GetCurrentObjective(id);
		ObjectiveSet *os = objective_sets.back();

        if (obj) {
            obj->SetDone();
            os->CheckSetDone();
            uint32_t next_id = doa_get_next(id);
            Objective *next = GetCurrentObjective(next_id);
            if (next && !next->IsStarted()) next->SetStarted();
        }
		return false;
	});
}

void ObjectiveTimerWindow::ObjectiveSet::StopObjectives() {
	for (Objective& obj : objectives) {
		if (obj.done == -1)
			obj.is_open = false;
	}
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

	ObjectiveSet *os = new ObjectiveSet;
	::AsyncGetMapName(os->name, sizeof(os->name));
    Objective objs[n_areas] = {{Foundry, "Foundry"}, {City, "City"}, {Veil, "Veil"}, {Gloom, "Gloom"}};

    for (int i = 0; i < n_areas; ++i) {
        os->objectives.push_back(objs[(area + i) % n_areas]);
    }

    os->objectives.front().SetStarted();
    objective_sets.push_back(os);
}
void ObjectiveTimerWindow::AddFoWObjectiveSet() {
	ObjectiveSet *os = new ObjectiveSet;
	::AsyncGetMapName(os->name, sizeof(os->name));
	os->objectives.emplace_back(309, "ToC");
	os->objectives.emplace_back(310, "Wailing Lord");
	os->objectives.emplace_back(311, "Griffons");
	os->objectives.emplace_back(312, "Defend");
	os->objectives.emplace_back(313, "Camp");
	os->objectives.emplace_back(314, "Menzies");
	os->objectives.emplace_back(315, "Restore");
	os->objectives.emplace_back(316, "Khobay");
	os->objectives.emplace_back(317, "ToS");
	os->objectives.emplace_back(318, "Burning Forest");
	os->objectives.emplace_back(319, "The Hunt");
	objective_sets.push_back(os);
}
void ObjectiveTimerWindow::AddUWObjectiveSet() {
	ObjectiveSet *os = new ObjectiveSet;
	::AsyncGetMapName(os->name, sizeof(os->name));
	os->objectives.emplace_back(146, "Chamber");
	os->objectives.emplace_back(147, "Restore");
	os->objectives.emplace_back(148, "Escort");
	os->objectives.emplace_back(149, "UWG");
	os->objectives.emplace_back(150, "Vale");
	os->objectives.emplace_back(151, "Waste");
	os->objectives.emplace_back(152, "Pits");
	os->objectives.emplace_back(153, "Planes");
	os->objectives.emplace_back(154, "Mnts");
	os->objectives.emplace_back(155, "Pools");
	os->objectives.emplace_back(157, "Dhuum");
	objective_sets.push_back(os);
}

void ObjectiveTimerWindow::Update(float delta) {
    if (!objective_sets.empty() && objective_sets.back()->active) {
        objective_sets.back()->Update();
    }
}
void ObjectiveTimerWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

        if (objective_sets.empty()) {
            ImGui::Text("Enter DoA, FoW, or UW to begin");
        } else {
            for (auto& it = objective_sets.rbegin(); it != objective_sets.rend(); it++) {
                (*it)->Draw();
            }
        }
	}
	ImGui::End();
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::GetCurrentObjective(uint32_t obj_id) {
    if (objective_sets.empty()) return nullptr;
    if (!objective_sets.back()->active) return nullptr;

    for (Objective& objective : objective_sets.back()->objectives) {
        if (objective.id == obj_id) {
            return &objective;
        }
    }
    return nullptr;
}

void ObjectiveTimerWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
}

void ObjectiveTimerWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
}


ObjectiveTimerWindow::Objective::Objective(uint32_t _id, const char* _name) {
    id = _id;
    strncpy(name, _name, sizeof(name));
    start = TIME_UNKNOWN;
    done = TIME_UNKNOWN;
    duration = TIME_UNKNOWN;
    PrintTime(cached_done, sizeof(cached_done), TIME_UNKNOWN);
    PrintTime(cached_start, sizeof(cached_start), TIME_UNKNOWN);
    PrintTime(cached_duration, sizeof(cached_duration), TIME_UNKNOWN);
	is_open = false;
}

bool ObjectiveTimerWindow::Objective::IsStarted() const { 
    return start != TIME_UNKNOWN;
}
bool ObjectiveTimerWindow::Objective::IsDone() const { 
    return done != TIME_UNKNOWN;
}
void ObjectiveTimerWindow::Objective::SetStarted() {
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        start = 0; // still loading, just set to 0
    } else {
        start = GW::Map::GetInstanceTime();
    }
	is_open = true;
    PrintTime(cached_start, sizeof(cached_start), start);
}
void ObjectiveTimerWindow::Objective::SetDone() {
    if (start == TIME_UNKNOWN) SetStarted(); // something went wrong
    done = GW::Map::GetInstanceTime();
    PrintTime(cached_done, sizeof(cached_done), done);
    duration = done - start;
    PrintTime(cached_duration, sizeof(cached_duration), duration);
}

void ObjectiveTimerWindow::Objective::Update() {
    if (start == TIME_UNKNOWN) {
        PrintTime(cached_duration, sizeof(cached_duration), TIME_UNKNOWN);
    } else if (done == TIME_UNKNOWN && is_open) {
        PrintTime(cached_duration, sizeof(cached_duration), GW::Map::GetInstanceTime() - start);
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
    ImGui::InputText("##duration", cached_duration, sizeof(cached_duration), ImGuiInputTextFlags_ReadOnly);
}


void ObjectiveTimerWindow::ObjectiveSet::Update() {
    if (!active) return;

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        time = GW::Map::GetInstanceTime();
        ::PrintTime(cached_time, sizeof(cached_time), time, false);
    }

    for (Objective& obj : objectives) {
        obj.Update();
    }
}
void ObjectiveTimerWindow::ObjectiveSet::CheckSetDone() {
    bool done = true;
    for (const Objective& obj : objectives) {
        if (obj.done == TIME_UNKNOWN) {
            done = false;
            break;
        }
    }
    if (done) {
        active = false;
    }
}

ObjectiveTimerWindow::ObjectiveSet::ObjectiveSet() : ui_id(cur_ui_id++) {
	name[0] = 0;
	GetLocalTime(&system_time);
}

void ObjectiveTimerWindow::ObjectiveSet::Draw() {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s - %s###header%d", name, cached_time, ui_id);

    bool is_open = true;
    if (ImGui::CollapsingHeader(buf, &is_open, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID(ui_id);
        ImGui::SetCursorPosX(GetGridItemX(1));
        ImGui::Text("Started");
        ImGui::SameLine(GetGridItemX(2));
        ImGui::Text("Done");
        ImGui::SameLine(GetGridItemX(3));
        ImGui::Text("Duration");

        for (Objective& objective : objectives) {
            objective.Draw();
        }
		ImGui::PopID();
	}
}
