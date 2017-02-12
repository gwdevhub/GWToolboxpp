#pragma once

#include <string>
#include <list>

#include "SimpleIni.h"

namespace Config {
	extern std::string inifile_path_;		// the full ini file path
	extern CSimpleIni* inifile_;		// SimpleIni object

	void Initialize();
	void Destroy();

	inline void Save() {
		inifile_->SaveFile(inifile_path_.c_str());
	}

	// Retrieve a wstring value of the key in the section. 
	// Returns def if no value was found.
	inline const char* IniRead(const char* section, const char* key, const char* def) {
		return inifile_->GetValue(section, key, def);
	}

	// Retrieve a bool value of the key in the section. 
	// Returns def if no value was found.
	inline bool IniRead(const char* section, const char* key, bool def) {
		return inifile_->GetBoolValue(section, key, def);
	}

	// Retrieve a long value of the key in the section. 
	// Returns def if no value was found.
	inline long IniRead(const char* section, const char* key, long def) {
		return inifile_->GetLongValue(section, key, def);
	}

	inline int IniRead(const char* section, const char* key, int def) {
		return inifile_->GetLongValue(section, key, def);
	}

	// Retrieve a double value of the key in the section. 
	// Returns def if no value was found.
	inline double IniRead(const char* section, const char* key, double def) {
		return inifile_->GetDoubleValue(section, key, def);
	}

	// Add or update a wstring value in the given section / key combination.
	inline void IniWrite(const char* section, const char* key, const char* value) {
		inifile_->SetValue(section, key, value);
	}

	// Add or update a bool value in the given section / key combination.
	inline void IniWrite(const char* section, const char* key, bool value) {
		inifile_->SetBoolValue(section, key, value);
	}

	// Add or update a long value in the given section / key combination.
	inline void IniWrite(const char* section, const char* key, long value) {
		inifile_->SetLongValue(section, key, value);
	}

	inline void IniWrite(const char* section, const char* key, int value) {
		inifile_->SetLongValue(section, key, value);
	}

	// Add or update a double value in the given section / key combination.
	inline void IniWrite(const char* section, const char* key, double value) {
		inifile_->SetDoubleValue(section, key, value);
	}

	// returns a list of all the section names
	// note: do not rely on the order
	std::list<std::string> IniReadSections();

	// deletes a section from the ini file
	inline void IniDeleteSection(const char* section) {
		inifile_->Delete(section, NULL);
	}
};
