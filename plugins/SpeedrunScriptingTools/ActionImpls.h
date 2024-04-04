#pragma once

#include <Action.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/ItemIDs.h>
#include <GWCA/Constants/Constants.h>

#include <chrono>

namespace GW {
    struct AgentLiving;
}

class MoveToAction : public Action {
public:
    MoveToAction(GW::GamePos pos = {}, float accuracy = GW::Constants::Range::Adjacent) : pos{pos}, accuracy{accuracy} {}
    ActionType type() const final { return ActionType::MoveTo; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    GW::GamePos pos;
    float accuracy;
};

class CastOnSelfAction : public Action {
public:
    CastOnSelfAction(GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill) : id{id} {}
    ActionType type() const final { return ActionType::CastOnSelf; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID id;
};

class CastOnTargetAction : public Action {
public:
    CastOnTargetAction(GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill) : id{id} {}
    ActionType type() const final { return ActionType::CastOnTarget; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID id;
};

class UseItemAction : public Action {
public:
    UseItemAction(uint32_t id = 0) : id{id} {}
    ActionType type() const final { return ActionType::UseItem; }
    void initialAction() final;
    void drawSettings() final;

private:
    uint32_t id;
};

class SendDialogAction : public Action {
public:
    SendDialogAction(uint32_t id = 0) : id{id} {}
    ActionType type() const final { return ActionType::SendDialog; }
    void initialAction() final;
    void drawSettings() final;

private:
    uint32_t id;
};

class GoToNpcAction : public Action {
public:
    GoToNpcAction(uint32_t id = 0, float accuracy = GW::Constants::Range::Adjacent) : id{id}, accuracy{accuracy} {}
    ActionType type() const final { return ActionType::GoToNpc; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    uint32_t id;
    float accuracy;
    const GW::AgentLiving* npc = nullptr;
};

class WaitAction : public Action {
public:
    WaitAction(int timeInMs = 1000) : waitTime{timeInMs} {}
    ActionType type() const final { return ActionType::Wait; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    int waitTime;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

class SendChatAction : public Action {
public:
    enum class Channel {
        All,
        Guild,
        Team,
        Trade,
        Alliance,
        Whisper,
        Emote,
    };
    SendChatAction(Channel channel = Channel::Emote, std::string_view message = "") : channel{channel} { strcpy_s(buf, message.data()); }
    ActionType type() const final { return ActionType::SendChat; }
    void initialAction() final;
    void drawSettings() final;

private:
    Channel channel;
    char buf[100] = "";
};
