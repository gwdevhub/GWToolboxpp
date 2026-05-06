#include "SkinChanger.h"

#include <cstdint>

#include <GWCA/GWCA.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Hook.h>

#include "PluginUtils.h"
#include "ImGuiCppWrapper.h"
#include "BackupManager.h"
#include <io.h>

#include <format>
#include <sstream>

namespace 
{
    GW::HookEntry UseItem_Entry;
    GW::HookEntry AgentAdd_Entry;
    bool hasAgentAddHook = false;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry ItemGeneral_Entry;
    GW::HookEntry ItemGeneralReuse_Entry;
    GW::HookEntry RestoreChatCmd_HookEntry;

    bool needToFetchBagItems = false;

    std::map<GW::Constants::Bag, std::vector<InventoryItem>> available_items;
    struct DecodeEntry {
        std::wstring buffer;
        bool completed = false;
    };
    // Note: decodedNames grows unboundedly over a session. It cannot be cleared on plugin
    // reload because outstanding GameThread lambdas capture &entry references into the map;
    // clearing while those are pending would produce dangling references.
    std::map<std::wstring, DecodeEntry> decodedNames;

    std::optional<MinipetTransmog> pendingMinipetTransmog;

    constexpr char endOfStringIdentifier = '~';

    struct MiniPetStatus 
    {
        std::optional<uint32_t> poppedMinipetId = std::nullopt;
        std::chrono::time_point<std::chrono::steady_clock> lastPop = std::chrono::steady_clock::now();
    };
    MiniPetStatus minipetStatus;

    std::string removeTextInBrackets(std::string str)
    {
        while (true)
        {
            const auto left = str.find('<');
            if (left == std::string::npos) return str;
            const auto right = str.find('>', left);
            if (right == std::string::npos) return str;
            str.erase(left, right + 1);
        }
    }
    std::string decode(const std::wstring& wstring) 
    {
        if (wstring.empty()) return "";
        auto& entry = decodedNames[wstring];
        if (entry.buffer.empty()) 
        {
            entry.buffer.resize(256, L'\0');
            // std::map guarantees reference stability on insert, so &entry is never invalidated.
            GW::GameThread::Enqueue([wstring, &entry] {
                GW::UI::AsyncDecodeStr(wstring.c_str(), entry.buffer.data(), 256);
                entry.completed = true;
            });
            return "";
        }
        if (!entry.completed) return "";
        const auto len = wcsnlen(entry.buffer.data(), entry.buffer.size());
        return removeTextInBrackets(PluginUtils::WStringToString(std::wstring(entry.buffer.data(), len)));
    }

    std::optional<uint32_t> toInt(std::string str)
    {
        try 
        {
            const auto base = str.starts_with("0x") ? 16 : 10;
            return static_cast<uint32_t>(std::stoul(str, nullptr, base));
        }
        catch (...) 
        {
            return std::nullopt;
        }
    }

    // Use std::format (C++23) instead of stringstream — cleaner and avoids heap allocation.
    std::string toHexString(uint32_t value) 
    {
        return std::format("0x{:X}", value);
    }

    bool IsEquippable(const GW::Item* item)
    {
        if (!item) 
        {
            return false;
        }

        switch (static_cast<GW::Constants::ItemType>(item->type)) {
            case GW::Constants::ItemType::Axe:
            case GW::Constants::ItemType::Boots:
            case GW::Constants::ItemType::Bow:
            case GW::Constants::ItemType::Chestpiece:
            case GW::Constants::ItemType::Offhand:
            case GW::Constants::ItemType::Gloves:
            case GW::Constants::ItemType::Hammer:
            case GW::Constants::ItemType::Headpiece:
            case GW::Constants::ItemType::Leggings:
            case GW::Constants::ItemType::Wand:
            case GW::Constants::ItemType::Shield:
            case GW::Constants::ItemType::Staff:
            case GW::Constants::ItemType::Sword:
            case GW::Constants::ItemType::Daggers:
            case GW::Constants::ItemType::Scythe:
            case GW::Constants::ItemType::Spear:
            case GW::Constants::ItemType::Costume_Headpiece:
            case GW::Constants::ItemType::Costume:
                return true;
            default:
                return false;
        }
    }

    // AllItems removed — forEachItem is only ever called with EquippableOnly.
    // slot index replaced with range-for since the index itself was never used.
    void forEachItem(std::function<bool(GW::Item*)> func) 
    {
        using GW::Constants::Bag;
        for (auto bagSlot : {Bag::Backpack, Bag::Belt_Pouch, Bag::Bag_1, Bag::Bag_2, Bag::Equipment_Pack, Bag::Equipped_Items}) 
        {
            const auto bag = GW::Items::GetBag(bagSlot);
            if (!bag) continue;

            for (const auto& item : bag->items) 
            {
                if (IsEquippable(item)) 
                {
                    if (func(item)) return;
                }
            }
        }
    }

