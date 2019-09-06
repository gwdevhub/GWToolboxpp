#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>

#include "Timer.h"
#include "GuiUtils.h"

class SkillListingWindow : public ToolboxWindow {
private:
    SkillListingWindow() {};
    ~SkillListingWindow() {};

    void AsyncDecodeStr(wchar_t* buffer, size_t n, uint32_t enc_num);
public:
    static SkillListingWindow& Instance() {
        static SkillListingWindow instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars Skill List"; }
    void Update(float delta) override;

    void Initialize() override;

};
