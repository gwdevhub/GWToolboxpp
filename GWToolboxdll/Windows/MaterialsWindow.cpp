#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MerchantMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/MaterialsWindow.h>

static const DWORD MIN_TIME_BETWEEN_RETRY = 160; // 10 frames

GW::Item *MaterialsWindow::GetMerchItem(Material mat) const {
    uint32_t model_id = GetModelID(mat);
    for (uint32_t item_id : merch_items) {
        GW::Item* item = GW::Items::GetItemById(item_id);
        if (item && item->model_id == model_id)
            return item;
    }
    return nullptr;
}

GW::Item *MaterialsWindow::GetBagItem(Material mat) const {
    uint32_t model_id = GetModelID(mat);
    int min_qty = mat <= Material::WoodPlank ? 10 : 1; // 10 if common, 1 if rare
    GW::Bag **bags = GW::Items::GetBagArray();
    if (!bags) return nullptr;
    size_t bag_i = (size_t)GW::Constants::Bag::Backpack;
    size_t bag_n = (size_t)GW::Constants::Bag::Bag_2;
    for (size_t i = bag_i; i <= bag_n; i++) {
        GW::Bag *bag = bags[i];
        if (!bag) continue;
        size_t pos = bag->find1(model_id, 0);
        while (pos != GW::Bag::npos) {
            GW::Item *item = bag->items[pos];
            if (item->quantity >= min_qty)
                return item;
            pos = bag->find1(model_id, pos + 1);
        }
    }
    return nullptr;
}

void MaterialsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (cancelled) return;
    const auto tickcount = GetTickCount();
    if (quote_pending && (tickcount < quote_pending_time)) return;
    if (trans_pending && (tickcount < trans_pending_time)) return;
    if (transactions.empty()) return;

    Transaction& trans = transactions.front();

    if (trans.type == Transaction::Buy || trans.type == Transaction::Quote) {
        trans.item_id = RequestPurchaseQuote(trans.material);
    } else if (trans.type == Transaction::Sell) {
        trans.item_id = RequestSellQuote(trans.material);
    }

    if (trans.item_id) {
        quote_pending_time = GetTickCount() + MIN_TIME_BETWEEN_RETRY;
        quote_pending = true;
    } else {
        Dequeue();
    }
}

bool MaterialsWindow::GetIsInProgress() {
    return !transactions.empty();
}

void MaterialsWindow::Initialize() {
    ToolboxWindow::Initialize();
    
    tex_essence = Resources::GetItemImage(L"Essence of Celerity");
    tex_grail = Resources::GetItemImage(L"Grail of Might");
    tex_armor = Resources::GetItemImage(L"Armor of Salvation");
    tex_powerstone = Resources::GetItemImage(L"Powerstone of Courage");
    tex_resscroll = Resources::GetItemImage(L"Scroll of Resurrection");
    
    for (int i = 0; i < N_MATS; ++i) {
        price[i] = PRICE_DEFAULT;
    }

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&QuotedItemPrice_Entry,
    [this](GW::HookStatus *, GW::Packet::StoC::QuotedItemPrice *pak) -> void {
        // printf("Received price %d for %d (item %d)\n", pak->price, item->ModelId, pak->itemid);
        if (transactions.empty()) return;
        Transaction& trans = transactions.front();
        if (cancelled || (trans.item_id != pak->itemid)) {
            quote_pending = false;
            return;
        }

        auto gold_character = GW::Items::GetGoldAmountOnCharacter();
        if (trans.type == Transaction::Quote) {
            price[trans.material] = static_cast<int>(pak->price);
            Dequeue();
        } else if (trans.type == Transaction::Buy) {
            price[trans.material] = static_cast<int>(pak->price);
            if (gold_character >= pak->price) {
                GW::Merchant::TransactionInfo give, recv;
                give.item_count = 0;
                give.item_ids = nullptr;
                give.item_quantities = nullptr;
                recv.item_count = 1;
                recv.item_ids = &pak->itemid;
                recv.item_quantities = nullptr;

                GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderBuy, pak->price, give, 0, recv);
                trans_pending_time = GetTickCount() + MIN_TIME_BETWEEN_RETRY;
                trans_pending = true;
            } else {
                if (manage_gold) {
                    GW::Items::WithdrawGold();
                } else {
                    Cancel();
                }
            }
            // printf("sending purchase request for %d (price=%d)\n", item->ModelId, pak->price);
        } else if (trans.type == Transaction::Sell) {
            if (gold_character + pak->price <= 100 * 1000) {
                GW::Merchant::TransactionInfo give, recv;
                give.item_count = 1;
                give.item_ids = &pak->itemid;
                give.item_quantities = nullptr;
                recv.item_count = 0;
                recv.item_ids = nullptr;
                recv.item_quantities = nullptr;

                GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderSell, 0, give, pak->price, recv);
                trans_pending_time = GetTickCount() + MIN_TIME_BETWEEN_RETRY;
                trans_pending = true;
            } else {
                if (manage_gold) {
                    GW::Items::DepositGold();
                } else {
                    Cancel();
                }
            }
            // printf("sending sell request for %d (price=%d)\n", item->ModelId, pak->price);
        }

        quote_pending = false;
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TransactionDone>(&TransactionDone_Entry,
        [this](GW::HookStatus *, GW::Packet::StoC::TransactionDone *pak) -> void {;
            UNREFERENCED_PARAMETER(pak);
            if (transactions.empty()) return;
            trans_pending = false;
            Dequeue();
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemStreamEnd>(&ItemStreamEnd_Entry,
    [this](GW::HookStatus *, GW::Packet::StoC::ItemStreamEnd *pak) -> void {
        // @Remark: unk1 = 13 means "selling" tab
        if (pak->unk1 != 12) return;
        GW::MerchItemArray* items = GW::Merchant::GetMerchantItemsArray();
        merch_items.clear();
        if (items) {
            for (const auto item_id : *items)
                merch_items.push_back(item_id);
        }

    });
}

void MaterialsWindow::Terminate() {
    ToolboxWindow::Terminate();
}

void MaterialsWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    manage_gold = ini->GetBoolValue(Name(), VAR_NAME(manage_gold), manage_gold);
    use_stock = ini->GetBoolValue(Name(), VAR_NAME(use_stock), use_stock);
}

void MaterialsWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(manage_gold), manage_gold);
    ini->SetBoolValue(Name(), VAR_NAME(use_stock), use_stock);
}

void MaterialsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

        float x, y, h;

        const size_t stockStart = static_cast<size_t>(GW::Constants::Bag::Backpack);
        const size_t stockEnd = static_cast<size_t>(GW::Constants::Bag::Storage_14);

        // note: textures are 64 x 64, but both off-center 
        // and with a bunch of empty space. We want to center the image
        // while minimizing the rescaling

        // === Essence ===
        ImGui::Image((ImTextureID)*tex_essence, ImVec2(50, 50),
            ImVec2(4.0f / 64, 9.0f / 64), ImVec2(47.0f / 64, 52.0f / 64));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Essence of Celerity\nFeathers and Dust");
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(Feather, 5.0f, PileofGlitteringDust, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##essence", ImVec2(100.0f, 0))) {
            EnqueueQuote(Feather);
            EnqueueQuote(PileofGlitteringDust);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_essence = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###essenceqty", &qty_essence);
        if (qty_essence < 1) qty_essence = 1;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##essence", ImVec2(100.0f, 0))) {            
            int featherStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(Feather), stockStart, stockEnd);
            int dustStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(PileofGlitteringDust), stockStart, stockEnd);

            int qty = 5 * qty_essence;
            for (int i = 0; i < qty; ++i) {
                if (!use_stock || i < qty - featherStock / 10) {
                    EnqueuePurchase(Feather);
                }
                if (!use_stock || i < qty - dustStock / 10) {
                    EnqueuePurchase(PileofGlitteringDust);
                }
            }
        }

        ImGui::Separator();
        // === Grail ===
        ImGui::Image((ImTextureID)*tex_grail, ImVec2(50, 50),
            ImVec2(3.0f / 64, 11.0f / 64), ImVec2(49.0f / 64, 57.0f / 64));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grail of Might\nIron and Dust");
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(IronIngot, 5.0f, PileofGlitteringDust, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##grail", ImVec2(100.0f, 0))) {
            EnqueueQuote(IronIngot);
            EnqueueQuote(PileofGlitteringDust);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_grail = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###grailqty", &qty_grail);
        if (qty_grail < 1) qty_grail = 1;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##grail", ImVec2(100.0f, 0))) {
            int ironStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(IronIngot), stockStart, stockEnd);
            int dustStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(PileofGlitteringDust), stockStart, stockEnd);

            int qty = 5 * qty_grail;
            for (int i = 0; i < qty; ++i) {
                if (!use_stock || i < qty - ironStock / 10) {
                    EnqueuePurchase(IronIngot);
                }
                if (!use_stock || i < qty - dustStock / 10) {
                    EnqueuePurchase(PileofGlitteringDust);
                }
            }
        }

        ImGui::Separator();
        // === Armor ===
        ImGui::Image((ImTextureID)*tex_armor, ImVec2(50, 50),
            ImVec2(0, 1.0f / 64), ImVec2(59.0f / 64, 60.0f / 64));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Armor of Salvation\nIron and Bones");
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(IronIngot, 5.0f, Bone, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##armor", ImVec2(100.0f, 0))) {
            EnqueueQuote(IronIngot);
            EnqueueQuote(Bone);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_armor = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###armorqty", &qty_armor);
        if (qty_armor < 1) qty_armor = 1;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##armor", ImVec2(100.0f, 0))) {
            int ironStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(IronIngot), stockStart, stockEnd);
            int boneStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(Bone), stockStart, stockEnd);

            int qty = 5 * qty_armor;
            for (int i = 0; i < qty; ++i) {
                if (!use_stock || i < qty - ironStock / 10) {
                    EnqueuePurchase(IronIngot);
                }
                if (!use_stock || i < qty - boneStock / 10) {
                    EnqueuePurchase(Bone);
                }
            }
        }

        ImGui::Separator();
        // === Powerstone ===
        ImGui::Image((ImTextureID)*tex_powerstone, ImVec2(50, 50),
            ImVec2(0, 6.0f / 64), ImVec2(54.0f / 64, 60.0f / 64));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Powerstone of Courage\nGranite and Dust");
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GraniteSlab, 10.0f, PileofGlitteringDust, 10.0f, 1000).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##pstone", ImVec2(100.0f, 0))) {
            EnqueueQuote(GraniteSlab);
            EnqueueQuote(PileofGlitteringDust);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_pstone = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###pstoneqty", &qty_pstone);
        if (qty_pstone < 1) qty_pstone = 1;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##pstone", ImVec2(100.0f, 0))) {
            int graniteStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(GraniteSlab), stockStart, stockEnd);
            int dustStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(PileofGlitteringDust), stockStart, stockEnd);

            int qty = 10 * qty_pstone;
            for (int i = 0; i < qty; ++i) {
                if (!use_stock || i < qty - graniteStock / 10) {
                    EnqueuePurchase(GraniteSlab);
                }
                if (!use_stock || i < qty - dustStock / 10) {
                    EnqueuePurchase(PileofGlitteringDust);
                }
            }
        }

        ImGui::Separator();
        // === Res scroll ===
        ImGui::Image((ImTextureID)*tex_resscroll, ImVec2(50, 50),
            ImVec2(1.0f / 64, 4.0f / 64), ImVec2(56.0f / 64, 59.0f / 64));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scroll of Resurrection\nFibers and Bones");
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(PlantFiber, 2.5f, Bone, 2.5f, 250).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##resscroll", ImVec2(100.0f, 0))) {
            EnqueueQuote(PlantFiber);
            EnqueueQuote(Bone);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_resscroll = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###resscrollqty", &qty_resscroll);
        if (qty_resscroll < 1) qty_resscroll = 1;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##resscroll", ImVec2(100.0f, 0))) {
            int fiberStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(PlantFiber), stockStart, stockEnd);
            int boneStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID(Bone), stockStart, stockEnd);

            float qty = 2.5f * qty_resscroll;
            for (int i = 0; i < (int)std::ceil(qty); ++i) {
                if (!use_stock || i < qty - fiberStock / 10) {
                    EnqueuePurchase(PlantFiber);
                }
                if (!use_stock || i < qty - boneStock / 10) {
                    EnqueuePurchase(Bone);
                }
            }
        }

        ImGui::Separator();

        float width2 = 100.0f;
        float width1 = (ImGui::GetContentRegionAvail().x - width2 - 100.0f - ImGui::GetStyle().ItemSpacing.x * 2);

        // === generic materials ===
        static int common_idx = 0;
        static int common_qty = 1;
        static const char* common_names[] = {
            "10 Bolts of Cloth",
            "10 Bones",
            "10 Chitin Fragments",
            "10 Feathers",
            "10 Granite Slabs",
            "10 Iron Ingots",
            "10 Piles of Glittering Dust",
            "10 Plant Fibers",
            "10 Scales",
            "10 Tanned Hide Squares",
            "10 Wood Planks",
        };
        ImGui::PushItemWidth(width1);
        ImGui::Combo("##commoncombo", &common_idx, common_names, 11);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(width2);
        ImGui::InputInt("##commonqty", &common_qty);
        ImGui::PopItemWidth();
        if (common_qty < 1) common_qty = 1;
        ImGui::SameLine();
        if (ImGui::Button("Buy##common", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            int materialStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID((Material)common_idx), stockStart, stockEnd);

            for (int i = 0; i < common_qty - (use_stock ? std::floor(materialStock / 10) : 0); ++i) {
                EnqueuePurchase((Material)common_idx);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Sell##common", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            for (int i = 0; i < common_qty; ++i) {
                EnqueueSell((Material)common_idx);
            }
        }

        // === Rare materials ===
        static int rare_idx = 0;
        static int rare_qty = 1;
        static const char* rare_names[] = { "Amber Chunk",
            "Bolt of Damask",
            "Bolt of Linen",
            "Bolt of Silk",
            "Deldrimor Steel Ingot",
            "Diamond",
            "Elonian Leather Square",
            "Fur Square",
            "Glob of Ectoplasm",
            "Jadeite Shard",
            "Leather Square",
            "Lump of Charcoal",
            "Monstrous Claw",
            "Monstrous Eye",
            "Monstrous Fang",
            "Obsidian Shard",
            "Onyx Gemstone",
            "Roll of Parchment",
            "Roll of Vellum",
            "Ruby",
            "Sapphire",
            "Spiritwood Plank",
            "Steel Ingot",
            "Tempered Glass Vial",
            "Vial of Ink"
        };
        ImGui::PushItemWidth(width1);
        ImGui::Combo("##rarecombo", &rare_idx, rare_names, 25);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(width2);
        ImGui::InputInt("##rareqty", &rare_qty);
        ImGui::PopItemWidth();
        if (rare_qty < 1) rare_qty = 1;
        ImGui::SameLine();
        if (ImGui::Button("Buy##rare", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            int materialStock = GW::Items::CountItemByModelId(MaterialsWindow::GetModelID((Material)(rare_idx + AmberChunk)), stockStart, stockEnd);

            for (int i = 0; i < rare_qty - (use_stock ? materialStock : 0); ++i) {
                EnqueuePurchase((Material)(rare_idx + AmberChunk));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Sell##rare", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            for (int i = 0; i < rare_qty; ++i) {
                EnqueueSell((Material)(rare_idx + AmberChunk));
            }
        }

        ImGui::Separator();
        float progress = 0.0f;
        if (trans_queued > 0) progress = (float)(trans_done) / trans_queued;
        const char* status = "";
        if (cancelled) status = "Cancelled";
        else if (trans_done < trans_queued)
            status = "Working";
        else status = "Ready";
        ImGui::Text("%s [%d / %d]", status, trans_done, trans_queued);
        ImGui::SameLine(width1 + ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ItemSpacing.x);
        ImGui::ProgressBar(progress, ImVec2(width2, 0));
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0))) {
            Cancel();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel the current queue of operations");
    }
    ImGui::End();
}

