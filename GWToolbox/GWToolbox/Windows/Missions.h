#pragma once

#include <string>
#include <Defines.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Constants\Skills.h>
#include <GWCA\Managers\MapMgr.h>
#include <SimpleIni.h>

// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBMission {
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

	static TBMission* HotkeyFactory(CSimpleIni* ini, const char* section);

	bool pressed = false;	// if the key has been pressed
	bool active = true;		// if the hotkey is enabled/active

	long hotkey = 0;
	long modifier = 0;

	// Create hotkey, load from file if 'ini' is not null
	TBMission(CSimpleIni* ini, const char* section);

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
class MissionSendChat : public TBMission {
private:
	char message[139];
	char channel = '/';

public:
	inline static const char* IniSection() { return "SendChat"; }
	const char* Name() const override { return IniSection(); }

	MissionSendChat(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class MissionUseItem : public TBMission {
private:
	UINT item_id = 0;
	char name[140];

public:
	static const char* IniSection() { return "UseItem"; }
	const char* Name() const override { return IniSection(); }

	MissionUseItem(CSimpleIni* ini, const char* section);

	void Save(CSimpleIni* ini, const char* section) const override;

	void Draw() override;
	void Description(char* buf, int bufsz) const;
	void Execute() override;
};
