#pragma once 

#include <vector>
#include <functional>
#include <algorithm>
#include <Windows.h>
#include <string>

#include "../API/APIMain.h"
#include "logger.h"

using namespace GWAPI;
using namespace std;

// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey {
public:
	bool pressed;
	bool active;
	const long key;
protected:
	inline bool isLoading() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

public:
	TBHotkey(long _key, bool _active) :
		pressed(false), active(_active), key(_key) {
	}

	virtual void exec() = 0;
};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	const wstring msg;
	const wchar_t channel;

public:
	HotkeySendChat(long key, bool active, wstring _msg, wchar_t _channel) :
		TBHotkey(key, active), msg(_msg), channel(_channel) {}

	static const wchar_t* iniSection() { return L"SendChat"; }
	static const wchar_t* iniKeyMsg() { return L"msg"; }
	static const wchar_t* iniKeyChannel() { return L"channel"; }
	void exec();
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
private:
	const UINT ItemID;
	const wstring ItemName;

public:
	HotkeyUseItem(long key, bool active, UINT _ID, wstring _name) :
		TBHotkey(key, active), ItemID(_ID), ItemName(_name) {}

	static const wchar_t* iniSection() { return L"UseItem"; }
	static const wchar_t* iniItemIDKey() { return L"ItemID"; }
	static const wchar_t* iniItemNameKey() { return L"ItemName"; }
	void exec();
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
private:
	const UINT SkillID;

public:
	HotkeyDropUseBuff(long key, bool active, UINT _skillID) :
		TBHotkey(key, active), SkillID(_skillID) {}

	static const wchar_t* iniSection() { return L"DropUseBuff"; }
	static const wchar_t* iniSkillIDKey() { return L"SkillID"; }
	void exec();
};

// hotkey to invoke a generic function
// needs more work
// in theory its for toggles
class HotkeyToggle : public TBHotkey {
private:
	const UINT ToggleID; // some kind of ID (yet to be defined) MUST BE NON-ZERO

public:
	HotkeyToggle(long key, bool active, UINT _toggleID) :
		TBHotkey(key, active), ToggleID(_toggleID) {}

	static const wchar_t* iniSection() { return L"Toggle"; }
	static const wchar_t* iniToggleIDKey() { return L"ToggleID"; }
	void exec();
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
private:
	const UINT TargetID;

public:
	HotkeyTarget(long key, bool active, UINT _targetID) :
		TBHotkey(key, active), TargetID(_targetID) {}

	static const wchar_t* iniSection() { return L"Target"; }
	static const wchar_t* iniTargetID() { return L"TargetID"; }
	void exec();
};

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
private:
	const float x;
	const float y;

public:
	HotkeyMove(long key, bool active, float _x, float _y) :
		TBHotkey(key, active), x(_x), y(_y) {}

	static const wchar_t* iniSection() { return L"Move"; }
	static const wchar_t* iniXKey() { return L"x"; }
	static const wchar_t* iniYKey() { return L"y"; }

	void exec();
};

class HotkeyDialog : public TBHotkey {
private:
	UINT DialogID;

public:
	HotkeyDialog(long key, bool active, UINT _id) :
		TBHotkey(key, active), DialogID(_id) {}

	static const wchar_t* iniSection() { return L"Dialog"; }
	static const wchar_t* iniDialogIDKey() { return L"DialogID"; }

	void exec();
};

class HotkeyPingBuild : public TBHotkey {
private:
	UINT BuildIdx;

public:
	HotkeyPingBuild(long key, bool active, UINT _idx) :
		TBHotkey(key, active), BuildIdx(_idx) {}

	static const wchar_t* iniSection() { return L"PingBuild"; }
	static const wchar_t* iniBuildIdxKey() { return L"BuildIndex"; }

	void exec();
};


// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeyMgr {
private:
	std::vector<TBHotkey*> hotkeys;

public:
	HotkeyMgr()
		: hotkeys(std::vector<TBHotkey*>()) {
	}
	
	// Adds a new hotkey
	// Returns the identifier to the hotkey, required to delete it later
	void add(TBHotkey* h) {
		hotkeys.push_back(h); 
	}

	// Removes an hotkey. 
	// Uses the ID returned by add.
	void remove(TBHotkey* hk) {
		// TODO
	}

	bool processMessage(LPMSG msg);
};