    std::vector<GW::ItemModifier> extractMods(const GW::Item* item)
    {
        if (!item->mod_struct || !item->mod_struct_size)
            return {};
        std::vector<GW::ItemModifier> result;
        result.resize(item->mod_struct_size);
        for (auto i = 0u; i < item->mod_struct_size; ++i) 
        {
            result[i] = item->mod_struct[i];
        }
        return result;
    }

    bool compareMods(const std::vector<GW::ItemModifier>& vec, const GW::ItemModifier* ptr, size_t size) 
    {
        if (vec.size() != size) return false;
        for (size_t i = 0; i < size; ++i)
            if (vec[i].mod != ptr[i].mod) return false;
        return true;
    }

    void drawItemSelector(InventoryItem& inventoryItem) 
    {
        auto bagName = [](GW::Constants::Bag bag) -> const char* {
            switch (bag) {
                case GW::Constants::Bag::Backpack:       return "Backpack";
                case GW::Constants::Bag::Belt_Pouch:     return "Belt Pouch";
                case GW::Constants::Bag::Bag_1:          return "Bag 1";
                case GW::Constants::Bag::Bag_2:          return "Bag 2";
                case GW::Constants::Bag::Equipment_Pack: return "Equipment Pack";
                case GW::Constants::Bag::Equipped_Items: return "Equipped items";
                default:                                 return "Unknown";
            }
        };

        if (ImGui::Button("Edit item")) {
            needToFetchBagItems = true;
            ImGui::OpenPopup("Choose item to adjust");
        }
        ImGui::SameLine();

        if (inventoryItem.modelID == 0) 
        {
            ImGui::TextUnformatted("No item chosen");
        }
        else if (!inventoryItem.encodedName.empty())
        {
            ImGui::TextUnformatted(decode(inventoryItem.encodedName).c_str());
        }
        else 
        {
            // Encoded names are not serialized to avoid wstring headache (and maybe issues when the game updates). Find the name.
            forEachItem([&inventoryItem](const GW::Item* item) 
            {
                if (item->model_id == inventoryItem.modelID && compareMods(inventoryItem.modifiers, item->mod_struct, item->mod_struct_size)) 
                {
                    inventoryItem.encodedName = item->single_item_name;
                    return true;
                }
                return false;
            });
        }

        if (ImGui::BeginPopupModal("Choose item to adjust", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) 
        {
            if (needToFetchBagItems) 
            {
                available_items.clear();
                forEachItem([&](const GW::Item* item)
                {
                    const auto bag = item->bag ? item->bag->bag_id() : GW::Constants::Bag::Backpack;
                    available_items[bag].push_back(InventoryItem{item->model_id, item->single_item_name, extractMods(item)});
                    return false;
                });
                needToFetchBagItems = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            for (auto& [bag, bagItems] : available_items) 
            {
                ImGui::TextUnformatted(bagName(bag));
                ImGui::Indent();
                for (auto& bagItem : bagItems) 
                {
                    ImGui::PushID(&bagItem);
                    if (ImGui::Button(decode(bagItem.encodedName).c_str())) 
                    {
                        inventoryItem = bagItem;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            if (ImGui::Button("Cancel")) 
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // Alpha (w) must be 1.f so colors are displayed opaquely in imgui 1.91.8+ where
    // alpha-preview is the default (ImGuiColorEditFlags_AlphaPreview was removed).
    constexpr ImVec4 palette[] = {
        {0.f, 0.f, 1.f, 1.f},       // Blue
        {0.f, 0.75f, 0.f, 1.f},     // Green
        {0.5f, 0.f, 0.5f, 1.f},     // Purple
        {1.f, 0.f, 0.f, 1.f},       // Red
        {1.f, 1.f, 0.f, 1.f},       // Yellow
        {0.5f, 0.25f, 0.f, 1.f},    // Brown
        {1.f, 0.65f, 0.f, 1.f},     // Orange
        {0.75f, 0.75f, 0.75f, 1.f}, // Silver
        {0.f, 0.f, 0.f, 1.f},       // Black
        {0.5f, 0.5f, 0.5f, 1.f},    // Gray
        {1.f, 1.f, 1.f, 1.f},       // White
        {0.95f, 0.5f, 0.95f, 1.f},  // Pink
    };
    ImVec4 ImVec4FromDyeColor(GW::DyeColor color)
    {
        switch (color) {
            case GW::DyeColor::Blue:
            case GW::DyeColor::Green:
            case GW::DyeColor::Purple:
            case GW::DyeColor::Red:
            case GW::DyeColor::Yellow:
            case GW::DyeColor::Brown:
            case GW::DyeColor::Orange:
            case GW::DyeColor::Silver:
            case GW::DyeColor::Black:
            case GW::DyeColor::Gray:
            case GW::DyeColor::White:
            case GW::DyeColor::Pink: {
                // color_id moved inside the valid-color cases so it is never computed
                // for the default branch where it could be out of range for the palette array.
                const uint32_t color_id = std::to_underlying(color) - std::to_underlying(GW::DyeColor::Blue);
                assert(color_id < _countof(palette));
                return palette[color_id];
            }
            default:
                return {};
        }
    }

    GW::DyeColor DyeColorFromInt(size_t color)
    {
        const auto col = static_cast<GW::DyeColor>(color);
        switch (col) {
            case GW::DyeColor::Blue:
            case GW::DyeColor::Green:
            case GW::DyeColor::Purple:
            case GW::DyeColor::Red:
            case GW::DyeColor::Yellow:
            case GW::DyeColor::Brown:
            case GW::DyeColor::Orange:
            case GW::DyeColor::Silver:
            case GW::DyeColor::Black:
            case GW::DyeColor::Gray:
            case GW::DyeColor::White:
            case GW::DyeColor::Pink:
                return col;
            default:
                return GW::DyeColor::None;
        }
    }

    bool drawDyePicker(const char* label, GW::DyeColor* color)
    {
        ImGui::PushID(label);

        const ImVec4 current_color = ImVec4FromDyeColor(*color);

        bool value_changed = false;
        const char* label_display_end = ImGui::FindRenderedTextEnd(label);

        if (ImGui::ColorButton("##ColorButton", current_color)) {
            ImGui::OpenPopup("picker");
        }

        if (ImGui::BeginPopup("picker")) {
            if (label != label_display_end) {
                ImGui::TextUnformatted(label, label_display_end);
                ImGui::Separator();
            }
            std::optional<size_t> palette_index;
            ImGui::PushID("##picker");
            ImGui::BeginGroup();
            for (size_t i = 0; i < _countof(palette); i++) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::ColorButton("", palette[i]))
                    palette_index = i;
                ImGui::PopID();
                if ((i + 1) % 7 != 0) ImGui::SameLine();
            }
            // "None" button
            ImGui::PushID(static_cast<int>(_countof(palette)));
            if (ImGui::ColorButton("", ImVec4{}))
                palette_index = _countof(palette);
            ImGui::PopID();
            ImGui::EndGroup();
            ImGui::PopID();
            if (palette_index.has_value()) {
                if (*palette_index < _countof(palette)) {
                    *color = DyeColorFromInt(*palette_index + static_cast<size_t>(GW::DyeColor::Blue));
                }
                else {
                    *color = GW::DyeColor::None;
                }
                value_changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return value_changed;
    }

    void TransmoAgent(DWORD agent_id, NpcTransmog transmo)
    {
        if (!transmo.npcID || !agent_id)
            return;

        const auto npcModelFileID = toInt(transmo.npcModelFileID);
        const auto npcModelFileData = toInt(transmo.npcModelFileData);
        const auto flags = toInt(transmo.flags);
        if (!npcModelFileID || !flags || npcModelFileID == 0) 
            return;

        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!agent || !agent->GetIsLivingType()) return;
        const auto existingNpc = GW::Agents::GetNPCByID(agent->player_number);
        // Fixed: cast to uint32_t before shifting into the high byte to avoid signed overflow.
        // The old signed int multiplication could produce a negative value for large scale inputs.
        // scale=0 means "keep original size"; valid non-zero range is 6-255 (matching GWToolbox's /transmog command).
        const auto scale = transmo.scale ? (static_cast<uint32_t>(std::clamp(transmo.scale, 6, 255)) * 0x1000000u) : (existingNpc ? existingNpc->visual_adjustment.scale : 0x23000000u);

        const auto& npcs = GW::GetGameContext()->world->npcs;
        if (transmo.npcID >= (int)npcs.size() || !npcs[transmo.npcID].model_file_id) 
        {
            // Removed GW::NPC staging struct — only two fields were ever read from it,
            // so capture the values directly instead of constructing an intermediate object.
            GW::GameThread::Enqueue([npcID = transmo.npcID, fileID = *npcModelFileID, npcFlags = *flags, scale] {
                    GW::Packet::StoC::NpcGeneralStats packet{};
                    packet.npc_id = npcID;
                    packet.file_id = fileID;
                    // Fixed: pass the computed scale so that the registered NPC uses the
                    // correct visual size; the default zero-initialized field rendered the NPC invisibly small.
                    packet.scale = scale;
                    packet.flags = npcFlags;
                    packet.profession = 1;
                    GW::StoC::EmulatePacket(&packet);
                });

            // Redundant self-rename capture fixed: npcModelFileData = npcModelFileData -> npcModelFileData.
            GW::GameThread::Enqueue([npcID = transmo.npcID, npcModelFileData] {
                // Zero-initialize so data[] fields beyond index 0 are not indeterminate.
                GW::Packet::StoC::NPCModelFile packet{};
                packet.npc_id = npcID;
                packet.count = npcModelFileData ? 1 : 0;
                if (packet.count)
                    packet.data[0] = *npcModelFileData;

                GW::StoC::EmulatePacket(&packet);
            });
        }

        const auto encodedName = transmo.nameToApply.empty() ? std::wstring{} : L"\x108\x107" + PluginUtils::StringToWString(transmo.nameToApply) + L"\x1";

        GW::GameThread::Enqueue([npcID = transmo.npcID, npcName = encodedName, agent_id, scale] 
        {
            GW::Packet::StoC::AgentScale packet1;
            packet1.header = GW::Packet::StoC::AgentScale::STATIC_HEADER;
            packet1.agent_id = agent_id;
            packet1.scale = scale;
            GW::StoC::EmulatePacket(&packet1);

            GW::Packet::StoC::AgentModel packet2;
            packet2.header = GW::Packet::StoC::AgentModel::STATIC_HEADER;
            packet2.agent_id = agent_id;
            packet2.model_id = npcID;
            GW::StoC::EmulatePacket(&packet2);

            if (!npcName.empty()) 
            {
                GW::Packet::StoC::AgentName packet;
                packet.header = GW::Packet::StoC::AgentName::STATIC_HEADER;
                packet.agent_id = agent_id;
                wcscpy_s(packet.name_enc, npcName.c_str());
                GW::StoC::EmulatePacket(&packet);
            }
        });
    }

    void trim(std::string& s) 
    {
        const auto isSpace = [](unsigned char ch) {return !std::isspace(ch);};
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), isSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), isSpace).base(), s.end());
    }

    void drawNpcSelector(NpcTransmog& npcTransmog)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("NPC slot ID:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(60.f);
        ImGui::PushID(0);
        ImGui::InputInt("##npcid", &npcTransmog.npcID, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unused NPC slot to register this appearance (e.g. 12). Must not collide with a real NPC ID.");
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Model file (hex):"); ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::PushID(1);
        ImGui::InputText("##npcmf", &npcTransmog.npcModelFileID);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Primary model file (mesh/skeleton), hex.");
        ImGui::PopID();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Extra file (hex):"); ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::PushID(2);
        ImGui::InputText("##npcef", &npcTransmog.npcModelFileData);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Secondary model file (e.g. armor overlay), hex. Leave as 0x if unused.");
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Flags (hex):"); ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::PushID(3);
        ImGui::InputText("##npcfl", &npcTransmog.flags);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("NPC rendering flags, hex. Use \"Copy from target\" to fill.");
        ImGui::PopID();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scale:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(60.f);
        ImGui::PushID(4);
        ImGui::InputInt("##npcsc", &npcTransmog.scale, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size multiplier (6-255). 0 = keep original size.");
        ImGui::PopID();
        if (npcTransmog.scale < 0) npcTransmog.scale = 0;
        if (npcTransmog.scale > 0 && npcTransmog.scale < 6) npcTransmog.scale = 6;
        if (npcTransmog.scale > 255) npcTransmog.scale = 255;
    }

    void drawMinipetSelector(MinipetTransmog& minipetTransmog) 
    {
        ImGui::TextUnformatted("Target:");
        ImGui::Indent();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Inventory model ID:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::PushID(2);
        { int v = static_cast<int>(minipetTransmog.itemToReplaceModelID); ImGui::InputInt("##miniitem", &v, 0); if (v >= 0) minipetTransmog.itemToReplaceModelID = static_cast<uint32_t>(v); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Model ID of the minipet item in your inventory (the pop). Triggers the transmog when used.");
        ImGui::PopID();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Terrain agent model ID:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::PushID(1);
        { int v = static_cast<int>(minipetTransmog.agentToReplaceModelID); ImGui::InputInt("##miniagent", &v, 0); if (v >= 0) minipetTransmog.agentToReplaceModelID = static_cast<uint32_t>(v); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Model ID of the minipet agent spawned on terrain (\"player_number\" on the agent).");
        ImGui::PopID();
        ImGui::Unindent();

        ImGui::Spacing();
        ImGui::TextUnformatted("Inventory skin:");
        ImGui::Indent();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Model file (hex):"); ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::PushID(3);
        ImGui::InputText("##miniitemskin", &minipetTransmog.replacementItemModelFileID);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Overrides the item's model file in your inventory. Leave as 0x to keep original.");
        ImGui::PopID();
        ImGui::Unindent();

        ImGui::Spacing();
        ImGui::TextUnformatted("Terrain appearance:");
        ImGui::Indent();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        ImGui::PushID(0);
        ImGui::InputText("##mininame", &minipetTransmog.npcTransmog.nameToApply);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name shown above the minipet on terrain. Leave empty to keep the original.");
        ImGui::PopID();
        if (minipetTransmog.npcTransmog.nameToApply.size() > 31) 
            minipetTransmog.npcTransmog.nameToApply.resize(31);
        // Strip the sentinel character used in INI serialization; it would corrupt the
        // name round-trip by terminating the string read early on next load.
        std::erase(minipetTransmog.npcTransmog.nameToApply, endOfStringIdentifier);

        if (ImGui::Button("Copy from target"))
        {
            [&] {
                // Fixed: GetTarget() can return null when no agent is targeted; the original
                // code proceeded to call GetIsLivingType() on a null pointer, causing a crash.
                const auto target = GW::Agents::GetTarget();
                if (!target || !target->GetIsLivingType()) return;
                const auto living = target->GetAsAgentLiving();
                const GW::Player* player = living && living->IsPlayer() ? GW::PlayerMgr::GetPlayerByID(living->player_number) : nullptr;
                auto npcID = living && living->IsNPC() ? living->player_number : 0;
                if (player && living->transmog_npc_id & 0x20000000) npcID = living->transmog_npc_id ^ 0x20000000;

                const auto npc = GW::Agents::GetNPCByID(npcID);
                if (!npc) return;
                minipetTransmog.npcTransmog.npcID = npcID;
                minipetTransmog.npcTransmog.scale = npc->visual_adjustment.scale / 0x1000000;
                minipetTransmog.npcTransmog.flags = toHexString(npc->npc_flags);
                minipetTransmog.npcTransmog.npcModelFileID = toHexString(npc->model_file_id);
                if (npc->files_count) 
                    minipetTransmog.npcTransmog.npcModelFileData = toHexString(npc->model_files[0]);
            }();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto-fill terrain appearance fields from your current target");

        ImGui::PushID(4);
        drawNpcSelector(minipetTransmog.npcTransmog);
        ImGui::PopID();
        ImGui::Unindent();
    }

    std::optional<uint32_t> getInteractionOverwrite(uint32_t modelFileID) 
    {
        // Non-EotN elementalist headpieces have a weird interaction value. If one tries to transmog them to the EotN pieces, it fails.
        // Manually overwrite the value when using these skins.
        switch (modelFileID) 
        {
            case 0x7F5: // Bandana
            case 0x817: // Blindfold
            case 0x818: // Crown
            case 0x830: // Dread Mask
            case 0x8AE: // Highlander Woad
            case 0x7F6: // Mask of the Mo Zing
            case 0x8AF: // Norn Woad
            case 0x844: // Slim Spectacles
            case 0x845: // Spectacles
            case 0x846: // Tinted Spectacles
                return 0x20000006;
            default:
                return std::nullopt;
        }
    }

    void applyOverrideToItem(GW::Item* item,
                             const std::vector<ItemChange>& itemChanges,
                             const std::vector<MinipetTransmog>& minipetTransmogs)
    {
        const auto itemChangeIt = std::ranges::find_if(itemChanges, [&item](const auto& ic) {
            return ic.item.modelID
                && ic.item.modelID == item->model_id
                && compareMods(ic.item.modifiers, item->mod_struct, item->mod_struct_size);
        });
        if (itemChangeIt != itemChanges.end()) {
            if (const auto modelFileID = toInt(itemChangeIt->modelFileID); modelFileID && *modelFileID != 0) {
                item->model_file_id = *modelFileID;
                if (const auto interactionOverwrite = getInteractionOverwrite(*modelFileID))
                    item->interaction = *interactionOverwrite;
            }
            if (itemChangeIt->enableDyes) {
                item->dye.dye_tint = itemChangeIt->tint;
                item->dye.dye1    = itemChangeIt->dyes[0];
                item->dye.dye2    = itemChangeIt->dyes[1];
                item->dye.dye3    = itemChangeIt->dyes[2];
                item->dye.dye4    = itemChangeIt->dyes[3];
            }
        }

        const auto minipetIt = std::ranges::find_if(minipetTransmogs, [&item](const auto& transmog) {
            return transmog.itemToReplaceModelID && item->model_id == transmog.itemToReplaceModelID;
        });
        if (minipetIt != minipetTransmogs.end()) {
            if (const auto modelFileID = toInt(minipetIt->replacementItemModelFileID); modelFileID && *modelFileID)
                item->model_file_id = *modelFileID;
        }
    }

    } // namespace

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static SkinChanger instance;
    return &instance;
}

void SkinChanger::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    BackupManager::getInstance().initialize(folder);

    loadFromIniFile(GetSettingFile(folder).c_str());
}

void SkinChanger::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    
    std::string itemsToSave;
    itemsToSave.reserve(4096);
    for (const auto& itemChange : itemChanges) 
    {
        itemsToSave += std::to_string(itemChange.item.modelID) + " ";
        for (const auto& mod : itemChange.item.modifiers) 
        {
            itemsToSave += std::to_string(mod.mod) + " ";
        }
        itemsToSave += "S ";
        itemsToSave += itemChange.modelFileID + " ";
        for (const auto& dye : itemChange.dyes)
            itemsToSave += std::to_string((int)dye) + " ";
        itemsToSave += std::to_string(itemChange.tint) + " ";
        itemsToSave += std::to_string((int)itemChange.enableDyes) + " ";
        itemsToSave += "END ";
    }
    ini.SetValue(Name(), VAR_NAME(itemChanges), itemsToSave.c_str());

    std::string transmogsToSave;
    transmogsToSave.reserve(4096);
    for (const auto& minipetTransmog : minipetTransmogs) 
    {
        transmogsToSave += std::to_string(minipetTransmog.agentToReplaceModelID) + " ";
        transmogsToSave += std::to_string(minipetTransmog.itemToReplaceModelID) + " ";
        transmogsToSave += minipetTransmog.replacementItemModelFileID + " ";
        transmogsToSave += minipetTransmog.npcTransmog.flags + " ";
        transmogsToSave += minipetTransmog.npcTransmog.nameToApply + " " + endOfStringIdentifier + " ";
        transmogsToSave += std::to_string(minipetTransmog.npcTransmog.npcID) + " ";
        transmogsToSave += minipetTransmog.npcTransmog.npcModelFileData + " ";
        transmogsToSave += minipetTransmog.npcTransmog.npcModelFileID + " ";
        transmogsToSave += std::to_string(minipetTransmog.npcTransmog.scale) + " ";
        transmogsToSave += "END ";
    }
    ini.SetValue(Name(), VAR_NAME(minipetTransmogs), transmogsToSave.c_str());

    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);

    if (itemChanges.size() || minipetTransmogs.size()) BackupManager::getInstance().save(PluginUtils::StringToWString(Name()), GetSettingFile(folder));
}

void SkinChanger::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Agents::GetControlledCharacter()) 
    {
        return;
    }
    ImGui::Text("Item skin overrides:");
    {
        constexpr float indent = 16.f;
        int index = 0;
        std::optional<int> indexToDelete;
        for (auto& itemChange : itemChanges) {
            ImGui::PushID(index++);

            if (ImGui::Button("X")) indexToDelete = index - 1;
            ImGui::SameLine();
            drawItemSelector(itemChange.item);

            ImGui::Indent(indent);

            ImGui::Text("New model file ID:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(100.f);
            ImGui::InputText("##modelfileid", &itemChange.modelFileID);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Model file ID to apply to this item (hex, e.g. 0x7F5). Takes effect on next map change.");
            std::erase_if(itemChange.modelFileID, [](auto c) { return std::isspace(c); });

            ImGui::PushID(&itemChange.enableDyes);
            ImGui::Checkbox("Override dyes", &itemChange.enableDyes);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, forces the item to display with the chosen dye colours");
            ImGui::PopID();

            if (itemChange.enableDyes) {
                ImGui::Text("Dyes:"); ImGui::SameLine();
                drawDyePicker("Dye 1", &itemChange.dyes[0]); ImGui::SameLine();
                drawDyePicker("Dye 2", &itemChange.dyes[1]); ImGui::SameLine();
                drawDyePicker("Dye 3", &itemChange.dyes[2]); ImGui::SameLine();
                drawDyePicker("Dye 4", &itemChange.dyes[3]); ImGui::SameLine();
                ImGui::Text("Tint:"); ImGui::SameLine();
                int tint = itemChange.tint;
                ImGui::SetNextItemWidth(55.f);
                ImGui::InputInt("##tint", &tint, 0);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Dye tint value (0-255)");
                if (tint < 0) tint = 0;
                if (tint > 255) tint = 255;
                itemChange.tint = (uint8_t)tint;
            }

            ImGui::Unindent(indent);
            ImGui::PopID();
        }
        if (ImGui::Button("+##items")) itemChanges.push_back({{}, "0x", false, {GW::DyeColor::None, GW::DyeColor::None, GW::DyeColor::None, GW::DyeColor::None}, 255});
        if (indexToDelete) itemChanges.erase(itemChanges.begin() + *indexToDelete);
        if (!itemChanges.empty()) ImGui::Separator();
    }

    ImGui::Spacing();
    ImGui::Text("Minipet transmogs:");
    {
        constexpr float indent = 16.f;
        int index = 0;
        std::optional<int> indexToDelete;
        for (auto& minipetTransmog : minipetTransmogs) {
            ImGui::PushID(index++ + (int)itemChanges.size());

            if (ImGui::Button("X")) indexToDelete = index - 1;
            ImGui::SameLine();
            ImGui::TextUnformatted("Minipet to replace:");

            ImGui::Indent(indent);
            drawMinipetSelector(minipetTransmog);
            ImGui::Unindent(indent);

            ImGui::PopID();
        }
        ImGui::PushID(&minipetTransmogs);
        if (ImGui::Button("+##minipets")) minipetTransmogs.push_back({});
        ImGui::PopID();
        if (indexToDelete) minipetTransmogs.erase(minipetTransmogs.begin() + *indexToDelete);
    }

    ImGui::Text("Version 2.0.0");
}

void SkinChanger::loadFromIniFile(const wchar_t* filePath)
{
    ini.LoadFile(filePath);
    itemChanges.clear();
    minipetTransmogs.clear();

    [&]{
        std::string loadedItems = ini.GetValue(Name(), VAR_NAME(itemChanges), "");
        if (loadedItems.empty()) return;

        std::stringstream ss{loadedItems};
        while (ss) {
            ItemChange itemChange;

            ss >> std::ws >> itemChange.item.modelID >> std::ws;

            uint32_t readMod;
            while (ss && ss.peek() != 'S') {
                ss >> readMod;
                itemChange.item.modifiers.push_back({readMod});
                ss >> std::ws;
            }
            if (ss && ss.peek() == 'S')
                ss.get();
            else
                return;

            ss >> std::ws;
            if (ss) ss >> itemChange.modelFileID;

            int readDye;
            for (auto& dye : itemChange.dyes) {
                ss >> readDye;
                dye = (GW::DyeColor)(readDye);
            }
            {
                // String stream for uint8_t does not what you would expect
                int read;
                ss >> read;
                if (read >= 0 && read < 256) itemChange.tint = (uint8_t)read;
            }
            ss >> itemChange.enableDyes;

            std::string read;
            while (ss >> read && read != "END") {}

            itemChanges.push_back(itemChange);
        }
    }();

    [&]{
        std::string loadedTransmogs = ini.GetValue(Name(), VAR_NAME(minipetTransmogs), "");
        if (loadedTransmogs.empty()) return;

        std::stringstream ss{loadedTransmogs};
        while (ss) 
        {
            MinipetTransmog transmog;

            ss >> transmog.agentToReplaceModelID;
            ss >> transmog.itemToReplaceModelID;
            ss >> std::ws >> transmog.replacementItemModelFileID;
            ss >> std::ws >> transmog.npcTransmog.flags;
            while (true) 
            {
                if (!ss) break;
                ss >> std::ws;
                if (ss.peek() == endOfStringIdentifier) 
                {
                    ss.get();
                    break;
                }
                std::string read;
                ss >> read;
                transmog.npcTransmog.nameToApply += read;
                transmog.npcTransmog.nameToApply += " ";
            }
            trim(transmog.npcTransmog.nameToApply);
            ss >> std::ws >> transmog.npcTransmog.npcID;
            ss >> std::ws >> transmog.npcTransmog.npcModelFileData;
            ss >> std::ws >> transmog.npcTransmog.npcModelFileID;
            ss >> std::ws >> transmog.npcTransmog.scale;

            std::string read;
            while (ss >> read && read != "END") {}
            ss >> std::ws;

            minipetTransmogs.push_back(transmog);
        }
    }();
}

void SkinChanger::applyOverrideToItem(GW::Item* item) const
{
    ::applyOverrideToItem(item, itemChanges, minipetTransmogs);
}

void SkinChanger::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);

