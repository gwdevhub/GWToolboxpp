#include "MaterialsWindow.h"

#include <imgui_internal.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\MerchantMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Context\GameContext.h>

#include "GuiUtils.h"
#include <logger.h>
#include <Modules\Resources.h>

void MaterialsWindow::Update(float delta) {
	if (cancelled) return;
	if (!quotequeue.empty()) {
		if (last_request_type == None) { // no requests are active
			const Material mat = quotequeue.front();
			int itemid = RequestPurchaseQuote(mat);
			if (itemid > 0) {
				quotequeue.pop_front();
				last_request_itemid = itemid;
				last_request_type = Quote;
			} else {
				quotequeue.clear(); // panic
			}
		}
	} else if (!sellqueue.empty()) {
		if (last_request_type == None) {
			const Material mat = sellqueue.front();
			if (price[mat] > 0 && GW::Items::GetGoldAmountOnCharacter() + (DWORD)price[mat] > 100 * 1000) {
				Log::Error("Cannot sell, too much gold!");
				sellqueue.clear();
			} else {
				int itemid = RequestSellQuote(mat);
				if (itemid > 0) {
					sellqueue.pop_front();
					last_request_itemid = itemid;
					last_request_type = Sell;
				} else {
					sellqueue.clear(); // panic
				}
			}
		}
	} else if (!purchasequeue.empty()) {
		if (last_request_type == None) {
			const Material mat = purchasequeue.front();
			if (price[mat] > 0 && (DWORD)price[mat] > GW::Items::GetGoldAmountOnCharacter()) {
				Log::Error("Cannot purchase, not enough gold!");
				purchasequeue.clear();
			} else {
				int itemid = RequestPurchaseQuote(mat);
				if (itemid > 0) {
					purchasequeue.pop_front();
					last_request_itemid = itemid;
					last_request_type = Purchase;
				} else {
					purchasequeue.clear(); // panic
				}
			}
		}
	}
}

void MaterialsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath("img/icons", "feather.png"), IDB_Icon_Feather);

	Resources::Instance().LoadTextureAsync(&tex_essence, Resources::GetPath("img/materials", "Essence_of_Celerity.png"), IDB_Mat_Essence);
	Resources::Instance().LoadTextureAsync(&tex_grail, Resources::GetPath("img/materials", "Grail_of_Might.png"), IDB_Mat_Grail);
	Resources::Instance().LoadTextureAsync(&tex_armor, Resources::GetPath("img/materials", "Armor_of_Salvation.png"), IDB_Mat_Armor);
	Resources::Instance().LoadTextureAsync(&tex_powerstone, Resources::GetPath("img/materials", "Powerstone_of_Courage.png"), IDB_Mat_Powerstone);
	Resources::Instance().LoadTextureAsync(&tex_resscroll, Resources::GetPath("img/materials", "Scroll_of_Resurrection.png"), IDB_Mat_ResScroll);
	
	for (int i = 0; i < N_MATS; ++i) {
		price[i] = PRICE_DEFAULT;
	}
	
	GW::StoC::AddCallback<GW::Packet::StoC::P235_QuotedItemPrice>(
		[&](GW::Packet::StoC::P235_QuotedItemPrice* pak) -> bool {
		GW::ItemArray items = GW::Items::GetItemArray();
		if (!items.valid()) return false;
		if (pak->itemid >= items.size()) return false;
		GW::Item* item = items[pak->itemid];
		if (item == nullptr) return false;
		price[GetMaterial(item->ModelId)] = pak->price;
		//printf("Received price %d for %d (item %d)\n", pak->price, item->ModelId, pak->itemid);

		if (last_request_itemid == pak->itemid && !cancelled) {
			if (last_request_type == Quote) {
				// everything is ok already
				last_request_type = None;
			} else if (last_request_type == Purchase) {
				// buy the item
				last_request_price = pak->price;
				GW::Merchant::TransactionInfo give;
				give.itemcount = 0;
				give.itemids = nullptr;
				give.itemquantities = nullptr;
				GW::Merchant::TransactionInfo recv;
				recv.itemcount = 1;
				recv.itemids = &pak->itemid;
				recv.itemquantities = nullptr;
				GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderBuy, pak->price, give, 0, recv);
				//printf("sending purchase request for %d (price=%d)\n", item->ModelId, pak->price);
			} else if (last_request_type == Sell) {
				// sell the item
				last_request_price = pak->price;

				GW::Merchant::TransactionInfo give;
				give.itemcount = 1;
				give.itemids = &pak->itemid;
				give.itemquantities = nullptr;
				GW::Merchant::TransactionInfo recv;
				recv.itemcount = 0;
				recv.itemids = nullptr;
				recv.itemquantities = nullptr;
				GW::Merchant::TransactItems(GW::Merchant::TransactionType::TraderSell, 0, give, pak->price, recv);
				//printf("sending sell request for %d (price=%d)\n", item->ModelId, pak->price);
			}
		}
		return false;
	});

	// Those 2 are just gold update packets, but we use them to know when the transaction
	// was successful. Hopefully people won't deposit or widthrawal exactly that amount while buying.
	GW::StoC::AddCallback<GW::Packet::StoC::P310>(
		[&](GW::Packet::StoC::P310* pak) -> bool {
		if (last_request_type == Sell && last_request_price == pak->gold) {
			last_request_type = None;
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::P325>(
		[&](GW::Packet::StoC::P325* pak) -> bool {
		//printf("Removed %d gold\n", pak->gold);
		if (last_request_type == Purchase && last_request_price == pak->gold) {
			last_request_type = None;
		}
		return false;
	});
}

void MaterialsWindow::Terminate() {
	if (tex_essence) tex_essence->Release(); tex_essence = nullptr;
	if (tex_grail) tex_grail->Release(); tex_grail = nullptr;
	if (tex_armor) tex_armor->Release(); tex_armor = nullptr;
	if (tex_powerstone) tex_powerstone->Release(); tex_powerstone = nullptr;
	if (tex_resscroll) tex_resscroll->Release(); tex_resscroll = nullptr;
}

void MaterialsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
}

void MaterialsWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

		float x, y, h;

		// note: textures are 64 x 64, but both off-center 
		// and with a bunch of empty space. We want to center the image
		// while minimizing the rescaling

		// === Essence ===
		ImGui::Image((ImTextureID)tex_essence, ImVec2(50, 50),
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
		h = ImGui::GetCurrentWindow()->DC.LastItemRect.GetHeight();
		static int qty_essence = 1;
		ImGui::SetCursorPosX(x);
		ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
		ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputInt("###essenceqty", &qty_essence);
		if (qty_essence < 1) qty_essence = 1;
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Buy##essence", ImVec2(100.0f, 0))) {
			for (int i = 0; i < 5 * qty_essence; ++i) {
				EnqueuePurchase(Feather);
				EnqueuePurchase(PileofGlitteringDust);
			}
		}

		ImGui::Separator();
		// === Grail ===
		ImGui::Image((ImTextureID)tex_grail, ImVec2(50, 50),
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
		h = ImGui::GetCurrentWindow()->DC.LastItemRect.GetHeight();
		static int qty_grail = 1;
		ImGui::SetCursorPosX(x);
		ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
		ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputInt("###grailqty", &qty_grail);
		if (qty_grail < 1) qty_grail = 1;
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Buy##grail", ImVec2(100.0f, 0))) {
			for (int i = 0; i < 5 * qty_grail; ++i) {
				EnqueuePurchase(IronIngot);
				EnqueuePurchase(PileofGlitteringDust);
			}
		}

		ImGui::Separator();
		// === Armor ===
		ImGui::Image((ImTextureID)tex_armor, ImVec2(50, 50),
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
		h = ImGui::GetCurrentWindow()->DC.LastItemRect.GetHeight();
		static int qty_armor = 1;
		ImGui::SetCursorPosX(x);
		ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
		ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputInt("###armorqty", &qty_armor);
		if (qty_armor < 1) qty_armor = 1;
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Buy##armor", ImVec2(100.0f, 0))) {
			for (int i = 0; i < 5 * qty_armor; ++i) {
				EnqueuePurchase(IronIngot);
				EnqueuePurchase(Bone);
			}
		}

		ImGui::Separator();
		// === Powerstone ===
		ImGui::Image((ImTextureID)tex_powerstone, ImVec2(50, 50),
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
		h = ImGui::GetCurrentWindow()->DC.LastItemRect.GetHeight();
		static int qty_pstone = 1;
		ImGui::SetCursorPosX(x);
		ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
		ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputInt("###pstoneqty", &qty_pstone);
		if (qty_pstone < 1) qty_pstone = 1;
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Buy##pstone", ImVec2(100.0f, 0))) {
			for (int i = 0; i < 10 * qty_pstone; ++i) {
				EnqueuePurchase(GraniteSlab);
				EnqueuePurchase(PileofGlitteringDust);
			}
		}

		ImGui::Separator();
		// === Res scroll ===
		ImGui::Image((ImTextureID)tex_resscroll, ImVec2(50, 50),
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
		h = ImGui::GetCurrentWindow()->DC.LastItemRect.GetHeight();
		static int qty_resscroll = 1;
		ImGui::SetCursorPosX(x);
		ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
		ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputInt("###resscrollqty", &qty_resscroll);
		if (qty_resscroll < 1) qty_resscroll = 1;
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Buy##resscroll", ImVec2(100.0f, 0))) {
			for (int i = 0; i < qty_resscroll; ++i) { // for each scroll
				int qty = (i % 2 == 0 ? 2 : 3);
				for (int j = 0; j < qty; ++j) {
					EnqueuePurchase(PlantFiber);
					EnqueuePurchase(Bone);
				}
			}
		}

		ImGui::Separator();

		float width2 = 100.0f;
		float width1 = (ImGui::GetWindowContentRegionWidth() - width2 - 100.0f - ImGui::GetStyle().ItemSpacing.x * 2);

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
			for (int i = 0; i < common_qty; ++i) {
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
			for (int i = 0; i < rare_qty; ++i) {
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
		int to_do = quotequeue.size() + purchasequeue.size() + sellqueue.size();;
		int done = cancelled ? cancelled_progress : max - to_do;
		float progress = 0.0f;
		if (max > 0) progress = (float)(done) / max;
		const char* status = "";
		if (cancelled) status = "Cancelled";
		else if (to_do > 0) status = "Working";
		else status = "Ready";
		ImGui::Text("%s [%d / %d]", status, done, max);
		ImGui::SameLine(width1 + ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ItemSpacing.x);
		ImGui::ProgressBar(progress, ImVec2(width2, 0));
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100.0f, 0))) {
			purchasequeue.clear();
			quotequeue.clear();
			sellqueue.clear();
			cancelled = true;
			cancelled_progress = done;
			last_request_type = None;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel the current queue of operations");
	}
	ImGui::End();
}

