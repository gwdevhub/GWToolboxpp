#include <string>
#include <vector>

#include "Shlobj.h"
#include "Shlwapi.h"

#include "logger.h"

#include "../SimpleIni.h"

using namespace std;

class Config {
private:
	string  settingsFolderA;	// the settings folder as ansi string
	wstring settingsFolderW;	// the settings folder as wstring
	wstring iniFilePath;		// the full ini file path
	CSimpleIni* iniFile;		// SimpleIni object

public:
	const wstring version = L"1.0";

	Config() {
		WCHAR szPathW[MAX_PATH];
		szPathW[0] = L'\0';
		SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPathW);
		settingsFolderW = szPathW;
		settingsFolderW.append(L"\\GWToolbox");

		CHAR szPath[MAX_PATH];
		szPath[0] = '\0';
		SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath);
		settingsFolderA = szPath;
		settingsFolderA.append("\\GWToolbox");

		wstring iniFilePath = wstring(settingsFolderW);
		iniFilePath.append(L"\\GWToolbox.ini");

		iniFile = new CSimpleIni(false, false, false);
		iniFile->LoadFile(iniFilePath.c_str());
	}

	// Save the changes to the ini file
	void save() {
		iniFile->SaveFile(iniFilePath.c_str());
	}

	// Destruct this object
	// Important: call save if you wish to save the settings to ini
	~Config() {
		iniFile->Reset();
		delete iniFile;
	};

	// Retrieve a wstring value of the key in the section. 
	// Returns def if no value was found.
	const wchar_t* iniRead(const wchar_t* section, const wchar_t* key, const wchar_t* def) {
		return iniFile->GetValue(section, key, def);
	}

	// Retrieve a bool value of the key in the section. 
	// Returns def if no value was found.
	bool iniReadBool(const wchar_t* section, const wchar_t* key, bool def) {
		return iniFile->GetBoolValue(section, key, def);
	}

	// Retrieve a double value of the key in the section. 
	// Returns def if no value was found.
	double iniReadDouble(const wchar_t* section, const wchar_t* key, double def) {
		return iniFile->GetDoubleValue(section, key, def);
	}

	// Add or update a wstring value in the given section / key combination.
	void iniWrite(const wchar_t* section, const wchar_t* key, const wchar_t* value) {
		SI_Error err = iniFile->SetValue(section, key, value);
		IFLTZERR(err, "SimpleIni SetValue error: %d", err);
	}

	// Add or update a bool value in the given section / key combination.
	void iniWriteBool(const wchar_t* section, const wchar_t* key, bool value) {
		SI_Error err = iniFile->SetBoolValue(section, key, value);
		IFLTZERR(err, "SimpleIni SetBoolValue error: %d", err);
	}

	// Add or update a double value in the given section / key combination.
	void iniWriteDouble(const wchar_t* section, const wchar_t* key, double value) {
		SI_Error err = iniFile->SetDoubleValue(section, key, value);
		IFLTZERR(err, "SimpleIni SetDoubleValue error: %d", err);
	}

	wstring getSettingsFolderW() { return settingsFolderW; }
	string	getSettingsFolderA() { return settingsFolderA; }

	wstring getPathW(wstring filename) { return settingsFolderW + L"\\" + filename; }
	string	getPathA(string  filename) { return settingsFolderA +  "\\" + filename; }
};
