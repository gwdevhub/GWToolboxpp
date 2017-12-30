#include "LUAInterface.h"

#include <ctime>

#include <lua\lua.hpp>
#include <lua\lstate.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

/*
 - Execute(const char *str);
   - pushstring
   -
*/

#define LUA_CFUNC_SIG(name) int (name)(lua_State *state)

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
	float x = (float)luaL_checknumber(L, 1);
	float y = (float)luaL_checknumber(L, 2);

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
	lua_Integer id = luaL_checkinteger(L, 1);

	GW::Agent* ag;
	switch (id) {
	case -1: ag = GW::Agents::GetTarget(); break;
	case -2: ag = GW::Agents::GetPlayer(); break;
	default: ag = GW::Agents::GetAgentByID((DWORD)id); break;
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
	lua_Integer id = luaL_checkinteger(L, 1);

	GW::Agent* ag;
	switch (id) {
	case -1: ag = GW::Agents::GetTarget(); break;
	case -2: ag = GW::Agents::GetPlayer(); break;
	default: ag = GW::Agents::GetAgentByID((DWORD)id); break;
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
	lua_Integer id = luaL_checkinteger(L, 1);
	GW::Agents::ChangeTarget((DWORD)id);
	return 0;
}

static int cmdUseSkill(lua_State* L)
{
	lua_Integer skill = luaL_checkinteger(L, 1);
	lua_Integer agent = luaL_checkinteger(L, 2);


	if (skill < 1 || skill > 8)
		return 0;
	switch (agent) {
	case -1: agent = GW::Agents::GetTargetId(); break;
	case -2: agent = GW::Agents::GetPlayerId(); break;
	}

	GW::SkillbarMgr::UseSkill((DWORD)skill, (DWORD)agent);

	return 0;
}


// Overrides
static int cmdPrint(lua_State* L)
{
	g_inst.buf_.appendf("< ");
	int argc = lua_gettop(L);
	for (int i = 1; i <= argc; ++i)
	{
		if (lua_isstring(L, i)) {
			const char* s = lua_tostring(L, i);
			g_inst.buf_.appendf(s);
		}
	}
	g_inst.buf_.appendf("\n");
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

static int cmdGetTargetId(lua_State *L)
{
	GW::Agent *a = GW::Agents::GetTarget();
	if (!a) lua_pushnil(L);
	else    lua_pushinteger(L, a->PlayerNumber);
	return 1;
}

LUA_CFUNC_SIG(Dialog) {
	lua_Integer dialog_id = luaL_checkinteger(state, 1);
	GW::Agents::Dialog((DWORD)dialog_id);
	return 0;
}

LUA_CFUNC_SIG(Travel) {
	int argc = lua_gettop(state);

	GW::Constants::District district;

	lua_Integer town = luaL_checkinteger(state, 1);
	const char *dis = luaL_checkstring(state, 2);

	// GetRegion(), GetLanguage()
	GW::Map::Travel((GW::Constants::MapID)town);

	return 0;
}

static lua_Integer ParseAgentId(lua_State *state, int arg) {
	lua_Integer agent_id = -1;
	if (lua_isstring(state, arg)) {
		const char *s = lua_tostring(state, arg);
		// "player", "target", "mouseover", "pet", "npc", "party1" .. "party12", "arena1" .. "arena8"
		if (!strcmp(s, "player")) {
			agent_id = GW::Agents::GetPlayerId();
		} else if (!strcmp(s, "target")) {
			agent_id = GW::Agents::GetTargetId();
		} else if (!strcmp(s, "mouseover")) {
			agent_id = GW::Agents::GetMouseoverId();
		} else if (!strcmp(s, "pet")) {

		} else if (!strcmp(s, "npc")) {

		} else if (!strcmp(s, "party1")) {

		} else if (!strcmp(s, "party2")) {

		} else if (!strcmp(s, "party3")) {

		} else if (!strcmp(s, "party4")) {

		} else if (!strcmp(s, "party5")) {

		} else if (!strcmp(s, "party6")) {

		} else if (!strcmp(s, "party7")) {

		} else if (!strcmp(s, "party8")) {

		} else if (!strcmp(s, "party9")) {

		} else if (!strcmp(s, "party10")) {

		} else if (!strcmp(s, "party11")) {

		} else if (!strcmp(s, "party12")) {

		}
	}
	return agent_id;
}

LUA_CFUNC_SIG(AgentExists) {
	lua_Integer agent = ParseAgentId(state, 1);
	lua_pushboolean(state, agent != -1);
	return 1;
}

LUA_CFUNC_SIG(AgentWorldId) {
	lua_Integer agent = ParseAgentId(state, 1);
	if (agent == -1) {
		lua_pushnil(state);
	} else {
		lua_pushinteger(state, agent);
	}
	return 1;
}

LUA_CFUNC_SIG(AgentName) {
	return 0;
}

LUA_CFUNC_SIG(AgentGUID) {
	return 0;
}

LUA_CFUNC_SIG(AgentProfession) {
	return 0;
}

LUA_CFUNC_SIG(AgentHealth) {
	return 0;
}

LUA_CFUNC_SIG(AgentHealthMax) {
	return 0;
}

LUA_CFUNC_SIG(AgentEnergy) {
	return 0;
}

LUA_CFUNC_SIG(AgentEnergyMax) {
	return 0;
}

#pragma endregion

void LUAInterface::Initialize()
{
	state = luaL_newstate();

	// Fix that
	luaL_openlibs(state);

	lua_register(state, "Move", cmdMove);
	lua_register(state, "SendChat", cmdSendChat);

	lua_register(state, "Dialog", Dialog);
	lua_register(state, "Travel", Travel);

	lua_register(state, "AgentExists", AgentExists);
	lua_register(state, "AgentName", AgentName);
	lua_register(state, "AgentGUID", AgentGUID);
	lua_register(state, "AgentHealth", AgentHealth);
	lua_register(state, "AgentHealthMax", AgentHealthMax);
	lua_register(state, "AgentHealth", AgentEnergy);
	lua_register(state, "AgentHealth", AgentEnergyMax);
	lua_register(state, "AgentProfession", AgentProfession);

	/*
	 * CreateCommand
	 * DeleteCommand
	 * LoadBuild
	 * Travel(474, "ee", 1)
	 * SendChat
	 *
	 * agentID = "player", "target", "mouseover", "pet", "npc", "party1" .. "party12", "arena1" .. "arena8"
	 *
	 * AgentName(agentID)
	 * AgentGUID(name)
	 *
	 */

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

	lua_getglobal(state, "_G");
	luaL_setfuncs(state, globallib, 0);
	lua_pop(state, 1);

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
	lua_close(state);
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
	if (visible) {
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiSetCond_FirstUseEver);
		ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
		if (ImGui::Button("Clear")) g_inst.buf_.clear();
		ImGui::SameLine();
		if (ImGui::InputText("Input", input, 0x100, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			buf_.appendf("> %s\n", input);
			RunString(input);
			scrolltobottom_ = true;
			input[0] = '\0';
		}
		ImGui::Separator();
		ImGui::BeginChild("LUA_Output");
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
		ImGui::TextUnformatted(buf_.begin(), buf_.end());
		if (scrolltobottom_)
			ImGui::SetScrollHere(1.0f);
		scrolltobottom_ = false;
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::End();
	}
}

int LUAInterface::RunString(std::string cmds)
{
	return RunString(cmds.c_str());
}

int LUAInterface::RunString(const char * cmds)
{
// lua_sethook(L, f, LUA_MASKRET, 0);
	int old_top = lua_gettop(state);
	luaL_loadstring(state, cmds);

	int err = lua_pcall(state, 0, LUA_MULTRET, 0);
	StkId returns = state->ci->func;

	TValue *r1 = returns + 1;
	if (ttisnumber(r1))
		lua_Integer i = r1->value_.i;

	return err;
}

int LUAInterface::RunFile(std::string path)
{
	return RunFile(path.c_str());
}

int LUAInterface::RunFile(const char * path)
{
	return luaL_dofile(state, path);
}

void LUAInterface::ClearVM()
{
	Terminate();
	Initialize();
}