void MaterialsWindow::EnqueueQuote(Material material) {
	if (quotequeue.empty() && purchasequeue.empty() && sellqueue.empty()) max = 0;
	quotequeue.push_back(material);
	cancelled = false;
	++max;
}
void MaterialsWindow::EnqueuePurchase(Material material) {
	if (quotequeue.empty() && purchasequeue.empty() && sellqueue.empty()) max = 0;
	purchasequeue.push_back(material);
	cancelled = false;
	++max;
}
void MaterialsWindow::EnqueueSell(Material material) {
	if (quotequeue.empty() && purchasequeue.empty() && sellqueue.empty()) max = 0;
	sellqueue.push_back(material);
	cancelled = false;
	++max;
}

DWORD MaterialsWindow::RequestPurchaseQuote(Material material) {
	DWORD modelid = GetModelID(material);
	GW::Item* item = nullptr;
	GW::ItemArray items = GW::Items::GetItemArray();
	if (!items.valid()) return false;
	for (DWORD i = 0; i < items.size(); ++i) {
		GW::Item* cur = items[i];
		if (cur 
			&& cur->ModelId == modelid 
			&& cur->AgentId == 0
			&& cur->Bag == nullptr
			&& cur->Interaction == 8200
			&& cur->Quantity == 1) {
			item = cur;
			break;
		}
	}

	if (item == nullptr) { // mark the price as 'cannot compute'
		if (price[material] < 0) price[material] = PRICE_NOT_AVAILABLE;
		return 0;
	}

	DWORD itemid = item->ItemId;
	GW::Merchant::QuoteInfo give;
	give.unknown = 0;
	give.itemcount = 0;
	give.itemids = nullptr;
	GW::Merchant::QuoteInfo recv;
	recv.unknown = 0;
	recv.itemcount = 1;
	recv.itemids = &itemid;
	GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderBuy, give, recv);
	//printf("Sent purchase request for %d (item %d)\n", modelid, itemid);

	if (price[material] < 0) price[material] = PRICE_COMPUTING_SENT;
	return itemid;
}

