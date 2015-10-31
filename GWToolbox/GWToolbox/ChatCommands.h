#pragma once

#include <string>
#include <vector>

class ChatCommands {
public:
	ChatCommands();

private:
	void PconCmd(const std::vector<std::wstring> &args);
};
