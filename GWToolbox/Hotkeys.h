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
	static const int LABEL_Y = ITEM_Y + 5;

	

protected:
	bool pressed_;
	bool active_;
	OSHGui::Key key_;
	OSHGui::Key modifier_;
	const wstring ini_section_;

	inline void set_active(bool active) { active_ = active; }
	inline void set_key(OSHGui::Key key) { key_ = key; }
	inline void set_modifier(OSHGui::Key modifier) { modifier_ = modifier; }
	inline bool isLoading() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost() { return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

	TBHotkey(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section);

public:
	inline static const wchar_t* IniKeyActive() { return L"active"; }
	inline static const wchar_t* IniKeyHotkey() { return L"hotkey"; }
	inline static const wchar_t* IniKeyModifier() { return L"modifier"; }

	inline wstring ini_section() { return ini_section_; }
	inline bool active() { return active_; }
	inline bool pressed() { return pressed_; }
	inline void set_pressed(bool pressed) { pressed_ = pressed; }
	inline OSHGui::Key key() { return key_; }
	inline OSHGui::Key modifier() { return modifier_; }
	virtual void exec() = 0;
	virtual string GetDescription() = 0;
};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	wstring msg_;
	wchar_t channel_;

	inline void set_channel(wchar_t channel) { channel_ = channel; }
	inline void set_msg(wstring msg) { msg_ = msg; }
	int ChannelToIndex(wchar_t channel);
	wchar_t IndexToChannel(int index);

public:
	HotkeySendChat(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, wstring _msg, wchar_t _channel);

	inline static const wchar_t* IniSection() { return L"SendChat"; }
	inline static const wchar_t* IniKeyMsg() { return L"msg"; }
	inline static const wchar_t* IniKeyChannel() { return L"channel"; }
	
	void exec();
	string GetDescription() override { return string("Send Chat"); }
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
private:
	UINT item_id_;
	wstring item_name_;

	inline void set_item_id(UINT ID) { item_id_ = ID; }
	inline void set_item_name(wstring name) { item_name_ = name; }

public:
	HotkeyUseItem(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT item_id_, wstring item_name_);

	static const wchar_t* IniSection() { return L"UseItem"; }
	static const wchar_t* IniItemIDKey() { return L"ItemID"; }
	static const wchar_t* IniItemNameKey() { return L"ItemName"; }

	void exec();
	string GetDescription() override { return string("Use Item"); }
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
private:
	UINT id_;
	OSHGui::ComboBox* combo_;

	UINT IndexToSkillID(int index);
	inline void set_id(UINT id) { id_ = id; }
public:
	HotkeyDropUseBuff(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT skill_id);

	static const wchar_t* IniSection() { return L"DropUseBuff"; }
	static const wchar_t* IniSkillIDKey() { return L"SkillID"; }
	void exec();
	string GetDescription() override { return string("Drop/Use Buff"); }
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

	inline void set_target(Toggle target) { target_ = target; }

public:
	HotkeyToggle(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, int toggle_id);

	static const wchar_t* IniSection() { return L"Toggle"; }
	static const wchar_t* IniToggleIDKey() { return L"ToggleID"; }

	void exec();
	string GetDescription() override { return string("Toggle"); }
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
private:
	UINT id_;
	wstring name_;

	inline void set_id(UINT targetID) { id_ = targetID; }
	inline void set_name(wstring name) { name_ = name; }

public:
	HotkeyTarget(OSHGui::Key key, OSHGui::Key modifier, bool active,
		wstring ini_section, UINT id, wstring name);

	static const wchar_t* IniSection() { return L"Target"; }
	static const wchar_t* IniTargetIDKey() { return L"TargetID"; }
	static const wchar_t* IniTargetNameKey() { return L"TargetName"; }

	void exec();
	string GetDescription() override { return string("Target"); }
};

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
private:
	float x_;
	float y_;
	wstring name_;

	inline void set_x(float x) { x_ = x; }
	inline void set_y(float y) { y_ = y; }
	inline void set_name(wstring name) { name_ = name; }

public:
	HotkeyMove(OSHGui::Key key, OSHGui::Key modifier, bool active,
		wstring ini_section, float x, float y, wstring name);

	static const wchar_t* IniSection() { return L"Move"; }
	static const wchar_t* IniXKey() { return L"x"; }
	static const wchar_t* IniYKey() { return L"y"; }
	static const wchar_t* IniNameKey() { return L"name"; }

	void exec();
	string GetDescription() override { return string("Move"); }
};

class HotkeyDialog : public TBHotkey {
private:
	UINT id_;
	wstring name_;

	inline void set_id(UINT id) { id_ = id; }
	inline void set_name(wstring name) { name_ = name; }

public:
	HotkeyDialog(OSHGui::Key key, OSHGui::Key modifier, bool active,
		wstring ini_section, UINT dialogID, wstring dialog_name);

	static const wchar_t* IniSection() { return L"Dialog"; }
	static const wchar_t* IniDialogIDKey() { return L"DialogID"; }
	static const wchar_t* IniDialogNameKey() { return L"DialogName"; }
	
	void exec();
	string GetDescription() override { return string("Dialog"); }
};

class HotkeyPingBuild : public TBHotkey {
private:
	UINT build_index_;

public:
	HotkeyPingBuild(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, UINT _idx) :
		TBHotkey(key, modifier, active, ini_section), build_index_(_idx) {
		// TODO (maybe or maybe just get rid of it)
	}

	static const wchar_t* IniSection() { return L"PingBuild"; }
	static const wchar_t* IniBuildIdxKey() { return L"BuildIndex"; }

	void exec();
	string GetDescription() override { return string("Ping Build"); }
};
