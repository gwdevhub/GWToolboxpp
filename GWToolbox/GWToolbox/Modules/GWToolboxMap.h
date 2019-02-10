#pragma once

#include <string>
#include <vector>
#include <functional>
#include <Windows.h>
#include <time.h>
#include <Timer.h>
#include <logger.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Constants\Maps.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

class GWToolboxMap : public ToolboxModule {
	GWToolboxMap() {};
	~GWToolboxMap() {};
public:
	static GWToolboxMap& Instance() {
		static GWToolboxMap instance;
		return instance;
	}

	const char* Name() const override { return "GWToolboxMap"; }

	static void Travel(GW::Constants::MapID MapID, GW::Constants::District district, int district_number = 0);
	static void ScrollToOutpost(GW::Constants::MapID outpost_id, GW::Constants::District district = GW::Constants::District::Current, int district_number = 0);
	static bool IsWaitingForMapTravel();
	// Update. Will always be called every frame.
	void Update(float delta) override;
private:
	static GW::Constants::MapID GWToolboxMap::scroll_to_outpost_id;
	static clock_t GWToolboxMap::scroll_to_outpost_timer;
	static bool was_waiting_for_map_travel;
};