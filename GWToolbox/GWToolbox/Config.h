#pragma once

#include <string>
#include <vector>
#include <list>

#include "Shlobj.h"
#include "Shlwapi.h"

#include "logger.h"
#include "GuiUtils.h"

#include "SimpleIni.h"

using namespace std;

class Config {
private:
	wstring iniFilePath;		// the full ini file path
	CSimpleIni* iniFile;		// SimpleIni object

public:
	const wstring version = L"1.0";

	Config() {
		iniFilePath = GuiUtils::getPathW(L"GWToolbox.ini");

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

	// Retrieve a long value of the key in the section. 
	// Returns def if no value was found.
	long iniReadLong(const wchar_t* section, const wchar_t* key, long def) {
		return iniFile->GetLongValue(section, key, def);
	}

	// Retrieve a double value of the key in the section. 
	// Returns def if no value was found.
	double iniReadDouble(const wchar_t* section, const wchar_t* key, double def) {
		return iniFile->GetDoubleValue(section, key, def);
	}

	// Add or update a wstring value in the given section / key combination.
	void iniWrite(const wchar_t* section, const wchar_t* key, const wchar_t* value) {
		SI_Error err = iniFile->SetValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetValue error: %d", err);
		}
	}

	// Add or update a bool value in the given section / key combination.
	void iniWriteBool(const wchar_t* section, const wchar_t* key, bool value) {
		SI_Error err = iniFile->SetBoolValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetBoolValue error: %d", err);
		}
	}

	// Add or update a long value in the given section / key combination.
	void iniWriteLong(const wchar_t* section, const wchar_t* key, long value) {
		SI_Error err = iniFile->SetLongValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetLongValue error: %d", err);
		}
	}

	// Add or update a double value in the given section / key combination.
	void iniWriteDouble(const wchar_t* section, const wchar_t* key, double value) {
		SI_Error err = iniFile->SetDoubleValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetDoubleValue error: %d", err);
		}
	}

	// returns a list of all the section names
	// note: do not rely on the order
	std::list<wstring> iniReadSections() {
		CSimpleIni::TNamesDepend entries;
		iniFile->GetAllSections(entries);
		std::list<wstring> sections(entries.size());
		for (CSimpleIni::Entry entry : entries) {
			sections.push_back(wstring(entry.pItem));
		}
		return sections;
	}

	// deletes a section from the ini file
	void iniDeleteSection(const wchar_t* section) {
		iniFile->Delete(section, NULL);
	}
};