    // Reset transient state so a plugin reload always starts clean.
    hasAgentAddHook = false;
    needToFetchBagItems = false;
    pendingMinipetTransmog = std::nullopt;
    minipetStatus = {};
    available_items.clear();

    // Use post-packet callbacks on the raw StoC ItemGeneral packets so our writes to model_file_id
    // happen AFTER the game has applied all packet fields to GW::Item. A UI message callback for
    // kItemUpdated fires as a pre-callback (before frame handlers write the item data), so any
    // changes we make there get overwritten immediately by the game's own processing.
    const auto itemGeneralHandler = [this](GW::HookStatus*, const GW::Packet::StoC::ItemGeneral_FirstID* pak) {
        const auto item = GW::Items::GetItemById(pak->item_id);
        if (!item) return;
        applyOverrideToItem(item);
    };
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::ItemGeneral_FirstID>(&ItemGeneral_Entry, itemGeneralHandler);
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&ItemGeneralReuse_Entry,
        [this](GW::HookStatus*, const GW::Packet::StoC::ItemGeneral_ReuseID* pak) {
            const auto item = GW::Items::GetItemById(pak->item_id);
            if (!item) return;
            applyOverrideToItem(item);
        });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        using namespace std::chrono_literals;
        minipetStatus.poppedMinipetId = std::nullopt;
        minipetStatus.lastPop = std::chrono::steady_clock::now() - 1h;
    });

    GW::Chat::CreateCommand(&RestoreChatCmd_HookEntry, L"restore", [](GW::HookStatus* status, const wchar_t*, const int argc, const LPWSTR* argv) {
        const auto instance = static_cast<SkinChanger*>(ToolboxPluginInstance());
        if (!instance || argc < 2) {
            status->blocked = false;
            return;
        }
        const auto arg1 = PluginUtils::ToLower(argv[1]);
        const auto pluginName = PluginUtils::StringToWString(instance->Name());


        std::filesystem::path iniToLoad;
        if (arg1 != PluginUtils::ToLower(pluginName)) {
            status->blocked = false;
            return;
        }
        if (argc < 3 || PluginUtils::ToLower(argv[2]) == L"recent") {
            logMessage("Restore most recent backup", instance->Name());
            iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Latest);
        }
        else if (PluginUtils::ToLower(argv[2]) == L"largest") {
            logMessage("Restore largest backup", instance->Name());
            iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Largest);
        }
        else if (PluginUtils::ToLower(argv[2]) == L"list") {
            logMessage("Available backups:", instance->Name());
            const auto paths = BackupManager::getInstance().list(pluginName);
            for (const auto& path : paths) {
                const auto name = path.filename().string().substr(0, 1);
                const auto time = std::format("{:%Y-%m-%d %H:%M}", std::filesystem::last_write_time(path));
                const auto size = std::filesystem::file_size(path);
                logMessage(std::format("Backup {}, Last change {}, File size {}", name, time, size), instance->Name());
            }
        }
        else if (PluginUtils::ToLower(argv[2]) == L"help") {
            logMessage("Type \"/restore " + std::string{instance->Name()} + " recent\" to restore the most recent backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " largest\" to restore the largest backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " list\" to show the available backups", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " $NUMBER\" to restore a specific backup", instance->Name());
            logMessage("Type \"/restore " + std::string{instance->Name()} + " help\" to show this menu", instance->Name());
        }
        else {
            try {
                const auto index = std::stoi(argv[2]);
                logMessage("Restore backup " + std::to_string(index), instance->Name());
                iniToLoad = BackupManager::getInstance().load(pluginName, BackupManager::LoadType::Index, index);
            } catch (...) {
                status->blocked = false;
                return;
            }
        }
        if (!iniToLoad.empty()) {
            instance->loadFromIniFile(iniToLoad.c_str());
        }
    });

    // Use [this] instead of [&] to avoid capturing dangling references if the callback
    // fires after SignalTerminate has begun tearing down local state.
    GW::UI::RegisterUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem, [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        if (!wparam) return;

        const auto item = GW::Items::GetItemById((uint32_t)wparam);
        if (!item) return;

        const auto potentialTransmog = std::ranges::find_if(minipetTransmogs, [id = item->model_id](const auto& transmog) { return transmog.itemToReplaceModelID == id; });
        const auto isGhostInTheBox = item->model_id == GW::Constants::ItemID::GhostInTheBox;
        if (!isGhostInTheBox && potentialTransmog == minipetTransmogs.end()) return;

        if (minipetStatus.poppedMinipetId && minipetStatus.poppedMinipetId.value() == item->model_id) 
        {
            minipetStatus.poppedMinipetId = std::nullopt;
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto msSinceLastPop = std::chrono::duration_cast<std::chrono::milliseconds>(now - minipetStatus.lastPop).count();

        if (msSinceLastPop <= 10'000) return;
        minipetStatus.lastPop = now;

        if (!isGhostInTheBox)
        {
            minipetStatus.poppedMinipetId = item->model_id;
            pendingMinipetTransmog = *potentialTransmog;
        }
    });
}

