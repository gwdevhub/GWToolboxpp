#pragma once


#include "ToolboxModule.h"


class LUAInterface 
	: public ToolboxModule
{
public:

	static LUAInterface& Instance();

	const char* Name() const override { return "LUA Interface"; }

	void Initialize() override;
	void Terminate() override;

	void Update() override;

	void ShowConsole();

	int RunString(std::string cmds);
	int RunString(const char* cmds);

	int RunFile(std::string path);
	int RunFile(const char* path);

	int ClearVM();

public:

	ImGuiTextBuffer     buf_;

	bool		    consoleopen;
	bool            scrolltobottom_;

private:
	void*				lua_;

};