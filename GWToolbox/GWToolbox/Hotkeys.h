#pragma once

#include <string>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\GWCA.h>
#include <GWCA\MapMgr.h>

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
	static const int HOTKEY_X = 0;
	static const int HOTKEY_Y = LINE_HEIGHT + VSPACE;
	static const int ITEM_X = 0;
	static const int ITEM_Y = 0;
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
	bool isLoading() { 
		return GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Loading; }
	bool isExplorable() { 
		return GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Explorable; }
	bool isOutpost() { 
		return GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Outpost; }

	TBHotkey(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section);

	virtual void PopulateGeometry() override;

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
	virtual wstring GetDescription() = 0;
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
	
	void exec() override;
	wstring GetDescription() override;
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
	static const wchar_t* IniKeyItemID() { return L"ItemID"; }
	static const wchar_t* IniKeyItemName() { return L"ItemName"; }

	void exec() override;
	wstring GetDescription() override;
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
private:
	GwConstants::SkillID id_;
	OSHGui::ComboBox* combo_;

	GwConstants::SkillID IndexToSkillID(int index);
	inline void set_id(GwConstants::SkillID id) { id_ = id; }
public:
	HotkeyDropUseBuff(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, GwConstants::SkillID skill_id);

	static const wchar_t* IniSection() { return L"DropUseBuff"; }
	static const wchar_t* IniKeySkillID() { return L"SkillID"; }
	void exec() override;
	wstring GetDescription() override;
};

// hotkey to toggle a toolbox function
class HotkeyToggle : public TBHotkey {
public:
	enum Toggle {
		Clicker,
		Pcons,
		CoinDrop
	};

private:
	Toggle target_; // the thing to toggle

	inline void set_target(Toggle target) { target_ = target; }

public:
	HotkeyToggle(OSHGui::Key key, OSHGui::Key modifier, bool active, 
		wstring ini_section, long toggle_id);

	static const wchar_t* IniSection() { return L"Toggle"; }
	static const wchar_t* IniKeyToggleID() { return L"ToggleID"; }

	void exec() override;
	wstring GetDescription() override;
};

class HotkeyAction : public TBHotkey {
public:
	enum Action {
		OpenXunlaiChest,
		OpenLockedChest,
		DropGoldCoin
	};
private:
	Action action_;
	inline void set_action(Action action) { action_ = action; }

public:
	HotkeyAction(OSHGui::Key key, OSHGui::Key modifier, bool active,
		wstring ini_section, long action_id);

	static const wchar_t* IniSection() { return L"Action"; }
	static const wchar_t* IniKeyActionID() { return L"ActionID"; }

	void exec() override;
	wstring GetDescription() override;
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
	static const wchar_t* IniKeyTargetID() { return L"TargetID"; }
	static const wchar_t* IniKeyTargetName() { return L"TargetName"; }

	void exec() override;
	wstring GetDescription() override;
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
	static const wchar_t* IniKeyX() { return L"x"; }
	static const wchar_t* IniKeyY() { return L"y"; }
	static const wchar_t* IniKeyName() { return L"name"; }

	void exec() override;
	wstring GetDescription() override;
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
	static const wchar_t* IniKeyDialogID() { return L"DialogID"; }
	static const wchar_t* IniKeyDialogName() { return L"DialogName"; }
	
	void exec() override;
	wstring GetDescription() override;
};

class HotkeyPingBuild : public TBHotkey {
private:
	long index_;

	inline void set_index(long index) { index_ = index; }
public:
	HotkeyPingBuild(OSHGui::Key key, OSHGui::Key modifier, bool active,
		wstring ini_section, long index);

	static const wchar_t* IniSection() { return L"PingBuild"; }
	static const wchar_t* IniKeyBuildIndex() { return L"BuildIndex"; }

	void exec() override;
	wstring GetDescription() override;
};
