#include "API\APIMain.h"
#include "GWToolbox\GWToolbox.h"

#include <time.h>
#include <fstream>

#define DEBUG 1

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
	PEXCEPTION_RECORD records = ExceptionInfo->ExceptionRecord;
	PCONTEXT cpudbg = ExceptionInfo->ContextRecord;
	if (records->ExceptionFlags != 0){

		time_t rawtime;
		time(&rawtime);

		struct tm timeinfo;
		localtime_s(&timeinfo, &rawtime);

		char buffer[64];
		asctime_s(buffer, sizeof(buffer), &timeinfo);

		string filename = string("Crash-") + string(buffer) + string(".txt");
		string path = GuiUtils::getPathA(filename.c_str());
		ofstream logfile(path.c_str());


		logfile << hex;
		logfile << "Code: " << records->ExceptionCode << endl;
		logfile << "Addr: " << records->ExceptionAddress << endl;
		logfile << "Flag: " << records->ExceptionFlags << endl << endl;

		logfile << "Registers:" << endl;
		logfile << "\teax: " << cpudbg->Eax << endl;
		logfile << "\tebx: " << cpudbg->Ebx << endl;
		logfile << "\tecx: " << cpudbg->Ecx << endl;
		logfile << "\tedx: " << cpudbg->Edx << endl;
		logfile << "\tesi: " << cpudbg->Esi << endl;
		logfile << "\tedi: " << cpudbg->Edi << endl;
		logfile << "\tesp: " << cpudbg->Esp << endl;
		logfile << "\tebp: " << cpudbg->Ebp << endl;
		logfile << "\teip: " << cpudbg->Eip << endl << endl;

		logfile << "Stack:" << endl;

		for (DWORD i = 0; i <= 0x40; i += 4)
			logfile << "\t[esp+" << i << "]: " << ((DWORD*)(cpudbg->Esp))[i / 4];


		logfile.close();

		MessageBoxA(0, "Guild Wars - Crash!", "GWToolbox has detected Guild Wars has crashed, oops.\nThis may be a crash toolbox has made, or something completely irrelevant to it.\nFor help on this matter, please access the log file placed in the \"crash\" folder of the GWToolbox data folder with the correct date and time, and send the file along with any information or actions surrounding the crash to the GWToolbox++ developers.\nThank you and sorry for the inconvenience.", 0);
		return EXCEPTION_EXECUTE_HANDLER;
	}
	else{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
}

// Do all your startup things here instead.
void init(HMODULE hModule){

#if DEBUG_BUILD
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitleA("GWTB++ Debug Console");
#endif

	SetUnhandledExceptionFilter(ExceptionHandler);
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)GWToolbox::threadEntry, hModule, 0, 0);
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}
