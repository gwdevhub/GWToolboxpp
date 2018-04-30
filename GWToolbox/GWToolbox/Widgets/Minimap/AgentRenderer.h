#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include <GWCA\GameEntities\Agent.h>
#include <SimpleIni.h>

#include "Color.h"
#include "VBuffer.h"

class AgentRenderer : public VBuffer {
public:
	AgentRenderer();
	virtual ~AgentRenderer();

	void Render(IDirect3DDevice9* device) override;

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;
	void LoadAgentColors();
	void SaveAgentColors() const;

private:
	static const size_t shape_size = 4;
	enum Shape_e { Tear, Circle, Quad, BigCircle };
	enum Color_Modifier {
		None, // rgb 0,0,0
		Dark, // user defined
		Light, // user defined
		CircleCenter // alpha -50
	};

	class CustomAgent {
	private:
		static unsigned int cur_ui_id;
	public:
		enum class Operation {
			None,
			MoveUp,
			MoveDown,
			Delete,
			ModelIdChange
		};

		CustomAgent(CSimpleIni* ini, const char* section);
		CustomAgent(DWORD _modelId, Color _color, const char* _name);

		bool DrawHeader();
		bool DrawSettings(Operation& op);
		void SaveSettings(CSimpleIni* ini, const char* section) const;

		// utility
		const unsigned int ui_id = 0; // to ensure UI consistency
		size_t index = 0; // index in the array. Used for faster sorting.

		// define the agent
		bool active = true;
		char name[128];
		DWORD modelId = 0;
		DWORD mapId = 0; // 0 for 'any map'

		// attributes to change
		Color color;
		Shape_e shape;
		float size = 0;
		bool color_active = true;
		bool shape_active = true;
		bool size_active = true;
	};

	struct Shape_Vertex : public GW::Vector2f {
		Shape_Vertex(float x, float y, Color_Modifier mod) 
			: GW::Vector2f(x, y), modifier(mod) {}
		Color_Modifier modifier;
	};
	struct Shape_t {
		std::vector<Shape_Vertex> vertices;
		void AddVertex(float x, float y, Color_Modifier mod);
	};
	Shape_t shapes[shape_size];

	void Initialize(IDirect3DDevice9* device) override;

	void Enqueue(const GW::Agent* agent, const CustomAgent* ca = nullptr);
	Color GetColor(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;
	float GetSize(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;
	Shape_e GetShape(const GW::Agent* agent, const CustomAgent* ca = nullptr) const;

	void Enqueue(Shape_e shape, const GW::Agent* agent, float size, Color color);

	D3DVertex* vertices;		// vertices array
	unsigned int vertices_count;// count of vertices
	unsigned int vertices_max;	// max number of vertices to draw in one call
	unsigned int max_shape_verts;// max number of triangles in a single shape

	Color color_agent_modifier;
	Color color_agent_damaged_modifier;
	Color color_eoe;
	Color color_qz;
	Color color_winnowing;
	Color color_target;
	Color color_player;
	Color color_player_dead;
	Color color_signpost;
	Color color_item;
	Color color_hostile;
	Color color_hostile_dead;
	Color color_neutral;
	Color color_ally;
	Color color_ally_npc;
	Color color_ally_spirit;
	Color color_ally_minion;
	Color color_ally_dead;

	std::vector<CustomAgent*> custom_agents;
	std::unordered_map<DWORD, std::vector<const CustomAgent*>> custom_agents_map;
	void BuildCustomAgentsMap();
	//const CustomAgent* FindValidCustomAgent(DWORD modelid) const;

	float size_default;
	float size_player;
	float size_signpost;
	float size_item;
	float size_boss;
	float size_minion;

	bool agentcolors_changed = false;
	CSimpleIni* agentcolorinifile = nullptr;
};
