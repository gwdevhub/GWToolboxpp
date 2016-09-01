#pragma once

#include <string>
#include <list>

#include "SimpleIni.h"

namespace Config {
	extern std::wstring inifile_path_;		// the full ini file path
	extern CSimpleIni* inifile_;		// SimpleIni object

	void Initialize();
	void Destroy();

	inline void Save() {
		inifile_->SaveFile(inifile_path_.c_str());
	}

	// Retrieve a wstring value of the key in the section. 
	// Returns def if no value was found.
	inline const wchar_t* IniRead(const wchar_t* section, 
		const wchar_t* key, const wchar_t* def) {
		return inifile_->GetValue(section, key, def);
	}

	// Retrieve a bool value of the key in the section. 
	// Returns def if no value was found.
	inline bool IniReadBool(const wchar_t* section,
		const wchar_t* key, bool def) {
		return inifile_->GetBoolValue(section, key, def);
	}

	// Retrieve a long value of the key in the section. 
	// Returns def if no value was found.
	inline long IniReadLong(const wchar_t* section,
		const wchar_t* key, long def) {
		return inifile_->GetLongValue(section, key, def);
	}

	// Retrieve a double value of the key in the section. 
	// Returns def if no value was found.
	inline double IniReadDouble(const wchar_t* section,
		const wchar_t* key, double def) {
		return inifile_->GetDoubleValue(section, key, def);
	}

	// Add or update a wstring value in the given section / key combination.
	inline void IniWrite(const wchar_t* section,
		const wchar_t* key, const wchar_t* value) {
		inifile_->SetValue(section, key, value);
	}

	// Add or update a bool value in the given section / key combination.
	inline void IniWriteBool(const wchar_t* section,
		const wchar_t* key, bool value) {
		inifile_->SetBoolValue(section, key, value);
	}

	// Add or update a long value in the given section / key combination.
	inline void IniWriteLong(const wchar_t* section, 
		const wchar_t* key, long value) {
		inifile_->SetLongValue(section, key, value);
	}

	// Add or update a double value in the given section / key combination.
	inline void IniWriteDouble(const wchar_t* section, 
		const wchar_t* key, double value) {
		inifile_->SetDoubleValue(section, key, value);
	}

	// returns a list of all the section names
	// note: do not rely on the order
	std::list<std::wstring> IniReadSections();

	// deletes a section from the ini file
	inline void IniDeleteSection(const wchar_t* section) {
		inifile_->Delete(section, NULL);
	}
};
