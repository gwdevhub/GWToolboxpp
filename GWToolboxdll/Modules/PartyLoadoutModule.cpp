#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Utils/TeamBuild.h>
#include <Utils/TeamBuildEncoder.h>

#include <Modules/PartyLoadoutModule.h>

namespace {
    using GW::SkillbarMgr::SkillTemplate;

    constexpr uint32_t kAttributeCount = 0x33;

    enum class PartyMemberType : uint8_t { Player = 0, Henchman = 1, Hero = 2 };
    struct PartyTemplateMember {
        PartyMemberType type{};
        uint8_t hero_id{};
        GW::HeroBehavior behavior{};
        SkillTemplate skill_template{};
    };
    struct PartyTemplate {
        uint32_t version{};
        uint32_t party_size{};
        PartyTemplateMember members[8]{};
    };

    // LSB-first bit reader over packed bytes, matching GW's template bit order.
    struct BitReader {
        const uint8_t* data = nullptr;
        size_t bit_len = 0;
        size_t pos = 0;

        bool CanRead(uint32_t count) const { return pos + count <= bit_len; }
        uint32_t Read(uint32_t count) {
            uint32_t value = 0;
            for (uint32_t i = 0; i < count && pos < bit_len; i++, pos++)
                value |= ((data[pos >> 3] >> (pos & 7)) & 1u) << i;
            return value;
        }
    };

    bool DecodeSkillTemplateBody(BitReader& reader, SkillTemplate& out) {
        out = {};
        if (!reader.CanRead(2)) return false;
        const uint32_t bits_per_prof = 2 * reader.Read(2) + 4;
        if (!reader.CanRead(2 * bits_per_prof + 8)) return false;
        out.primary = static_cast<GW::Constants::Profession>(reader.Read(bits_per_prof));
        out.secondary = static_cast<GW::Constants::Profession>(reader.Read(bits_per_prof));

        const uint32_t attrib_count = reader.Read(4);
        const uint32_t bits_per_attr = reader.Read(4) + 4;
        if (attrib_count >= _countof(out.attribute_ids)) return false;
        out.attributes_count = attrib_count;
        for (uint32_t i = 0; i < attrib_count; i++) {
            if (!reader.CanRead(bits_per_attr + 4)) return false;
            const uint32_t attrib_id = reader.Read(bits_per_attr);
            const uint32_t attrib_val = reader.Read(4);
            if (attrib_id >= kAttributeCount || attrib_val > 12) return false;
            out.attribute_ids[i] = static_cast<GW::Constants::Attribute>(attrib_id);
            out.attribute_values[i] = attrib_val;
        }

        if (!reader.CanRead(4)) return false;
        const uint32_t bits_per_skill = reader.Read(4) + 8;
        for (uint32_t i = 0; i < _countof(out.skills) && reader.CanRead(bits_per_skill); i++)
            out.skills[i] = static_cast<GW::Constants::SkillID>(reader.Read(bits_per_skill));
        return true;
    }

    bool DecodePartyMember(BitReader& reader, PartyTemplateMember& member) {
        member = {};
        if (!reader.CanRead(10)) return false;
        member.type = static_cast<PartyMemberType>(reader.Read(2));
        member.hero_id = static_cast<uint8_t>(reader.Read(6));
        member.behavior = static_cast<GW::HeroBehavior>(reader.Read(2));
        return DecodeSkillTemplateBody(reader, member.skill_template);
    }

    bool DecodePartyTemplate(BitReader& reader, PartyTemplate& out) {
        out = {};
        if (!reader.CanRead(20)) return false;
        if (reader.Read(4) != 15) return false;
        if (reader.Read(8) != 1) return false;
        out.version = reader.Read(4);
        const uint32_t party_size = reader.Read(4);
        if (party_size < 1 || party_size > _countof(out.members)) return false;
        out.party_size = party_size;
        for (uint32_t i = 0; i < party_size; i++) {
            if (!DecodePartyMember(reader, out.members[i])) return false;
        }
        return true;
    }

