#include <string>
#include <iostream>
#include <vector>

#include "Shlobj.h"
#include "Shlwapi.h"

#include "../SimpleIni.h"

using namespace std;

class Config {
private:
	string  settingsFolder;
	wstring settingsFolderW;
	wstring iniFilePath;
	CSimpleIni iniFile;

public:
	const wstring version = L"1.0";

	Config() {
		TCHAR szPathW[MAX_PATH];
		szPathW[0] = L'\0';
		SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPathW);
		settingsFolderW = szPathW;
		settingsFolderW.append(L"\\GWToolbox");

		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath);
		settingsFolder = szPath;
		settingsFolder.append("\\GWToolbox");

		iniFilePath = wstring(settingsFolderW);
		iniFilePath.append(L"\\GWToolbox.ini");

		std::wcout << settingsFolderW << std::endl;
		std::cout << settingsFolder << std::endl;

		CSimpleIni iniFile(false, false, false);
		iniFile.LoadFile(iniFilePath.c_str());
	}

	~Config() {};

	wstring iniRead(wstring section, wstring key, wstring def) {
		return iniFile.GetValue(section.c_str(), key.c_str(), def.c_str());
	}

	bool iniReadBool(wstring section, wstring key, bool def) {
		wstring defStr = def ? L"True" : L"False";
		wstring value = iniFile.GetValue(section.c_str(), key.c_str(), defStr.c_str());
		return value.compare(L"True") == 0;
	}

	void iniWrite(wstring section, wstring key, wstring value) {
		iniFile.SetValue(section.c_str(), key.c_str(), value.c_str());
	}

	wstring getSettingsFolderW() { return settingsFolderW; }
	string	getSettingsFolder () { return settingsFolder; }

	wstring getPathW(wstring filename) { return settingsFolderW + L"\\" + filename; }
	string	getPath (string filename)  { return settingsFolder  + "\\"  + filename; }
};
