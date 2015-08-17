#include <string>

#include "Shlobj.h"
#include "Shlwapi.h"

#include "../SimpleIni.h"

using namespace std;

class Config {
private:
	wstring settingsFolder;
	wstring iniFilePath;
	CSimpleIni iniFile;

public:

	const wstring version = L"1.0";

	Config() {
		TCHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath);
		
		settingsFolder = szPath;
		settingsFolder.append(L"\\GWToolbox");

		iniFilePath = wstring(settingsFolder);
		iniFilePath.append(L"\\GWToolbox.ini");

		CSimpleIni iniFile(false, false, false);
		iniFile.LoadFile(iniFilePath.c_str());
	}

	
};