    bool TeamBuildToPartyTemplate(const TeamBuild& tbuild, PartyTemplate& out) {
        out = {};
        if (tbuild.builds.empty() || tbuild.builds.size() > _countof(out.members)) return false;
        out.party_size = static_cast<uint32_t>(tbuild.builds.size());
        for (size_t i = 0; i < tbuild.builds.size(); i++) {
            const auto& build = tbuild.builds[i];
            SkillTemplate tmpl{};
            if (!build.code.empty() && !GW::SkillbarMgr::DecodeSkillTemplate(tmpl, build.code.c_str())) return false;
            out.members[i].skill_template = tmpl;
            out.members[i].hero_id = static_cast<uint8_t>(build.hero_id);
            out.members[i].type = build.hero_id == GW::Constants::HeroID::NoHero ? PartyMemberType::Player : PartyMemberType::Hero;
            out.members[i].behavior = static_cast<GW::HeroBehavior>(build.behavior);
        }
        return true;
    }

    // Chain: built-in extended-template parser first, TeamBuildEncoder fallback second.
    bool ParseTemplateFromString(const wchar_t* text, PartyTemplate& out) {
        std::string narrow;
        for (size_t i = 0; text[i]; i++) narrow.push_back(static_cast<char>(text[i]));

        const auto bytes = TeamBuildEncoder::DaybreakToBytes(narrow);
        if (!bytes.empty()) {
            BitReader reader{ bytes.data(), bytes.size() * 8 };
            if (DecodePartyTemplate(reader, out) && out.party_size >= 1) return true;
        }
        TeamBuild tbuild;
        return TeamBuildEncoder::DaybreakToTeamBuild(narrow, tbuild) && TeamBuildToPartyTemplate(tbuild, out) && out.party_size >= 1;
    }

    bool ParseTemplateFromBytes(const uint8_t* data, uint32_t size, PartyTemplate& out) {
        BitReader reader{ data, static_cast<size_t>(size) * 8 };
        return DecodePartyTemplate(reader, out) && out.party_size >= 1;
    }

    uint32_t PartySignature(const PartyTemplate& party) {
        uint32_t sig = party.version * 2654435761u + party.party_size;
        for (uint32_t i = 0; i < party.party_size; i++) {
            const auto& m = party.members[i];
            sig = sig * 16777619u + (static_cast<uint32_t>(m.type) << 16 | m.hero_id << 8 | static_cast<uint32_t>(m.behavior));
        }
        return sig;
    }

    bool pending_party_template_valid = false;
    bool decode_header_ran = false;
    PartyTemplate pending_party_template{};

    bool last_party_template_valid = false;
    PartyTemplate last_party_template{};

    bool last_decoded_party_template_valid = false;
    PartyTemplate last_decoded_party_template{};

    std::unordered_map<uint32_t, PartyTemplate> party_template_cache;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> party_stack_state;
    uint32_t last_applied_party_signature = 0;

    bool GetCachedPartyTemplate(uint32_t template_id, PartyTemplate& out) {
        const auto it = party_template_cache.find(template_id);
        if (it == party_template_cache.end()) return false;
        out = it->second;
        return true;
    }

    struct GWReader { uint32_t fields[7]; };
    GWReader decode_header_snapshot{};

    GW::UI::UIInteractionCallback SkillFrameHandler_Func = nullptr;
    uint32_t pending_party_stack_item_flags = 0x300;

    struct PartyStackContext {
        uint32_t frame_id = 0;
        uint32_t item_flags = 0x300;
        uint32_t party_size = 0;
        PartyTemplate party{};
    };

    PartyStackContext* GetPartyStackContext(GW::UI::InteractionMessage* message) {
        return message && message->wParam ? *reinterpret_cast<PartyStackContext**>(message->wParam) : nullptr;
    }

