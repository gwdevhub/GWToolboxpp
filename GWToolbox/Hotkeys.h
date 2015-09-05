#pragma once

#include <string>

#include "../include/OSHGui/OSHGui.hpp"
#include "../API/APIMain.h"

using namespace std;


// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey : public OSHGui::Panel {
public:
	static const int WIDTH = 400;
	static const int HEIGHT = 60;

protected:
	//OSHGui::CheckBox* checkbox_;
	//OSHGui::Button* hotkey_button_;
	//OSHGui::Label* name_;
	//OSHGui::Button* run_button_;
	//OSHGui::Button* delete_button_;

	bool pressed_;
	bool active_;
	const long key_;

	inline bool isLoading() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

	TBHotkey(string name, long key, bool active);

	inline void set_active(bool active) { 
		active_ = active; 
		// TODO update ini
	}

public:

	inline bool pressed() { return pressed_; }
	inline void set_pressed(bool pressed) { pressed_ = pressed; }
	inline long key() { return key_; }
	virtual void exec() = 0;
};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	const wstring msg_;
	const wchar_t channel_;

public:
	HotkeySendChat(long key, bool active, wstring _msg, wchar_t _channel);

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
		TBHotkey("Use Item", key, active), ItemID(_ID), ItemName(_name) {
	}

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
		TBHotkey("Use/Drop Buff", key, active), SkillID(_skillID) {
	}

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
		TBHotkey("Toggle", key, active), ToggleID(_toggleID) {
	}

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
		TBHotkey("Target", key, active), TargetID(_targetID) {
	}

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
		TBHotkey("Move", key, active), x(_x), y(_y) {
	}

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
		TBHotkey("Dialog", key, active), DialogID(_id) {
	}

	static const wchar_t* iniSection() { return L"Dialog"; }
	static const wchar_t* iniDialogIDKey() { return L"DialogID"; }

	void exec();
};

class HotkeyPingBuild : public TBHotkey {
private:
	UINT BuildIdx;

public:
	HotkeyPingBuild(long key, bool active, UINT _idx) :
		TBHotkey("Ping Build", key, active), BuildIdx(_idx) {
	}

	static const wchar_t* iniSection() { return L"PingBuild"; }
	static const wchar_t* iniBuildIdxKey() { return L"BuildIndex"; }

	void exec();
};