void SkinChanger::Update(float)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) 
    {
        pendingMinipetTransmog = std::nullopt;
        if (hasAgentAddHook) 
        {
            hasAgentAddHook = false;
            GW::StoC::RemovePostCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry);
        }
        return;
    }
    if (!hasAgentAddHook) 
    {
        hasAgentAddHook = true;
        // Use [this] instead of [&] for the same reason as the UseItem callback above.
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry, [this](GW::HookStatus* status, const GW::Packet::StoC::AgentAdd* pak) 
        {
            if (status->blocked || !pak->agent_type || !pendingMinipetTransmog) return;

            const auto agent = GW::Agents::GetAgentByID(pak->agent_id);
            if (!agent || !agent->GetIsLivingType()) return;
            if (agent->GetAsAgentLiving()->player_number != pendingMinipetTransmog->agentToReplaceModelID) return;

            TransmoAgent(agent->agent_id, pendingMinipetTransmog->npcTransmog);
            pendingMinipetTransmog = std::nullopt;
        });
    }
}

void SkinChanger::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();

    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::ItemGeneral_FirstID>(&ItemGeneral_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&ItemGeneralReuse_Entry);
    GW::UI::RemoveUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry);
    GW::Chat::DeleteCommand(&RestoreChatCmd_HookEntry);
}
