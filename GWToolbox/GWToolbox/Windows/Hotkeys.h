#pragma once

#include <string>
#include <Defines.h>

#include <GWCA\GWCA.h>
#include <GWCA\Constants\Skills.h>
#include <GWCA\Managers\MapMgr.h>
#include <SimpleIni.h>

#include "Key.h"

// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey {
public:
	enum Op {
		Op_None,
		Op_MoveUp,
		Op_MoveDown,
		Op_Delete,
		Op_BlockInput,
	};

	static bool show_active_in_header;
	static bool show_run_in_header;
	static bool hotkeys_changed;

	static TBHotkey* HotkeyFactory(CSimpleIni* ini, const char* section);

	bool pressed = false;	// if the key has been pressed
	bool active = true;		// if the hotkey is enabled/active

	long hotkey = 0;
	long modifier = 0;

	// Create hotkey, load from file if 'ini' is not null
	TBHotkey(CSimpleIni* ini, const char* section);

	virtual void Save(CSimpleIni* ini, const char* section) const;

	void Draw(Op* op);

	virtual const char* Name() const = 0;
	virtual void Draw() = 0;
	virtual void Description(char* buf, int bufsz) const = 0;
	virtual void Execute() = 0;

protected:
	inline bool isLoading() const { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading; }
	inline bool isExplorable() const { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable; }
	inline bool isOutpost() const { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost; }

	const unsigned int ui_id = 0;	// an internal id to ensure interface consistency

private:
	static unsigned int cur_ui_id;

};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
private:
	char message[139];
	char channel = '/';

public:
	inline static const char* IniSection() { return "SendChat"; }
	const char* Name() const override { return IniSection(); }

	HotkeySendChat(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
private:
	UINT item_id = 0;
	char name[140];

public:
	static const char* IniSection() { return "UseItem"; }
	const char* Name() const override { return IniSection(); }

	HotkeyUseItem(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
	enum SkillIndex {
		Recall,
		UA,
		HolyVeil,
		Other
	};
	static bool GetText(void* data, int idx, const char** out_text);
	SkillIndex GetIndex() const;

public:
	GW::Constants::SkillID id;

	static const char* IniSection() { return "DropUseBuff"; }
	const char* Name() const override { return IniSection(); }

	HotkeyDropUseBuff(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to toggle a toolbox function
class HotkeyToggle : public TBHotkey {
	const int n_targets = 3;
	enum Toggle {
		Clicker,
		Pcons,
		CoinDrop
	};
	static bool GetText(void*, int idx, const char** out_text);

public:
	Toggle target; // the thing to toggle

	static const char* IniSection() { return "Toggle"; }
	const char* Name() const override { return IniSection(); }

	HotkeyToggle(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

class HotkeyAction : public TBHotkey {
	const int n_actions = 4;
	enum Action {
		OpenXunlaiChest,
		OpenLockedChest,
		DropGoldCoin,
		ReapplyLBTitle
	};
	static bool GetText(void*, int idx, const char** out_text);

public:
	Action action;

	static const char* IniSection() { return "Action"; }
	const char* Name() const override { return IniSection(); }

	HotkeyAction(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
public:
	UINT id = 0;
	char name[140];

	static const char* IniSection() { return "Target"; }
	const char* Name() const override { return IniSection(); }

	HotkeyTarget(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
public:
	float x = 0.0;
	float y = 0.0;
	float range = 0.0;
	DWORD mapid = 0;
	char name[140];

	static const char* IniSection() { return "Move"; }
	const char* Name() const override { return IniSection(); }

	HotkeyMove(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

class HotkeyDialog : public TBHotkey {
public:
	UINT id = 0;
	char name[140];

	static const char* IniSection() { return "Dialog"; }
	const char* Name() const override { return IniSection(); }

	HotkeyDialog(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

class HotkeyPingBuild : public TBHotkey {
	static bool GetText(void*, int idx, const char** out_text);

public:
	int index = 0;

	static const char* IniSection() { return "PingBuild"; }
	const char* Name() const override { return IniSection(); }

	HotkeyPingBuild(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

class HotkeyHeroTeamBuild : public TBHotkey {
	static bool GetText(void*, int idx, const char** out_text);

public:
	int index = 0;

	static const char* IniSection() { return "HeroTeamBuild"; }
	const char* Name() const override { return IniSection(); }

	HotkeyHeroTeamBuild(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

#ifdef ENABLE_LUA
class HotkeyLUACmd : public TBHotkey {

public:
	/*
	char name[60] = "";
	char command[512] = "";
	*/
	mutable char cmd[0x200];
	bool editopen;

	static const char* IniSection() { return "LUACmd"; }
	const char* Name() const override { return IniSection(); }

	HotkeyLUACmd(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};
#endif