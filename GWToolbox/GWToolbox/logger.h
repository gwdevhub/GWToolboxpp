#pragma once

namespace Log {
	// === Setup and cleanup ====
	// in release redirects stdout and stderr to log file
	// in debug creates console
	void InitializeLog();
	void InitializeChat();
	void Terminate();

	// === File/console logging ===
	// printf-style log
	void Log(const char* msg, ...);
	
	// printf-style wide-string log
	void LogW(const wchar_t* msg, ...);

	// flushes log file.
	//static void FlushFile() { fflush(logfile); }

	// === Game chat logging ===
	// Shows to the user in the form of a white chat message from toolbox
	void Info(const char* format, ...);

	// Shows to the user in the form of a red chat message from toolbox
	void Error(const char* format, ...);

	// Shows to the user in the form of a yellow chat message from toolbox
	void Warning(const char* format, ...);

	// === Crash Dump ===
	// Creates minidump, to be called from within __except()
	LONG WINAPI GenerateDump(
		EXCEPTION_POINTERS* pExceptionPointers);
};
