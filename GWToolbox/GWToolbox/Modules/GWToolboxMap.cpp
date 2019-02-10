#include "GWToolboxMap.h"
#include <GWCA\Constants\Constants.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\PartyContext.h>


GW::Constants::MapID GWToolboxMap::scroll_to_outpost_id = GW::Constants::MapID::None;
clock_t GWToolboxMap::scroll_to_outpost_timer = 0;
bool GWToolboxMap::was_waiting_for_map_travel = false;

void GWToolboxMap::Travel(GW::Constants::MapID MapID, GW::Constants::District District /*= 0*/, int district_number) {
	switch (MapID) {
	case GW::Constants::MapID::The_Deep:
	case GW::Constants::MapID::Urgozs_Warren:
		return ScrollToOutpost(MapID, District, district_number);
	}
	return GW::Map::Travel(MapID, District, district_number);
}
void GWToolboxMap::Update(float delta) {
	if (scroll_to_outpost_id == GW::Constants::MapID::None) return; // Not in the process of scrolling anywhere
	if (!GW::Map::IsMapLoaded() || !GW::PartyMgr::GetIsPartyLoaded()) {
		was_waiting_for_map_travel = false; // Map loading, so we're no longer waiting for travel timer.
		scroll_to_outpost_timer = 0;
		return; // Wait until map fully loaded and all party members are connected.
	}
	if (IsWaitingForMapTravel()) {
		was_waiting_for_map_travel = true; // Currently in travel countdown.
	} else if(was_waiting_for_map_travel) {
		was_waiting_for_map_travel = false; // We were waiting for countdown, but it was cancelled.
		scroll_to_outpost_id = GW::Constants::MapID::None; 
		return;
	}
	ScrollToOutpost(scroll_to_outpost_id); // We're in the process of scrolling to an outpost, map is loaded and ready to go.
}
bool GWToolboxMap::IsWaitingForMapTravel() {
	return (GW::GameContext::instance()->party->flag & 0x8) > 0;
}
void GWToolboxMap::ScrollToOutpost(GW::Constants::MapID outpost_id, GW::Constants::District district, int district_number) {
	if (scroll_to_outpost_timer > 0 && TIMER_DIFF(scroll_to_outpost_timer) < 2000) return; // Checking too soon e.g. not enough time for Travel packet to be sent.
	if (!GW::Map::IsMapLoaded() || !GW::PartyMgr::GetIsPartyLoaded()) return; // Wait until map fully loaded and all party members are connected.
	if (IsWaitingForMapTravel()) return; // Wait until no longer waiting for travel.

	if (scroll_to_outpost_id == GW::Constants::MapID::None) {
		scroll_to_outpost_id = outpost_id;
	}
	if (scroll_to_outpost_id != outpost_id) return; // Already travelling to another outpost
	GW::Constants::MapID map_id = GW::Map::GetMapID();
	if (map_id == outpost_id) { // Already here. Fall back to GW::Map::Travel just in case district is different.
		scroll_to_outpost_id = GW::Constants::MapID::None;
		GW::Map::Travel(outpost_id, district, district_number);
		return; 
	}

	int scroll_model_id = 0;
	bool is_ready_to_scroll = map_id == GW::Constants::MapID::Embark_Beach;
	switch (scroll_to_outpost_id) {
		case GW::Constants::MapID::The_Deep:
			scroll_model_id = 22279;
			is_ready_to_scroll = is_ready_to_scroll || map_id == GW::Constants::MapID::Cavalon_outpost;
			break;
		case GW::Constants::MapID::Urgozs_Warren:
			scroll_model_id = 3256;
			is_ready_to_scroll = is_ready_to_scroll || map_id == GW::Constants::MapID::House_zu_Heltzer_outpost;
			break;
	}

	GW::Item* scroll_to_use = scroll_model_id > 0 ? GW::Items::GetItemByModelId(scroll_model_id, 1, 21) : nullptr;
	if (!scroll_to_use) {
		scroll_to_outpost_id = GW::Constants::MapID::None;
		Log::Error("No scroll found in inventory for travel");
		return; // No scroll found.
	}
	if (is_ready_to_scroll) {
		scroll_to_outpost_id = GW::Constants::MapID::None;
		GW::Items::UseItem(scroll_to_use);
		return; // Done.
	}
	GW::Map::Travel(GW::Constants::MapID::Embark_Beach, district, district_number); // Travel to embark.
	scroll_to_outpost_timer = TIMER_INIT();
}