void MaterialsWindow::Cancel() {
    cancelled = true;
    quote_pending = false;
    trans_pending = false;
    transactions.clear();
}

void MaterialsWindow::Dequeue() {
    trans_done++;
    transactions.pop_front();
}

void MaterialsWindow::Enqueue(Transaction::Type type, Material mat) {
    if (transactions.empty()) {
        trans_done = 0;
        trans_queued = 0;
    }
    transactions.emplace_back(type, mat);
    trans_queued++;
    cancelled = false;
}

void MaterialsWindow::EnqueueQuote(Material material) {
    Enqueue(Transaction::Quote, material);
}

void MaterialsWindow::EnqueuePurchase(Material material) {
    Enqueue(Transaction::Buy, material);
}
void MaterialsWindow::EnqueueSell(Material material) {
    Enqueue(Transaction::Sell, material);
}

DWORD MaterialsWindow::RequestPurchaseQuote(Material material) {
    GW::Item *item = GetMerchItem(material);
    if (!item) return 0;
    GW::Merchant::QuoteInfo give, recv;
    give.unknown = 0;
    give.item_count = 0;
    give.item_ids = nullptr;
    recv.unknown = 0;
    recv.item_count = 1;
    recv.item_ids = &item->item_id;
    GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderBuy, give, recv);
    return item->item_id;
}

