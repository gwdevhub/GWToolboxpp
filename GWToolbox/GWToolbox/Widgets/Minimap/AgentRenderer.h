#pragma once

#include <GWCA/GameEntities/Agent.h>
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

	bool show_hidden_npcs = false;
	bool boss_colors = true;

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
		Color color = 0xFFF00000;
		Shape_e shape = Tear;
		float size = 0.0f;
		bool color_active = true;
		bool shape_active = true;
		bool size_active = true;
	};

	struct Shape_Vertex : public GW::Vec2f {
		Shape_Vertex(float x, float y, Color_Modifier mod) 
			: GW::Vec2f(x, y), modifier(mod) {}
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

	D3DVertex* vertices = nullptr;	// vertices array
	unsigned int vertices_count = 0;// count of vertices
	unsigned int vertices_max = 0;	// max number of vertices to draw in one call
	unsigned int max_shape_verts = 0;// max number of triangles in a single shape

	Color color_agent_modifier = 0;
	Color color_agent_damaged_modifier = 0;
	Color color_eoe = 0;
	Color color_qz = 0;
	Color color_winnowing = 0;
	Color color_target = 0;
	Color color_player = 0;
	Color color_player_dead = 0;
	Color color_signpost = 0;
	Color color_item = 0;
	Color color_hostile = 0;
	Color color_hostile_dead = 0;
	Color color_neutral = 0;
	Color color_ally = 0;
	Color color_ally_npc = 0;
	Color color_ally_spirit = 0;
	Color color_ally_minion = 0;
	Color color_ally_dead = 0;

	Color profession_colors[11] = {
		0xFF666666,
		0xFFEEAA33,
		0xFF55AA00,
		0xFF4444BB,
		0xFF00AA55,
		0xFF8800AA,
		0xFFBB3333,
		0xFFAA0088,
		0xFF00AAAA,
		0xFF996600,
		0xFF7777CC
	};

	std::vector<CustomAgent*> custom_agents;
	std::unordered_map<DWORD, std::vector<const CustomAgent*>> custom_agents_map;
	void BuildCustomAgentsMap();
	//const CustomAgent* FindValidCustomAgent(DWORD modelid) const;

	float size_default = 0.f;
	float size_player = 0.f;
	float size_signpost = 0.f;
	float size_item = 0.f;
	float size_boss = 0.f;
	float size_minion = 0.f;

	bool agentcolors_changed = false;
	CSimpleIni* agentcolorinifile = nullptr;
};
