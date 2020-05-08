#pragma once

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Maps.h>

#include <GWCA/Packets/StoC.h>

class PartyWindowModule : public ToolboxModule {
	PartyWindowModule() {};
	~PartyWindowModule() {
		ClearSpecialNPCs();
	}
public:
	static PartyWindowModule& Instance() {
		static PartyWindowModule instance;
		return instance;
	}

	const char* Name() const override { return "Party Window"; }
	const char* SettingsName() const override { return "Party Settings"; }
	void Initialize() override;
	void SignalTerminate() override;
	bool CanTerminate() override;
	void Update(float delta) override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;
	void Error(const char* fmt, ...);
	void CheckMap();

private:
	void LoadDefaults();
private:

	struct PendingAddToParty {
		PendingAddToParty(uint32_t _agent_id, uint32_t _allegiance_bits, uint32_t _player_number) : agent_id(_agent_id), allegiance_bits(_allegiance_bits), player_number(_player_number) {};
		uint32_t agent_id;
		uint32_t player_number;
		uint32_t allegiance_bits;
		bool IsValidAgent() const;
	};
	struct SpecialNPCToAdd {
		SpecialNPCToAdd(const char* _alias, uint32_t _model_id, GW::Constants::MapID _map_id) : model_id(_model_id), map_id(_map_id), alias(_alias) {};
		std::wstring map_name;
		bool decode_pending = false;
		std::wstring* GetMapName();
		std::string alias;
		uint32_t model_id = 0;
		GW::Constants::MapID map_id = GW::Constants::MapID::None;
	};

	std::vector<uint32_t> allies_added_to_party;

	void AddSpecialNPC(SpecialNPCToAdd npc) {
		SpecialNPCToAdd* new_npc = new SpecialNPCToAdd(npc);
		user_defined_npcs.push_back(new_npc);
		user_defined_npcs_by_model_id.emplace(npc.model_id, new_npc);
	}
	void RemoveSpecialNPC(uint32_t model_id) {
		user_defined_npcs_by_model_id.erase(model_id);
		for (uint32_t i = 0; i < user_defined_npcs.size(); i++) {
			if (!user_defined_npcs[i])
				continue;
			if (user_defined_npcs[i]->model_id == model_id) {
				delete user_defined_npcs[i];
				user_defined_npcs[i] = nullptr;
				// Don't actually call erase() because its mad dodgy, but set to nullptr instead.
				break;
			}
		}
	}
	void ClearSpecialNPCs() {
		user_defined_npcs_by_model_id.clear();
		for (uint32_t i = 0; i < user_defined_npcs.size(); i++) {
			if (!user_defined_npcs[i]) continue;
			delete user_defined_npcs[i];
		}
		user_defined_npcs.clear();
	}
	void ClearAddedAllies();

	std::vector<SpecialNPCToAdd*> user_defined_npcs;
	std::map<uint32_t,SpecialNPCToAdd*> user_defined_npcs_by_model_id;

	std::map<GW::Constants::MapID, std::wstring> map_names_by_id;

	bool add_npcs_to_party_window = true; // Quick tickbox to disable the module without restarting TB
	bool add_player_numbers_to_party_window = false;

	char new_npc_alias[128] = { 0 };
	int new_npc_model_id = 0;
	int new_npc_map_id = 0;
	bool map_name_to_translate = true;
	size_t pending_clear = 0;

	bool ShouldAddAgentToPartyWindow(uint32_t agent_type);
	bool ShouldAddAgentToPartyWindow(GW::Agent* agent);
	bool ShouldRemoveAgentFromPartyWindow(uint32_t agent_id);
	void RemoveAllyActual(uint32_t agent_id);
	void AddAllyActual(PendingAddToParty p);

	GW::HookEntry AgentState_Entry;
	GW::HookEntry AgentRemove_Entry;
	GW::HookEntry AgentAdd_Entry;
	GW::HookEntry GameSrvTransfer_Entry;

};