DWORD MaterialsWindow::RequestSellQuote(Material material) {
    GW::Item *item = GetBagItem(material);
    if (!item) return 0;
    GW::Merchant::QuoteInfo give, recv;
    give.unknown = 0;
    give.item_count = 1;
    give.item_ids = &item->item_id;
    recv.unknown = 0;
    recv.item_count = 0;
    recv.item_ids = nullptr;
    GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderSell, give, recv);
    return item->item_id;
}

std::string MaterialsWindow::GetPrice(MaterialsWindow::Material mat1, float fac1,
    MaterialsWindow::Material mat2, float fac2, int extra) const {
    int p1 = price[mat1];
    int p2 = price[mat2];
    if (p1 == PRICE_NOT_AVAILABLE || p2 == PRICE_NOT_AVAILABLE) {
        return "Price: (Material not available)";
    } else if (p1 == PRICE_DEFAULT || p2 == PRICE_DEFAULT) {
        return "Price:  -";
    } else if (p1 == PRICE_COMPUTING_SENT || p2 == PRICE_COMPUTING_SENT) {
        return "Price: Computing (request sent)";
    } else if (p1 == PRICE_COMPUTING_QUEUE || p2 == PRICE_COMPUTING_QUEUE) {
        return "Price: Computing (in queue)";
    } else {
        char buf[128];
        snprintf(buf, 128, "Price: %g k", (p1 * fac1 + p2 * fac2 + extra) / 1000.0f);
        return std::string(buf);
    }
}

