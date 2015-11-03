#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

class ChatCommands {
	typedef std::function<bool(std::vector<std::wstring>)> Handler_t;

public:
	ChatCommands();

private:
	void AddCommand(std::wstring cmd, Handler_t);

	void ParseCommand(std::wstring cmd, std::vector<std::wstring> args);

	bool PconCmd(std::vector<std::wstring> args);

	// map from command to handler executing the command
	// the handler takes a vector of arguments
	// returns true if successful, false if error. 
	// On error, a sequence of commands will stop executing
	std::map<std::wstring, Handler_t> commands_;
};
