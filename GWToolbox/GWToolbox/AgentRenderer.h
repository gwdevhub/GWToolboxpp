#pragma once

#include <vector>

#include <GWCA\GameEntities\Agent.h>

#include "VBuffer.h"
#include "MinimapUtils.h"

class AgentRenderer : public VBuffer {
public:
	AgentRenderer();

	void Render(IDirect3DDevice9* device) override;

private:
	static const size_t shape_size = 4;
	enum Shape_e { Tear, Circle, Quad, BigCircle };
	struct Shape_t {
		std::vector<GW::Vector2f> vertices;
		std::vector<MinimapUtils::Color> colors;
		void AddVertex(float x, float y, MinimapUtils::Color color);
	};
	Shape_t shapes[shape_size];

	void Initialize(IDirect3DDevice9* device) override;

	void Enqueue(GW::Agent* agent);
	MinimapUtils::Color GetColor(GW::Agent* agent) const;
	float GetSize(GW::Agent* agent) const;
	Shape_e GetShape(GW::Agent* agent) const;

	void Enqueue(Shape_e shape, GW::Agent* agent, float size, MinimapUtils::Color color);

	D3DVertex* vertices;		// vertices array
	unsigned int vertices_count;// count of vertices
	unsigned int vertices_max;	// max number of vertices to draw in one call
	unsigned int max_shape_verts;// max number of triangles in a single shape

	MinimapUtils::Color color_eoe;
	MinimapUtils::Color color_qz;
	MinimapUtils::Color color_target;
	MinimapUtils::Color color_player;
	MinimapUtils::Color color_player_dead;
	MinimapUtils::Color color_signpost;
	MinimapUtils::Color color_item;
	MinimapUtils::Color color_hostile;
	MinimapUtils::Color color_hostile_damaged;
	MinimapUtils::Color color_hostile_dead;
	MinimapUtils::Color color_neutral;
	MinimapUtils::Color color_ally_party;
	MinimapUtils::Color color_ally_npc;
	MinimapUtils::Color color_ally_spirit;
	MinimapUtils::Color color_ally_minion;
	MinimapUtils::Color color_ally_dead;
};