void MaterialsWindow::FullConsPriceTooltip() const {
    if (ImGui::IsItemHovered()) {
        char buf[256];
        if (price[Feather] == PRICE_NOT_AVAILABLE
            || price[PileofGlitteringDust] == PRICE_NOT_AVAILABLE
            || price[IronIngot] == PRICE_NOT_AVAILABLE
            || price[Bone] == PRICE_NOT_AVAILABLE) {
            strcpy_s(buf, "Full Conset Price: (Material not available)");
        } else if (price[Feather] < 0
            || price[PileofGlitteringDust] < 0
            || price[IronIngot] < 0
            || price[Bone] < 0) {
            strcpy_s(buf, "Full Conset Price: -");
        } else {
            int p = price[IronIngot] * 10 + price[PileofGlitteringDust] * 10 +
                price[Bone] * 5 + price[Feather] * 5 + 750;
            snprintf(buf, 256, "Full Conset Price: %g k", p / 1000.0f);
        }
        ImGui::SetTooltip(buf);
    }
}

void MaterialsWindow::DrawSettingInternal() {
    ToolboxWindow::DrawSettingInternal();
    ImGui::Checkbox("Automatically manage gold", &manage_gold);
    ImGui::ShowHelp("It will automatically withdraw and deposit gold while buying materials");
    ImGui::Checkbox("Use stock", &use_stock);
    ImGui::ShowHelp("Will take materials in inventory and storage into account when buying materials");    
}
MaterialsWindow::Material MaterialsWindow::GetMaterial(DWORD modelid) {
    switch (modelid) {
    case GW::Constants::ItemID::BoltofCloth:            return BoltofCloth;
    case GW::Constants::ItemID::Bone:                   return Bone;
    case GW::Constants::ItemID::ChitinFragment:         return ChitinFragment;
    case GW::Constants::ItemID::Feather:                return Feather;
    case GW::Constants::ItemID::GraniteSlab:            return GraniteSlab;
    case GW::Constants::ItemID::IronIngot:              return IronIngot;
    case GW::Constants::ItemID::PileofGlitteringDust:   return PileofGlitteringDust;
    case GW::Constants::ItemID::PlantFiber:             return PlantFiber;
    case GW::Constants::ItemID::Scale:                  return Scale;
    case GW::Constants::ItemID::TannedHideSquare:       return TannedHideSquare;
    case GW::Constants::ItemID::WoodPlank:              return WoodPlank;
    case GW::Constants::ItemID::AmberChunk:             return AmberChunk;
    case GW::Constants::ItemID::BoltofDamask:           return BoltofDamask;
    case GW::Constants::ItemID::BoltofLinen:            return BoltofLinen;
    case GW::Constants::ItemID::BoltofSilk:             return BoltofSilk;
    case GW::Constants::ItemID::DeldrimorSteelIngot:    return DeldrimorSteelIngot;
    case GW::Constants::ItemID::Diamond:                return Diamond;
    case GW::Constants::ItemID::ElonianLeatherSquare:   return ElonianLeatherSquare;
    case GW::Constants::ItemID::FurSquare:              return FurSquare;
    case GW::Constants::ItemID::GlobofEctoplasm:        return GlobofEctoplasm;
    case GW::Constants::ItemID::JadeiteShard:           return JadeiteShard;
    case GW::Constants::ItemID::LeatherSquare:          return LeatherSquare;
    case GW::Constants::ItemID::LumpofCharcoal:         return LumpofCharcoal;
    case GW::Constants::ItemID::MonstrousClaw:          return MonstrousClaw;
    case GW::Constants::ItemID::MonstrousEye:           return MonstrousEye;
    case GW::Constants::ItemID::MonstrousFang:          return MonstrousFang;
    case GW::Constants::ItemID::ObsidianShard:          return ObsidianShard;
    case GW::Constants::ItemID::OnyxGemstone:           return OnyxGemstone;
    case GW::Constants::ItemID::RollofParchment:        return RollofParchment;
    case GW::Constants::ItemID::RollofVellum:           return RollofVellum;
    case GW::Constants::ItemID::Ruby:                   return Ruby;
    case GW::Constants::ItemID::Sapphire:               return Sapphire;
    case GW::Constants::ItemID::SpiritwoodPlank:        return SpiritwoodPlank;
    case GW::Constants::ItemID::SteelIngot:             return SteelIngot;
    case GW::Constants::ItemID::TemperedGlassVial:      return TemperedGlassVial;
    case GW::Constants::ItemID::VialofInk:              return VialofInk;
    default:                                            return BoltofCloth;
    }
}