DWORD MaterialsWindow::RequestSellQuote(Material material) {
	DWORD modelid = GetModelID(material);
	GW::Item* item = nullptr;
	GW::ItemArray items = GW::Items::GetItemArray();
	if (!items.valid()) return false;
	for (DWORD i = 0; i < items.size(); ++i) {
		GW::Item* cur = items[i];
		if (cur 
			&& cur->ModelId == modelid 
			&& cur->AgentId == 0
			&& cur->Bag != nullptr
			&& cur->Bag->Index < 5
			&& cur->Interaction == 8200) {
			item = cur;
			break;
		}
	}

	if (item == nullptr) { // mark the price as 'cannot compute'
		if (price[material] < 0) price[material] = PRICE_NOT_AVAILABLE;
		return 0;
	}

	DWORD itemid = item->ItemId;
	GW::Merchant::QuoteInfo give;
	give.unknown = 0;
	give.itemcount = 1;
	give.itemids = &itemid;
	GW::Merchant::QuoteInfo recv;
	recv.unknown = 0;
	recv.itemcount = 0;
	recv.itemids = nullptr;
	GW::Merchant::RequestQuote(GW::Merchant::TransactionType::TraderSell, give, recv);
	//printf("Sent sell request for %d (item %d)\n", modelid, itemid);

	if (price[material] < 0) price[material] = PRICE_COMPUTING_SENT;

	return itemid;
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

MaterialsWindow::Material MaterialsWindow::GetMaterial(DWORD modelid) {
	switch (modelid) {
	case GW::Constants::ItemID::BoltofCloth: 			return BoltofCloth;
	case GW::Constants::ItemID::Bone: 					return Bone;
	case GW::Constants::ItemID::ChitinFragment: 		return ChitinFragment;
	case GW::Constants::ItemID::Feather: 				return Feather;
	case GW::Constants::ItemID::GraniteSlab: 			return GraniteSlab;
	case GW::Constants::ItemID::IronIngot: 				return IronIngot;
	case GW::Constants::ItemID::PileofGlitteringDust:	return PileofGlitteringDust;
	case GW::Constants::ItemID::PlantFiber: 			return PlantFiber;
	case GW::Constants::ItemID::Scale: 					return Scale;
	case GW::Constants::ItemID::TannedHideSquare: 		return TannedHideSquare;
	case GW::Constants::ItemID::WoodPlank: 				return WoodPlank;
	case GW::Constants::ItemID::AmberChunk: 			return AmberChunk;
	case GW::Constants::ItemID::BoltofDamask: 			return BoltofDamask;
	case GW::Constants::ItemID::BoltofLinen: 			return BoltofLinen;
	case GW::Constants::ItemID::BoltofSilk: 			return BoltofSilk;
	case GW::Constants::ItemID::DeldrimorSteelIngot:	return DeldrimorSteelIngot;
	case GW::Constants::ItemID::Diamond: 				return Diamond;
	case GW::Constants::ItemID::ElonianLeatherSquare:	return ElonianLeatherSquare;
	case GW::Constants::ItemID::FurSquare: 				return FurSquare;
	case GW::Constants::ItemID::GlobofEctoplasm: 		return GlobofEctoplasm;
	case GW::Constants::ItemID::JadeiteShard: 			return JadeiteShard;
	case GW::Constants::ItemID::LeatherSquare: 			return LeatherSquare;
	case GW::Constants::ItemID::LumpofCharcoal: 		return LumpofCharcoal;
	case GW::Constants::ItemID::MonstrousClaw: 			return MonstrousClaw;
	case GW::Constants::ItemID::MonstrousEye: 			return MonstrousEye;
	case GW::Constants::ItemID::MonstrousFang: 			return MonstrousFang;
	case GW::Constants::ItemID::ObsidianShard: 			return ObsidianShard;
	case GW::Constants::ItemID::OnyxGemstone: 			return OnyxGemstone;
	case GW::Constants::ItemID::RollofParchment: 		return RollofParchment;
	case GW::Constants::ItemID::RollofVellum: 			return RollofVellum;
	case GW::Constants::ItemID::Ruby: 					return Ruby;
	case GW::Constants::ItemID::Sapphire: 				return Sapphire;
	case GW::Constants::ItemID::SpiritwoodPlank: 		return SpiritwoodPlank;
	case GW::Constants::ItemID::SteelIngot: 			return SteelIngot;
	case GW::Constants::ItemID::TemperedGlassVial: 		return TemperedGlassVial;
	case GW::Constants::ItemID::VialofInk: 				return VialofInk;
	default:											return BoltofCloth;
	}
}

DWORD MaterialsWindow::GetModelID(MaterialsWindow::Material mat) const {
	switch (mat) {
	case BoltofCloth: 			return GW::Constants::ItemID::BoltofCloth;
	case Bone: 					return GW::Constants::ItemID::Bone;
	case ChitinFragment: 		return GW::Constants::ItemID::ChitinFragment;
	case Feather: 				return GW::Constants::ItemID::Feather;
	case GraniteSlab: 			return GW::Constants::ItemID::GraniteSlab;
	case IronIngot: 			return GW::Constants::ItemID::IronIngot;
	case PileofGlitteringDust:	return GW::Constants::ItemID::PileofGlitteringDust;
	case PlantFiber: 			return GW::Constants::ItemID::PlantFiber;
	case Scale: 				return GW::Constants::ItemID::Scale;
	case TannedHideSquare: 		return GW::Constants::ItemID::TannedHideSquare;
	case WoodPlank: 			return GW::Constants::ItemID::WoodPlank;
	case AmberChunk: 			return GW::Constants::ItemID::AmberChunk;
	case BoltofDamask: 			return GW::Constants::ItemID::BoltofDamask;
	case BoltofLinen: 			return GW::Constants::ItemID::BoltofLinen;
	case BoltofSilk: 			return GW::Constants::ItemID::BoltofSilk;
	case DeldrimorSteelIngot:	return GW::Constants::ItemID::DeldrimorSteelIngot;
	case Diamond: 				return GW::Constants::ItemID::Diamond;
	case ElonianLeatherSquare:	return GW::Constants::ItemID::ElonianLeatherSquare;
	case FurSquare: 			return GW::Constants::ItemID::FurSquare;
	case GlobofEctoplasm: 		return GW::Constants::ItemID::GlobofEctoplasm;
	case JadeiteShard: 			return GW::Constants::ItemID::JadeiteShard;
	case LeatherSquare: 		return GW::Constants::ItemID::LeatherSquare;
	case LumpofCharcoal: 		return GW::Constants::ItemID::LumpofCharcoal;
	case MonstrousClaw: 		return GW::Constants::ItemID::MonstrousClaw;
	case MonstrousEye: 			return GW::Constants::ItemID::MonstrousEye;
	case MonstrousFang: 		return GW::Constants::ItemID::MonstrousFang;
	case ObsidianShard: 		return GW::Constants::ItemID::ObsidianShard;
	case OnyxGemstone: 			return GW::Constants::ItemID::OnyxGemstone;
	case RollofParchment: 		return GW::Constants::ItemID::RollofParchment;
	case RollofVellum: 			return GW::Constants::ItemID::RollofVellum;
	case Ruby: 					return GW::Constants::ItemID::Ruby;
	case Sapphire: 				return GW::Constants::ItemID::Sapphire;
	case SpiritwoodPlank: 		return GW::Constants::ItemID::SpiritwoodPlank;
	case SteelIngot: 			return GW::Constants::ItemID::SteelIngot;
	case TemperedGlassVial: 	return GW::Constants::ItemID::TemperedGlassVial;
	case VialofInk: 			return GW::Constants::ItemID::VialofInk;
	default: return 0;
	}
}
