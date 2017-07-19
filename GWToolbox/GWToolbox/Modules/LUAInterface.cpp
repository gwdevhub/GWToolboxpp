#include "LUAInterface.h"

#include <ctime>

#include <lua\lua.hpp>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>



static LUAInterface g_inst;
static char			g_tbscriptdir[0x160];
static char*		g_tbscriptptr = nullptr;
LUAInterface& LUAInterface::Instance() 
{
	return g_inst;
}

#pragma region cmds
#define LUA_NEWTABLE(l, count) lua_createtable(l, 0, count)
#define LUA_TABLEINT(l, name,val)    lua_pushinteger(l, val); lua_setfield(l, -2, name)
#define LUA_TABLEFLOAT(l, name,val)    lua_pushnumber(l, val); lua_setfield(l, -2, name)

static int cmdMove(lua_State* L) 
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);

	if (GW::Agents::GetAgentArray().size() > 0)
	{
		GW::Agents::Move(x, y);
	}
	return 0;
}

static int cmdSendChat(lua_State* L)
{
	const char* s = luaL_checkstring(L, 1);
	const char* c = luaL_checkstring(L, 2);
	if (s && c)
	{
		GW::Chat::SendChat(*c, s);
	}
	return 0;
}

static int cmdGetMapId(lua_State* L)
{
	lua_pushinteger(L, (lua_Integer)GW::Map::GetMapID());
	return 1;
}

static int cmdSleep(lua_State* L)
{
	//TODO: Do something to make a wait happen without blocking thread
	return 0;
}

static int cmdGetAgent(lua_State* L)
{
	int id = luaL_checkinteger(L, 1);

	GW::Agent* ag;
	switch (id) {
	case -1: ag = GW::Agents::GetTarget(); break;
	case -2: ag = GW::Agents::GetPlayer(); break;
	default: ag = GW::Agents::GetAgentByID(id); break;
	}
	
	if (!ag) {
		lua_pushinteger(L, 0);
		return 0;
	}
		

	LUA_NEWTABLE(L, 6);
	{
		LUA_TABLEINT(L,   "id", ag->Id);
		LUA_TABLEFLOAT(L, "x", ag->pos.x);
		LUA_TABLEFLOAT(L, "y", ag->pos.y);
		LUA_TABLEINT(L, "plane", ag->plane);
		LUA_TABLEINT(L, "dead", (ag->Effects & 0x10) > 0);
		LUA_TABLEINT(L, "modelidx", ag->PlayerNumber);
	}

	return 1;
}

static int cmdGetAgentPos(lua_State* L)
{
	int id = luaL_checkinteger(L, 1);

	GW::Agent* ag;
	switch (id) {
	case -1: ag = GW::Agents::GetTarget(); break;
	case -2: ag = GW::Agents::GetPlayer(); break;
	default: ag = GW::Agents::GetAgentByID(id); break;
	}

	if (ag) 
	{
		lua_pushnumber(L, ag->pos.x);
		lua_pushnumber(L, ag->pos.y);
	}
	else
	{
		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
	}
	return 2;
} 

static int cmdTargetAgent(lua_State* L)
{
	int id = luaL_checkinteger(L, 1);
	GW::Agents::ChangeTarget(id);
	return 0;
}

static int cmdUseSkill(lua_State* L)
{
	int skill = luaL_checkinteger(L, 1);
	int agent = luaL_checkinteger(L, 2);


	if (skill < 1 || skill > 8)
		return 0;
	switch (agent) {
	case -1: agent = GW::Agents::GetTargetId(); break;
	case -2: agent = GW::Agents::GetPlayerId(); break;
	}

	GW::SkillbarMgr::UseSkill(skill, agent);

	return 0;
}


// Overrides
static int cmdPrint(lua_State* L)
{
	g_inst.buf_.append("< ");
	int argc = lua_gettop(L);
	for (int i = 1; i <= argc; ++i)
	{
		if (lua_isstring(L, i)) {
			const char* s = lua_tostring(L, i);
			g_inst.buf_.append(s);
		}
	}
	g_inst.buf_.append("\n");
	g_inst.scrolltobottom_ = true;
	return 0;
}

static int cmdDoFile(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	if (name && strlen(name) <= 20) 
	{
		strcpy(g_tbscriptptr, name);
		luaL_dofile(L, g_tbscriptdir);
	}
	return 0;
}

#pragma endregion

void LUAInterface::Initialize()
{
	lua_ = (void*)luaL_newstate();
	luaL_openlibs((lua_State*)lua_);

	static luaL_Reg gwlib[] = 
	{
		{"Move", cmdMove },
		{"SendChat", cmdSendChat },
		{"GetMapId", cmdGetMapId },
		{"GetAgent", cmdGetAgent},
		{"GetPos", cmdGetAgentPos},
		{"TargetAgent", cmdTargetAgent },
		{"UseSkill", cmdUseSkill },
		{nullptr, nullptr}
	};

	luaL_newlib((lua_State*)lua_, gwlib);
	lua_setglobal((lua_State*)lua_, "GW");

#if 0
	static luaL_Reg utillib[] = 
	{
		{"Sleep",cmdSleep},
		{ nullptr, nullptr }
	};

	luaL_newlib((lua_State*)lua_, utillib);
	lua_setglobal((lua_State*)lua_, "util");
#endif

	static const struct luaL_Reg globallib[] = {
		{ "print", cmdPrint },
		{ "dofile", cmdDoFile },
		{ nullptr, nullptr }
	};

	lua_getglobal((lua_State*)lua_, "_G");
	luaL_setfuncs((lua_State*)lua_, globallib, 0);
	lua_pop((lua_State*)lua_, 1);

	{
		DWORD count = GetEnvironmentVariableA("LOCALAPPDATA", g_tbscriptdir, 0x160);
		g_tbscriptptr = g_tbscriptdir + count;
		strcpy(g_tbscriptptr, "\\GWToolboxpp\\scripts\\");
		g_tbscriptptr += sizeof("\\GWToolboxpp\\scripts\\") - 1;
	}

	ToolboxWindow::Initialize();
}

void LUAInterface::Terminate()
{
	lua_close((lua_State*)lua_);
}

void LUAInterface::Draw(IDirect3DDevice9*)
{
	if (visible)
	{
		ShowConsole();
	}
}

void LUAInterface::ShowConsole()
{
	static char input[0x100];

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiSetCond_FirstUseEver);
	ImGui::Begin("LUA Console");
	if (ImGui::Button("Clear")) g_inst.buf_.clear();
	ImGui::SameLine();
	if (ImGui::InputText("Input", input, 0x100, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		buf_.append("> %s\n", input);
		RunString(input);
		scrolltobottom_ = true;
		input[0] = '\0';
	}
	ImGui::Separator();
	ImGui::BeginChild("LUA_Output");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
	ImGui::TextUnformatted(buf_.begin(),buf_.end());
	if (scrolltobottom_)
		ImGui::SetScrollHere(1.0f);
	scrolltobottom_ = false;
	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::End();
}

int LUAInterface::RunString(std::string cmds)
{
	return RunString(cmds.c_str());
}

int LUAInterface::RunString(const char * cmds)
{
	return luaL_dostring((lua_State*)lua_, cmds);
}

int LUAInterface::RunFile(std::string path)
{
	return RunFile(path.c_str());
}

int LUAInterface::RunFile(const char * path)
{
	return luaL_dofile((lua_State*)lua_, path);
}

void LUAInterface::ClearVM()
{
	Terminate();
	Initialize();
}