    void PartyStackFrameHandler(GW::UI::InteractionMessage* message, void* wParam, void*) {
        if (!message) return;
        switch (message->message_id) {
        case GW::UI::UIMessage::kInitFrame: {
            if (!(message->wParam && !*message->wParam && SkillFrameHandler_Func)) return;
            auto* context = new PartyStackContext();
            context->frame_id = message->frame_id;
            context->item_flags = pending_party_stack_item_flags;
            *reinterpret_cast<PartyStackContext**>(message->wParam) = context;
            for (uint32_t i = 0; i < 8; i++) {
                const uint32_t child_id = GW::UI::CreateUIComponent(context->frame_id, context->item_flags, i, SkillFrameHandler_Func, nullptr, nullptr);
                if (auto* child = GW::UI::GetFrameById(child_id))
                    GW::UI::SetFrameVisible(child, false);
            }
            break;
        }
        case GW::UI::UIMessage::kDestroyFrame: {
            if (auto* context = GetPartyStackContext(message)) {
                delete context;
                *reinterpret_cast<PartyStackContext**>(message->wParam) = nullptr;
            }
            break;
        }
        case GW::UI::UIMessage::kFrameMessage_0x47: {
            auto* context = GetPartyStackContext(message);
            if (!(context && wParam)) break;
            context->party = *static_cast<PartyTemplate*>(wParam);
            context->party_size = std::min<uint32_t>(context->party.party_size, _countof(context->party.members));
            auto* container = GW::UI::GetFrameById(context->frame_id);
            for (uint32_t i = 0; i < _countof(context->party.members); i++) {
                auto* child = container ? GW::UI::GetChildFrame(container, i) : nullptr;
                if (!child) continue;
                const bool visible = i < context->party_size;
                GW::UI::SetFrameVisible(child, visible);
                if (visible)
                    GW::UI::SendFrameUIMessage(child, GW::UI::UIMessage::kFrameMessage_0x47, &context->party.members[i].skill_template);
            }
            break;
        }
        case GW::UI::UIMessage::kMeasureContent: {
            auto* context = GetPartyStackContext(message);
            auto* measure = static_cast<GW::UI::UIPacket::kMeasureContent*>(wParam);
            if (!(context && measure && measure->size_output)) break;
            auto* container = GW::UI::GetFrameById(context->frame_id);
            float rect[4] = { 0.0f, 0.0f, measure->max_width, measure->max_height };
            float out[2] = { 0.0f, 0.0f };
            for (uint32_t i = 0; i < context->party_size && container; i++) {
                if (auto* child = GW::UI::GetChildFrame(container, i))
                    GW::UI::SetFrameBounds(child, 0x114, rect, out);
            }
            measure->size_output[0] = out[0];
            measure->size_output[1] = out[1];
            break;
        }
        case GW::UI::UIMessage::kSetLayout: {
            auto* context = GetPartyStackContext(message);
            if (!(context && wParam)) break;
            auto* container = GW::UI::GetFrameById(context->frame_id);
            const float* payload = static_cast<float*>(wParam);
            float rect[4] = { 0.0f, 0.0f, payload[0], payload[1] };
            for (uint32_t i = context->party_size; i-- > 0 && container;) {
                if (auto* child = GW::UI::GetChildFrame(container, i))
                    GW::UI::SetFramePosition(child, 0x114, rect);
            }
            break;
        }
        default:
            break;
        }
    }

    uint32_t BuildPartyStack(uint32_t frame_id, const PartyTemplate& party) {
        if (!SkillFrameHandler_Func) return 0;
        auto* parent = GW::UI::GetFrameById(frame_id);
        if (auto* old_child = parent ? GW::UI::GetChildFrame(parent, 1) : nullptr)
            GW::UI::DestroyUIComponent(old_child);
        const uint32_t child_flags = 0x300 | 0x1000;
        pending_party_stack_item_flags = child_flags;
        const uint32_t stack_id = GW::UI::CreateUIComponent(frame_id, child_flags, 1, PartyStackFrameHandler, nullptr, nullptr);
        auto* stack_frame = stack_id ? GW::UI::GetFrameById(stack_id) : nullptr;
        if (!stack_frame) return 0;
        GW::UI::SendFrameUIMessage(stack_frame, GW::UI::UIMessage::kFrameMessage_0x47, const_cast<PartyTemplate*>(&party));
        return stack_id;
    }

    struct PendingPartyApply {
        PartyTemplate party;
        int ticks_waited = 0;
    };
    std::unique_ptr<PendingPartyApply> pending_party_apply;
    GW::HookEntry PartyApplyGameThread_Entry;

