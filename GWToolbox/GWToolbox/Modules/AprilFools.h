#pragma once
#include <GWCA/Constants/Constants.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>

class AprilFools : public ToolboxModule {
	AprilFools() {};
	~AprilFools() {};
public:
	static AprilFools& Instance() {
		static AprilFools instance;
		return instance;
	}

	const char* Name() const override { return "April Fools"; }

	void Initialize() override;
	void Update(float delta) override;
	void SetInfected(GW::Agent* agent, bool is_infected);
	void SetEnabled(bool is_enabled);
	bool enabled = false;
private:
	GW::HookEntry stoc_hook;

	std::map<uint32_t, GW::Agent*> player_agents;
};
