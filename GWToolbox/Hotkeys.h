#pragma once

#include <string>

#include "../include/OSHGui/OSHGui.hpp"
#include "../API/APIMain.h"

using namespace std;


// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey : public OSHGui::Panel {
public:
	static const int WIDTH = 250;
	static const int HEIGHT = 45;
	static const int CONTROL_WIDTH = 55;
	static const int CONTROL_HEIGHT = 22;
	static const int VSPACE = 2;
	static const int HSPACE = 20;
	static const int ITEM_X = CONTROL_WIDTH + HSPACE;
	static const int ITEM_Y = CONTROL_HEIGHT + VSPACE;

protected:
	bool pressed_;
	bool active_;
	long key_;
	const wstring ini_section_;

	inline bool isLoading() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

	TBHotkey(string name, long key, bool active, wstring ini_section);

	inline void set_active(bool active) { active_ = active; }

public:
	inline static const wchar_t* IniKeyActive() { return L"active"; }
	inline static const wchar_t* IniKeyHotkey() { return L"hotkey"; }

	inline bool pressed() { return pressed_; }
	inline void set_pressed(bool pressed) { pressed_ = pressed; }
	inline long key() { return key_; }
	virtual void exec() = 0;
};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	wstring msg_;
	wchar_t channel_;

public:
	HotkeySendChat(long key, bool active, wstring ini_section, 
		wstring _msg, wchar_t _channel);

	inline static const wchar_t* IniSection() { return L"SendChat"; }
	inline static const wchar_t* IniKeyMsg() { return L"msg"; }
	inline static const wchar_t* IniKeyChannel() { return L"channel"; }
	void exec();

	int ChannelToIndex(wchar_t channel);
	wchar_t IndexToChannel(int index);
	inline void set_channel(wchar_t channel) { channel_ = channel; }
	inline void set_msg(wstring msg) { msg_ = msg; }
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
private:
	const UINT ItemID;
	const wstring ItemName;

public:
	HotkeyUseItem(long key, bool active, wstring ini_section, UINT _ID, wstring _name) :
		TBHotkey("Use Item", key, active, ini_section), ItemID(_ID), ItemName(_name) {
	}

	static const wchar_t* IniSection() { return L"UseItem"; }
	static const wchar_t* IniItemIDKey() { return L"ItemID"; }
	static const wchar_t* IniItemNameKey() { return L"ItemName"; }
	void exec();
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
private:
	const UINT SkillID;

public:
	HotkeyDropUseBuff(long key, bool active, wstring ini_section, UINT _skillID) :
		TBHotkey("Use/Drop Buff", key, active, ini_section), SkillID(_skillID) {
	}

	static const wchar_t* IniSection() { return L"DropUseBuff"; }
	static const wchar_t* IniSkillIDKey() { return L"SkillID"; }
	void exec();
};

// hotkey to invoke a generic function
// needs more work
// in theory its for toggles
class HotkeyToggle : public TBHotkey {
private:
	const UINT ToggleID; // some kind of ID (yet to be defined) MUST BE NON-ZERO

public:
	HotkeyToggle(long key, bool active, wstring ini_section, UINT _toggleID) :
		TBHotkey("Toggle", key, active, ini_section), ToggleID(_toggleID) {
	}

	static const wchar_t* IniSection() { return L"Toggle"; }
	static const wchar_t* IniToggleIDKey() { return L"ToggleID"; }
	void exec();
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
private:
	const UINT TargetID;

public:
	HotkeyTarget(long key, bool active, wstring ini_section, UINT _targetID) :
		TBHotkey("Target", key, active, ini_section), TargetID(_targetID) {
	}

	static const wchar_t* IniSection() { return L"Target"; }
	static const wchar_t* IniTargetID() { return L"TargetID"; }
	void exec();
};

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
private:
	const float x;
	const float y;

public:
	HotkeyMove(long key, bool active, wstring ini_section, float _x, float _y) :
		TBHotkey("Move", key, active, ini_section), x(_x), y(_y) {
	}

	static const wchar_t* IniSection() { return L"Move"; }
	static const wchar_t* IniXKey() { return L"x"; }
	static const wchar_t* IniYKey() { return L"y"; }

	void exec();
};

class HotkeyDialog : public TBHotkey {
private:
	UINT DialogID;

public:
	HotkeyDialog(long key, bool active, wstring ini_section, UINT _id) :
		TBHotkey("Dialog", key, active, ini_section), DialogID(_id) {
	}

	static const wchar_t* IniSection() { return L"Dialog"; }
	static const wchar_t* IniDialogIDKey() { return L"DialogID"; }

	void exec();
};

class HotkeyPingBuild : public TBHotkey {
private:
	UINT BuildIdx;

public:
	HotkeyPingBuild(long key, bool active, wstring ini_section, UINT _idx) :
		TBHotkey("Ping Build", key, active, ini_section), BuildIdx(_idx) {
	}

	static const wchar_t* IniSection() { return L"PingBuild"; }
	static const wchar_t* IniBuildIdxKey() { return L"BuildIndex"; }

	void exec();
};
