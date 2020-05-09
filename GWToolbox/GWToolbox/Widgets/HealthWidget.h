#pragma once

#include "ToolboxWidget.h"
#include <Color.h>

class HealthWidget : public ToolboxWidget {
	HealthWidget() {};
	~HealthWidget();
public:
	static HealthWidget& Instance() {
		static HealthWidget instance;
		return instance;
	}

	const char* Name() const override { return "Health"; }

    void LoadSettings(CSimpleIni *ini) override;
	void SaveSettings(CSimpleIni *ini) override;
    void DrawSettingInternal() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void ClearThresholds();

    bool click_to_print_health = false;

    std::wstring agent_name_ping;
	bool hide_in_outpost = false;

	bool thresholds_changed = false;
private:
	class Threshold {
		private:
			static unsigned int cur_ui_id;
		public:
			enum class Operation {
				None,
				MoveUp,
				MoveDown,
				Delete,
			};

			Threshold(CSimpleIni* ini, const char* section);
			Threshold(const char* _name, Color _color, int _value);

			bool DrawHeader();
			bool DrawSettings(Operation& op);
			void SaveSettings(CSimpleIni* ini, const char* section);

			const unsigned int ui_id = 0;
			size_t index = 0;

			bool active = true;
			char name[128] = "";
			int modelId = 0;
			int skillId = 0;
			int mapId = 0;

			int value = 0;
			Color color = 0xFFFFFFFF;
	};

	std::vector<Threshold*> thresholds;
	CSimpleIni* inifile = nullptr;
};
