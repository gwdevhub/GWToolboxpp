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
public:
	const wstring version = L"1.0";

	Config() {
		inifile_path_ = GuiUtils::getPath(L"GWToolbox.ini");

		inifile_ = new CSimpleIni(false, false, false);
		inifile_->LoadFile(inifile_path_.c_str());
	}

	// Save the changes to the ini file
	void Save() {
		inifile_->SaveFile(inifile_path_.c_str());
	}

	// Destruct this object
	// Important: call save if you wish to save the settings to ini
	~Config() {
		inifile_->Reset();
		delete inifile_;
	};

	// Retrieve a wstring value of the key in the section. 
	// Returns def if no value was found.
	const wchar_t* IniRead(const wchar_t* section, const wchar_t* key, const wchar_t* def) {
		return inifile_->GetValue(section, key, def);
	}

	// Retrieve a bool value of the key in the section. 
	// Returns def if no value was found.
	bool IniReadBool(const wchar_t* section, const wchar_t* key, bool def) {
		return inifile_->GetBoolValue(section, key, def);
	}

	// Retrieve a long value of the key in the section. 
	// Returns def if no value was found.
	long IniReadLong(const wchar_t* section, const wchar_t* key, long def) {
		return inifile_->GetLongValue(section, key, def);
	}

	// Retrieve a double value of the key in the section. 
	// Returns def if no value was found.
	double IniReadDouble(const wchar_t* section, const wchar_t* key, double def) {
		return inifile_->GetDoubleValue(section, key, def);
	}

	// Add or update a wstring value in the given section / key combination.
	void IniWrite(const wchar_t* section, const wchar_t* key, const wchar_t* value) {
		SI_Error err = inifile_->SetValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetValue error: %d", err);
		}
	}

	// Add or update a bool value in the given section / key combination.
	void IniWriteBool(const wchar_t* section, const wchar_t* key, bool value) {
		SI_Error err = inifile_->SetBoolValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetBoolValue error: %d", err);
		}
	}

	// Add or update a long value in the given section / key combination.
	void IniWriteLong(const wchar_t* section, const wchar_t* key, long value) {
		SI_Error err = inifile_->SetLongValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetLongValue error: %d", err);
		}
	}

	// Add or update a double value in the given section / key combination.
	void IniWriteDouble(const wchar_t* section, const wchar_t* key, double value) {
		SI_Error err = inifile_->SetDoubleValue(section, key, value);
		if (err < 0) {
			LOG("SimpleIni SetDoubleValue error: %d", err);
		}
	}

	// returns a list of all the section names
	// note: do not rely on the order
	std::list<wstring> IniReadSections() {
		CSimpleIni::TNamesDepend entries;
		inifile_->GetAllSections(entries);
		std::list<wstring> sections(entries.size());
		for (CSimpleIni::Entry entry : entries) {
			sections.push_back(wstring(entry.pItem));
		}
		return sections;
	}

	// deletes a section from the ini file
	void IniDeleteSection(const wchar_t* section) {
		inifile_->Delete(section, NULL);
	}

private:
	wstring inifile_path_;		// the full ini file path
	CSimpleIni* inifile_;		// SimpleIni object
};
