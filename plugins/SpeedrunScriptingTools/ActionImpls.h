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
    MoveToAction(GW::GamePos pos, float squareAccuracy = GW::Constants::SqrRange::Adjacent) : pos{pos}, squareAccuracy{squareAccuracy} {}
    ActionType type() const final { return ActionType::MoveTo; }
    void initialAction() final;
    //void finalAction() const final;
    bool isComplete() const final;

private:
    GW::GamePos pos;
    float squareAccuracy;
};

class CastOnSelfAction : public Action {
public:
    CastOnSelfAction(GW::Constants::SkillID id) : id{id} {}
    ActionType type() const final { return ActionType::CastOnSelf; }
    void initialAction() final;
    bool isComplete() const final;

private:
    GW::Constants::SkillID id;
};

class CastOnTargetAction : public Action {
public:
    CastOnTargetAction(GW::Constants::SkillID id) : id{id} {}
    ActionType type() const final { return ActionType::CastOnTarget; }
    void initialAction() final;
    bool isComplete() const final;

private:
    GW::Constants::SkillID id;
};

class UseItemAction : public Action {
public:
    UseItemAction(uint32_t id) : id{id} {}
    ActionType type() const final { return ActionType::UseItem; }
    void initialAction() final;

private:
    uint32_t id;
};

class SendDialogAction : public Action {
public:
    SendDialogAction(uint32_t id) : id{id} {}
    ActionType type() const final { return ActionType::SendDialog; }
    void initialAction() final;

private:
    uint32_t id;
};

class GoToNpcAction : public Action {
public:
    GoToNpcAction(uint32_t id, float squareAccuracy = GW::Constants::SqrRange::Adjacent) : id{id}, squareAccuracy{squareAccuracy} {}
    ActionType type() const final { return ActionType::GoToNpc; }
    void initialAction() final;
    bool isComplete() const final;

private:
    uint32_t id;
    float squareAccuracy;
    const GW::AgentLiving* npc = nullptr;
};

class WaitAction : public Action {
public:
    WaitAction(uint32_t timeInMs) : waitTime{timeInMs} {}
    ActionType type() const final { return ActionType::Wait; }
    void initialAction() final;
    bool isComplete() const final;

private:
    uint32_t waitTime;
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
    SendChatAction(Channel channel, std::string_view message) : channel{channel}, message{message} {}
    ActionType type() const final { return ActionType::SendChat; }
    void initialAction() final;

private:
    Channel channel;
    std::string_view message;
};
