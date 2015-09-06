#pragma once

#include <string>

#include "../include/OSHGui/OSHGui.hpp"
#include "../API/APIMain.h"

using namespace std;


// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey : public OSHGui::Panel {
public:
	static const int LINE_HEIGHT = 27;
	static const int VSPACE = 2;
	static const int HSPACE = 10;
	static const int WIDTH = 250;
	static const int HEIGHT = LINE_HEIGHT * 2 + VSPACE * 3;
	static const int ITEM_X = 0;
	static const int ITEM_Y = LINE_HEIGHT + VSPACE;
	static const int LABEL_Y = ITEM_Y + 4;

protected:
	bool pressed_;
	bool active_;
	OSHGui::Key key_;
	OSHGui::Key modifier_;
	const wstring ini_section_;

	inline bool isLoading() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

	TBHotkey(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section);

	inline void set_active(bool active) { active_ = active; }

public:
	inline static const wchar_t* IniKeyActive() { return L"active"; }
	inline static const wchar_t* IniKeyHotkey() { return L"hotkey"; }
	inline static const wchar_t* IniKeyModifier() { return L"modifier"; }

	inline bool pressed() { return pressed_; }
	inline void set_pressed(bool pressed) { pressed_ = pressed; }
	inline OSHGui::Key key() { return key_; }
	inline OSHGui::Key modifier() { return modifier_; }
	inline void set_key(OSHGui::Key key) { key_ = key; }
	inline void set_modifier(OSHGui::Key modifier) { modifier_ = modifier; }
	virtual void exec() = 0;
};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	wstring msg_;
	wchar_t channel_;

public:
	HotkeySendChat(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, wstring _msg, wchar_t _channel);

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
	UINT item_id_;
	wstring item_name_;

public:
	HotkeyUseItem(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT item_id_, wstring item_name_);

	static const wchar_t* IniSection() { return L"UseItem"; }
	static const wchar_t* IniItemIDKey() { return L"ItemID"; }
	static const wchar_t* IniItemNameKey() { return L"ItemName"; }
	void exec();

	inline void set_item_id(UINT ID) { item_id_ = ID; }
	inline void set_item_name(wstring name) { item_name_ = name; }
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
private:
	UINT skillID_;
	OSHGui::ComboBox* combo_;

public:
	HotkeyDropUseBuff(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT _skillID);

	static const wchar_t* IniSection() { return L"DropUseBuff"; }
	static const wchar_t* IniSkillIDKey() { return L"SkillID"; }
	void exec();

	UINT IndexToSkillID(int index);
	inline void set_skillID(UINT skillID) { skillID_ = skillID; }
};

// hotkey to toggle a toolbox function
class HotkeyToggle : public TBHotkey {
public:
	enum Toggle {
		Clicker = 1,
		Pcons,
		CoinDrop,
		RuptBot
	};

private:
	Toggle target_; // the thing to toggle

public:
	HotkeyToggle(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, int toggle_id);

	static const wchar_t* IniSection() { return L"Toggle"; }
	static const wchar_t* IniToggleIDKey() { return L"ToggleID"; }
	void exec();

	inline void set_target(Toggle target) { target_ = target; }
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
private:
	const UINT TargetID;

public:
	HotkeyTarget(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT _targetID) :
		TBHotkey(key, modifier, active, ini_section), TargetID(_targetID) {
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
	HotkeyMove(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, float _x, float _y) :
		TBHotkey(key, modifier, active, ini_section), x(_x), y(_y) {
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
	HotkeyDialog(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT _id) :
		TBHotkey(key, modifier, active, ini_section), DialogID(_id) {
	}

	static const wchar_t* IniSection() { return L"Dialog"; }
	static const wchar_t* IniDialogIDKey() { return L"DialogID"; }

	void exec();
};

class HotkeyPingBuild : public TBHotkey {
private:
	UINT BuildIdx;

public:
	HotkeyPingBuild(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT _idx) :
		TBHotkey(key, modifier, active, ini_section), BuildIdx(_idx) {
	}

	static const wchar_t* IniSection() { return L"PingBuild"; }
	static const wchar_t* IniBuildIdxKey() { return L"BuildIndex"; }

	void exec();
};