    void ApplyHeroBuildsAndBehaviors(const PartyTemplate& party) {
        auto* party_info = GW::PartyMgr::GetPartyInfo();
        if (!party_info) return;
        for (uint32_t i = 1; i < party.party_size; i++) {
            const auto& member = party.members[i];
            if (member.type != PartyMemberType::Hero) continue;
            uint32_t hero_agent_id = 0;
            for (const auto& hero : party_info->heroes) {
                if (hero.hero_id == static_cast<GW::Constants::HeroID>(member.hero_id)) {
                    hero_agent_id = hero.agent_id;
                    break;
                }
            }
            if (!hero_agent_id) continue;
            GW::SkillbarMgr::LoadSkillTemplate(hero_agent_id, const_cast<SkillTemplate&>(member.skill_template));
            GW::PartyMgr::SetHeroBehavior(hero_agent_id, member.behavior);
        }
    }

    void OnPartyApplyGameThreadTick(GW::HookStatus*) {
        if (!pending_party_apply) {
            GW::GameThread::RemoveGameThreadCallback(&PartyApplyGameThread_Entry);
            return;
        }
        pending_party_apply->ticks_waited++;

        uint32_t expected_heroes = 0;
        for (uint32_t i = 1; i < pending_party_apply->party.party_size; i++) {
            if (pending_party_apply->party.members[i].type == PartyMemberType::Hero)
                expected_heroes++;
        }
        auto* party_info = GW::PartyMgr::GetPartyInfo();
        const bool heroes_ready = party_info && party_info->heroes.size() >= expected_heroes;

        constexpr int MaxTicksToWait = 300;
        if (!heroes_ready && pending_party_apply->ticks_waited < MaxTicksToWait) return;

        ApplyHeroBuildsAndBehaviors(pending_party_apply->party);
        pending_party_apply.reset();
        GW::GameThread::RemoveGameThreadCallback(&PartyApplyGameThread_Entry);
    }

    void TriggerPartyApply(const PartyTemplate& party) {
        GW::PartyMgr::KickAllHeroes();
        for (uint32_t i = 1; i < party.party_size; i++) {
            const auto& member = party.members[i];
            if (member.type == PartyMemberType::Hero)
                GW::PartyMgr::AddHero(static_cast<GW::Constants::HeroID>(member.hero_id));
        }
        pending_party_apply = std::make_unique<PendingPartyApply>();
        pending_party_apply->party = party;
        GW::GameThread::RegisterGameThreadCallback(&PartyApplyGameThread_Entry, OnPartyApplyGameThreadTick);
    }

    bool ud_is_party = false;
    bool ud_already_built = false;
    uint32_t ud_frame_id = 0;
    uint32_t ud_stack_key = 0;
    uint32_t ud_template_id = 0;
    PartyTemplate ud_party{};

    GW::HookEntry TemplateHook_Entry;

