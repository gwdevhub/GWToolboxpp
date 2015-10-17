#pragma once

#include <string>
#include <vector>

class ChatCommands {
public:
	ChatCommands();

private:
	void PconCmd(std::vector<std::wstring> args);
};
