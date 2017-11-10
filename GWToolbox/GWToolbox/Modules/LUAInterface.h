#pragma once


#include "ToolboxWindow.h"

struct lua_State;
class LUAInterface  : public ToolboxWindow
{
public:

	static LUAInterface& Instance();

	const char* Name() const override { return "LUA"; }

	void Initialize() override;
	void Terminate() override;

	void Draw(IDirect3DDevice9*) override;

	void ShowConsole();

	int RunString(std::string cmds);
	int RunString(const char* cmds);

	int RunFile(std::string path);
	int RunFile(const char* path);

	void ClearVM();

public:

	ImGuiTextBuffer     buf_;
	bool            scrolltobottom_;

private:
	lua_State *state;

	std::string commands[256];
};