    void Register() {
        using namespace GW::SkillbarMgr;

        RegisterDecodeTemplateHeaderCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, SkillTemplate*, void* reader, uint8_t*) {
                decode_header_ran = true;
                if (reader) decode_header_snapshot = *reinterpret_cast<GWReader*>(reader);
            }, -1);
        RegisterDecodeTemplateHeaderCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, SkillTemplate* out_template, void* reader, uint8_t* result) {
                if (*result != 0 || !reader) return;
                auto* r = reinterpret_cast<GWReader*>(reader);
                *r = decode_header_snapshot;
                const auto* data = reinterpret_cast<const uint8_t*>(r->fields[4]);
                const auto* end = reinterpret_cast<const uint8_t*>(r->fields[6]);
                PartyTemplate party{};
                if (r->fields[1] != 0 || !data || end <= data
                    || !ParseTemplateFromBytes(data, static_cast<uint32_t>(end - data), party))
                    return;
                *out_template = party.members[0].skill_template;
                pending_party_template = party;
                pending_party_template_valid = true;
                *result = 1;
            }, 1);

        RegisterGetAccountTemplateDataCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, uint32_t, SkillTemplate*, int*, int*) {
                pending_party_template_valid = false;
                decode_header_ran = false;
            }, -1);
        RegisterGetAccountTemplateDataCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, uint32_t template_id, SkillTemplate*, int*, int* result) {
                if (*result != 0 && pending_party_template_valid) {
                    party_template_cache[template_id] = pending_party_template;
                    last_party_template = pending_party_template;
                    last_party_template_valid = true;
                }
                else if (decode_header_ran) {
                    party_template_cache.erase(template_id);
                    last_party_template_valid = false;
                }
                pending_party_template_valid = false;
            }, 1);

        RegisterDecodeTemplateStringCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, const wchar_t* text, SkillTemplate* out_template, int* out_error, uint8_t* result) {
                if (*result != 0) {
                    last_decoded_party_template_valid = false;
                    return;
                }
                PartyTemplate party{};
                if (!ParseTemplateFromString(text, party)) {
                    last_decoded_party_template_valid = false;
                    return;
                }
                *out_template = party.members[0].skill_template;
                if (out_error) *out_error = 3;
                last_decoded_party_template = party;
                last_decoded_party_template_valid = true;
                *result = 1;
            }, 1);

        RegisterUpdateTemplateDisplayCallback(&TemplateHook_Entry,
            [](GW::HookStatus* status, uint32_t* frame_data) {
                ud_is_party = false;
                ud_already_built = false;
                const uint32_t template_id = frame_data[0x58];
                PartyTemplate party{};
                bool is_party = template_id != 0 && GetCachedPartyTemplate(template_id, party) && party.party_size > 1;
                if (!is_party && template_id == 0 && last_decoded_party_template_valid
                    && last_decoded_party_template.party_size > 1
                    && memcmp(&frame_data[0xd4], &last_decoded_party_template.members[0].skill_template, sizeof(SkillTemplate)) == 0) {
                    party = last_decoded_party_template;
                    is_party = true;
                }
                last_decoded_party_template_valid = false;
                if (!is_party) return;

                ud_is_party = true;
                ud_party = party;
                ud_template_id = template_id;
                ud_frame_id = frame_data[0];
                ud_stack_key = template_id ? template_id : PartySignature(party);
                const auto cached = party_stack_state.find(ud_frame_id);
                if (cached != party_stack_state.end() && cached->second.first == ud_stack_key
                    && GW::UI::GetFrameById(cached->second.second) != nullptr) {
                    ud_already_built = true;
                    status->blocked = true;
                    return;
                }
                frame_data[0x58] = 0;
            }, -1);
        RegisterUpdateTemplateDisplayCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, uint32_t* frame_data) {
                if (!ud_is_party || ud_already_built) return;
                frame_data[0x58] = ud_template_id;
                frame_data[2] = 0;
                if (const uint32_t stack_id = BuildPartyStack(ud_frame_id, ud_party))
                    party_stack_state[ud_frame_id] = { ud_stack_key, stack_id };
            }, 1);

        RegisterLoadSkillTemplateCallback(&TemplateHook_Entry,
            [](GW::HookStatus*, uint32_t agent_id, SkillTemplate*) {
                if (!(last_party_template_valid && last_party_template.party_size > 1
                    && agent_id == GW::Agents::GetControlledCharacterId()))
                    return;
                const uint32_t sig = PartySignature(last_party_template);
                if (sig != last_applied_party_signature) {
                    last_applied_party_signature = sig;
                    TriggerPartyApply(last_party_template);
                }
            }, 1);
    }
}

void PartyLoadoutModule::Initialize()
{
    ToolboxModule::Initialize();

    SkillFrameHandler_Func = reinterpret_cast<GW::UI::UIInteractionCallback>(GW::Scanner::Find(
        "\x55\x8B\xEC\x83\xEC\x3C\x56\x8B\x75\x08\x8B\x46\x04\x83\xF8\x38",
        "xxxxxxxxxxxxxxxx"));
    Log::Log("[PartyLoadout] SkillFrameHandler_Func = %p\n", SkillFrameHandler_Func);

    Register();
}

void PartyLoadoutModule::Terminate()
{
    ToolboxModule::Terminate();
    GW::SkillbarMgr::RemoveTemplateCallback(&TemplateHook_Entry);
    GW::GameThread::RemoveGameThreadCallback(&PartyApplyGameThread_Entry);
    pending_party_apply.reset();
    party_template_cache.clear();
    party_stack_state.clear();
}