DWORD MaterialsWindow::GetModelID(MaterialsWindow::Material mat) const {
    switch (mat) {
    case BoltofCloth:           return GW::Constants::ItemID::BoltofCloth;
    case Bone:                  return GW::Constants::ItemID::Bone;
    case ChitinFragment:        return GW::Constants::ItemID::ChitinFragment;
    case Feather:               return GW::Constants::ItemID::Feather;
    case GraniteSlab:           return GW::Constants::ItemID::GraniteSlab;
    case IronIngot:             return GW::Constants::ItemID::IronIngot;
    case PileofGlitteringDust:  return GW::Constants::ItemID::PileofGlitteringDust;
    case PlantFiber:            return GW::Constants::ItemID::PlantFiber;
    case Scale:                 return GW::Constants::ItemID::Scale;
    case TannedHideSquare:      return GW::Constants::ItemID::TannedHideSquare;
    case WoodPlank:             return GW::Constants::ItemID::WoodPlank;
    case AmberChunk:            return GW::Constants::ItemID::AmberChunk;
    case BoltofDamask:          return GW::Constants::ItemID::BoltofDamask;
    case BoltofLinen:           return GW::Constants::ItemID::BoltofLinen;
    case BoltofSilk:            return GW::Constants::ItemID::BoltofSilk;
    case DeldrimorSteelIngot:   return GW::Constants::ItemID::DeldrimorSteelIngot;
    case Diamond:               return GW::Constants::ItemID::Diamond;
    case ElonianLeatherSquare:  return GW::Constants::ItemID::ElonianLeatherSquare;
    case FurSquare:             return GW::Constants::ItemID::FurSquare;
    case GlobofEctoplasm:       return GW::Constants::ItemID::GlobofEctoplasm;
    case JadeiteShard:          return GW::Constants::ItemID::JadeiteShard;
    case LeatherSquare:         return GW::Constants::ItemID::LeatherSquare;
    case LumpofCharcoal:        return GW::Constants::ItemID::LumpofCharcoal;
    case MonstrousClaw:         return GW::Constants::ItemID::MonstrousClaw;
    case MonstrousEye:          return GW::Constants::ItemID::MonstrousEye;
    case MonstrousFang:         return GW::Constants::ItemID::MonstrousFang;
    case ObsidianShard:         return GW::Constants::ItemID::ObsidianShard;
    case OnyxGemstone:          return GW::Constants::ItemID::OnyxGemstone;
    case RollofParchment:       return GW::Constants::ItemID::RollofParchment;
    case RollofVellum:          return GW::Constants::ItemID::RollofVellum;
    case Ruby:                  return GW::Constants::ItemID::Ruby;
    case Sapphire:              return GW::Constants::ItemID::Sapphire;
    case SpiritwoodPlank:       return GW::Constants::ItemID::SpiritwoodPlank;
    case SteelIngot:            return GW::Constants::ItemID::SteelIngot;
    case TemperedGlassVial:     return GW::Constants::ItemID::TemperedGlassVial;
    case VialofInk:             return GW::Constants::ItemID::VialofInk;
    default: return 0;
    }
}
