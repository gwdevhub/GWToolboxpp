#include "InstanceTimerWindow.h"

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
static void Set(InstanceTimerWindow::Objective &obj, uint32_t id, const char *name);
static uint32_t doa_get_next(uint32_t id);

void InstanceTimerWindow::Initialize() {
	ToolboxWindow::Initialize();

	Set(obj_uw[0], 146, "Chamber");
	Set(obj_uw[1], 147, "Restore");
	Set(obj_uw[2], 148, "Escort");
	Set(obj_uw[3], 149, "UWG");
	Set(obj_uw[4], 150, "Vale");
	Set(obj_uw[5], 151, "Waste");
	Set(obj_uw[6], 152, "Pits");
	Set(obj_uw[7], 153, "Planes");
	Set(obj_uw[8], 154, "Mnts");
	Set(obj_uw[9], 155, "Pools");
	Set(obj_uw[10], 156, nullptr);
	Set(obj_uw[11], 157, "Dhuum");

	Set(obj_fow[0], 309, "ToC");
	Set(obj_fow[1], 310, "Wailing Lord");
	Set(obj_fow[2], 311, "Griffons");
	Set(obj_fow[3], 312, "Defend");
	Set(obj_fow[4], 313, "Camp");
	Set(obj_fow[5], 314, "Menzies");
	Set(obj_fow[6], 315, "Restore");
	Set(obj_fow[7], 316, "Khobay");
	Set(obj_fow[8], 317, "ToS");
	Set(obj_fow[9], 318, "Burning Forest");
	Set(obj_fow[10], 319, "The Hunt");

	Set(obj_doa[0], 0x273F, "Foundry");
	Set(obj_doa[1], 0x2740, "Veil");
	Set(obj_doa[2], 0x2741, "Gloom");
	Set(obj_doa[3], 0x2742, "City");

	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveAdd>(
	[this](GW::Packet::StoC::ObjectiveAdd* packet) -> bool {
		Objective *obj = get_objective(packet->objective_id);
		obj->start = GW::Map::GetInstanceTime() / 1000;
		DWORD t = obj->start;
		snprintf(obj->cached_start, sizeof(obj->cached_start), "%02d:%02d:%02d", t / (60 * 60), (t / 60) % 60, t % 60);
		return false;
	});
	
	GW::StoC::AddCallback<GW::Packet::StoC::ObjectiveDone>(
	[this](GW::Packet::StoC::ObjectiveDone* packet) -> bool {
		Objective *obj = get_objective(packet->objective_id);
		obj->done = GW::Map::GetInstanceTime() / 1000;
		DWORD t = obj->done;
		snprintf(obj->cached_done, sizeof(obj->cached_done), "%02d:%02d:%02d", t / (60 * 60), (t / 60) % 60, t % 60);
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::DoACompleteZone>(
	[this](GW::Packet::StoC::DoACompleteZone* packet) -> bool {
		if (packet->message[0] != 0x8101)
			return false;
		uint32_t id = packet->message[1];
		Objective *obj = get_objective(id);
		obj->done = GW::Map::GetInstanceTime() / 1000;
		DWORD t = obj->done;
		snprintf(obj->cached_done, sizeof(obj->cached_done), "%02d:%02d:%02d", t / (60 * 60), (t / 60) % 60, t % 60);
		uint32_t next_id = doa_get_next(id);
		Objective *next = get_objective(next_id);
		if (next && !next->done) {
			next->start = t;
			strncpy(next->cached_start, obj->cached_done, sizeof(next->cached_start));
		}
		return false;
	});
}

void InstanceTimerWindow::DrawObjectives(IDirect3DDevice9* pDevice, const char *title, Objective *objectives, size_t count) {
	if (ImGui::CollapsingHeader(title)) {
		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
		ImGui::Text("Taken");
		ImGui::SameLine(2 * ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
		ImGui::Text("Done");

		for (size_t i = 0; i < count; ++i) {
			Objective *obj = &objectives[i];
			if (!obj->name) continue;
			if (ImGui::Button(obj->name, ImVec2((ImGui::GetWindowContentRegionWidth() / 3), 0))) {
				snprintf(buf, 256, "[%s] ~ Taken: %s ~ Done: %s", obj->name, obj->cached_start, obj->cached_done);
				GW::Chat::SendChat('#', buf);
			}
			ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x * 2) / 3);
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::InputText("", obj->cached_start, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::InputText("", obj->cached_done, 32, ImGuiInputTextFlags_ReadOnly);
		}
	}
}

void InstanceTimerWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		if (show_doatimer)
			DrawObjectives(pDevice, "DoA Timer", obj_doa, countof(obj_doa));
		if (show_uwtimer)
			DrawObjectives(pDevice, "Underworld Timer", obj_uw, countof(obj_uw));
		if (show_fowtimer)
			DrawObjectives(pDevice, "Fissure of Woe Timer", obj_fow, countof(obj_fow));
	}
	ImGui::End();
}

static void Set(InstanceTimerWindow::Objective &obj, uint32_t id, const char *name) {
	obj.id = id;
	obj.name = name;
	strncpy(obj.cached_done, "00:00:00", sizeof(obj.cached_done));
	strncpy(obj.cached_start, "00:00:00", sizeof(obj.cached_start));
}

InstanceTimerWindow::Objective *InstanceTimerWindow::get_objective(uint32_t obj_id) {
	Objective *objectives;
	uint32_t id;
	if (309 <= obj_id && obj_id <= 319) {
		// fow
		id = obj_id - 309;
		if (id >= countof(obj_fow))
			return nullptr;
		objectives = obj_fow;
	} else if (146 <= obj_id && obj_id <= 157) {
		// uw
		id = obj_id - 146;
		if (id >= countof(obj_uw))
			return nullptr;
		objectives = obj_uw;
	} else if (0x273F <= obj_id && obj_id <= 0x2742) {
		id = obj_id - 0x273F;
		if (id >= countof(obj_doa))
			return nullptr;
		objectives = obj_doa;
	} else {
		return nullptr;
	}
	Objective *obj = objectives + id;
	if (obj_id != obj->id) {
		fprintf(stderr, "Objective id mismatch (expected: %lu, received: %lu)\n", obj->id, obj_id);
		return nullptr;
	}
	return obj;
}

static uint32_t doa_get_next(uint32_t id) {
	switch (id) {
	case 0x273F: return 0x2742; // foundry -> city;
	case 0x2742: return 0x2740; // city -> veil
	case 0x2740: return 0x2741; // veil -> gloom
	case 0x2741: return 0x273F; // gloom -> foundry
	}
	return 0;
}

void InstanceTimerWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
	show_uwtimer = ini->GetBoolValue(Name(), VAR_NAME(show_uwtimer), true);
	show_doatimer = ini->GetBoolValue(Name(), VAR_NAME(show_doatimer), true);
}

void InstanceTimerWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(show_uwtimer), show_uwtimer);
	ini->SetBoolValue(Name(), VAR_NAME(show_doatimer), show_doatimer);
}
