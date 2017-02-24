#include "MaterialsPanel.h"

#include <imgui_internal.h>
#include <d3dx9tex.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\MerchantMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Context\GameContext.h>

#include "GuiUtils.h"
#include "ChatLogger.h"

void MaterialsPanel::Update() {
	if (!quotequeue.empty()) {
		if (last_request_type == None) { // no requests are active
			int itemid = RequestQuote(quotequeue.front());
			if (itemid > 0) {
				quotequeue.pop_front();
				last_request_itemid = itemid;
				last_request_type = Quote;
			} else {
				quotequeue.clear(); // panic
				purchasequeue.clear();
			}
		}
	} else if (!purchasequeue.empty()) {
		if (last_request_type == None) {
			int* price = GetPricePtr(purchasequeue.front());
			if (*price > 0 && (DWORD)(*price) > GW::Items().GetGoldAmountOnCharacter()) {
				ChatLogger::Err("Cannot purchase, not enough gold!");
				purchasequeue.clear();
			} else {
				int itemid = RequestQuote(purchasequeue.front());
				if (itemid > 0) {
					purchasequeue.pop_front();
					last_request_itemid = itemid;
					last_request_type = Purchase;
				} else {
					quotequeue.clear(); // panic
					purchasequeue.clear();
				}
			}
		}
	}
}

MaterialsPanel::MaterialsPanel(IDirect3DDevice9* device) {
	D3DXCreateTextureFromFile(device, GuiUtils::getSubPath("feather.png", "img").c_str(), &texture);

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P235_QuotedItemPrice>(
		[&](GW::Packet::StoC::P235_QuotedItemPrice* pak) -> bool {
		GW::ItemArray items = GW::Items().GetItemArray();
		if (!items.valid()) return false;
		if (pak->itemid >= items.size()) return false;
		GW::Item* item = items[pak->itemid];
		if (item == nullptr) return false;
		int* price = GetPricePtr(item->ModelId);
		if (price) *price = pak->price;
		printf("Received price %d for %d (%d)\n", pak->price, item->ModelId, pak->itemid);

		if (last_request_itemid == pak->itemid) {
			if (last_request_type == Quote) {
				// everything is ok already
				last_request_type = None;
			} else if (last_request_type == Purchase) {
				// buy the item
				last_request_price = pak->price;

				GW::TransactionInfo give;
				give.itemcount = 0;
				give.itemids = nullptr;
				give.itemquantities = nullptr;
				GW::TransactionInfo recv;
				recv.itemcount = 1;
				recv.itemids = &pak->itemid;
				recv.itemquantities = nullptr;
				GW::Merchant().TransactItems(GW::TransactionType::TraderBuy, pak->price, give, 0, recv);
				printf("sending purchase request for %d\n", item->ModelId);
			}
		}

		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P325>(
		[&](GW::Packet::StoC::P325* pak) -> bool {
		// this is just a gold removed update packet, but we use it to know when the transaction
		// was successful. Hopefully people won't deposit exactly that amount while buying.
		if (last_request_type == Purchase && last_request_price == pak->gold) {
			// purchase successful and complete
			last_request_type = None;
		}
		return false;
	});
}

MaterialsPanel::~MaterialsPanel() {
	if (tex_essence) tex_essence->Release(); tex_essence = nullptr;
	if (tex_grail) tex_grail->Release(); tex_grail = nullptr;
	if (tex_armor) tex_armor->Release(); tex_armor = nullptr;
	if (tex_powerstone) tex_powerstone->Release(); tex_powerstone = nullptr;
	if (tex_resscroll) tex_resscroll->Release(); tex_resscroll = nullptr;
}

void MaterialsPanel::Draw(IDirect3DDevice9* pDevice) {
	auto EnsureLoaded = [](IDirect3DDevice9* pDevice, IDirect3DTexture9** tex, const char* filename) -> void {
		if (*tex == nullptr) {
			D3DXCreateTextureFromFile(pDevice, GuiUtils::getSubPath(filename, "img").c_str(), tex);
		}
	};
	EnsureLoaded(pDevice, &tex_essence, "Essence_of_Celerity.png");
	EnsureLoaded(pDevice, &tex_grail, "Grail_of_Might.png");
	EnsureLoaded(pDevice, &tex_armor, "Armor_of_Salvation.png");
	EnsureLoaded(pDevice, &tex_powerstone, "Powerstone_of_Courage.png");
	EnsureLoaded(pDevice, &tex_resscroll, "Scroll_of_Resurrection.png");

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::Begin(Name(), &visible);

	float x, y, h;

	// note: textures are 64 x 64, but both off-center 
	// and with a bunch of empty space. We want to center the image
	// while minimizing the rescaling

	// === Essence ===
	ImGui::Image((ImTextureID)tex_essence, ImVec2(50, 50), 
		ImVec2(4.0f/64, 9.0f/64), ImVec2(47.0f/64, 52.0f/64));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Essence of Celerity\nFeathers and Dust");
	ImGui::SameLine();
	x = ImGui::GetCursorPosX();
	y = ImGui::GetCursorPosY();
	ImGui::Text(GetPrice(price_feathers, 5.0f, price_dust, 5.0f, 250).c_str());
	FullConsPriceTooltip();
	ImGui::SameLine(ImGui::GetWindowWidth() -100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button("Price Check###essencepc", ImVec2(100.0f, 0))) {
		EnqueueQuote(GW::Constants::ItemID::Feathers);
		EnqueueQuote(GW::Constants::ItemID::Dust);
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
	if (ImGui::Button("Buy###essencebuy", ImVec2(100.0f, 0))) {
		for (int i = 0; i < 5 * qty_essence; ++i) {
			EnqueuePurchase(GW::Constants::ItemID::Feathers);
			EnqueuePurchase(GW::Constants::ItemID::Dust);
		}
	}

	ImGui::Separator();
	// === Grail ===
	ImGui::Image((ImTextureID)tex_grail, ImVec2(50, 50),
		ImVec2(3.0f/64, 11.0f/64), ImVec2(49.0f/64, 57.0f/64));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grail of Might\nIron and Dust");
	ImGui::SameLine();
	x = ImGui::GetCursorPosX();
	y = ImGui::GetCursorPosY();
	ImGui::Text(GetPrice(price_iron, 5.0f, price_dust, 5.0f, 250).c_str());
	FullConsPriceTooltip();
	ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button("Price Check###grailpc", ImVec2(100.0f, 0))) {
		EnqueueQuote(GW::Constants::ItemID::Iron);
		EnqueueQuote(GW::Constants::ItemID::Dust);
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
	if (ImGui::Button("Buy###grailbuy", ImVec2(100.0f, 0))) {
		for (int i = 0; i < 5 * qty_grail; ++i) {
			EnqueuePurchase(GW::Constants::ItemID::Iron);
			EnqueuePurchase(GW::Constants::ItemID::Dust);
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
	ImGui::Text(GetPrice(price_iron, 5.0f, price_bones, 5.0f, 250).c_str());
	FullConsPriceTooltip();
	ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button("Price Check###armorpc", ImVec2(100.0f, 0))) {
		EnqueueQuote(GW::Constants::ItemID::Iron);
		EnqueueQuote(GW::Constants::ItemID::Bones);
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
	if (ImGui::Button("Buy###armorbuy", ImVec2(100.0f, 0))) {
		for (int i = 0; i < 5 * qty_armor; ++i) {
			EnqueuePurchase(GW::Constants::ItemID::Iron);
			EnqueuePurchase(GW::Constants::ItemID::Bones);
		}
	}

	ImGui::Separator();
	// === Powerstone ===
	ImGui::Image((ImTextureID)tex_powerstone, ImVec2(50, 50), 
		ImVec2(0, 6.0f/64), ImVec2(54.0f/64, 60.0f/64));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Powerstone of Courage\nGranite and Dust");
	ImGui::SameLine();
	x = ImGui::GetCursorPosX();
	y = ImGui::GetCursorPosY();
	ImGui::Text(GetPrice(price_granite, 10.0f, price_dust, 10.0f, 1000).c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button("Price Check###pstonepc", ImVec2(100.0f, 0))) {
		EnqueueQuote(GW::Constants::ItemID::Granite);
		EnqueueQuote(GW::Constants::ItemID::Dust);
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
	if (ImGui::Button("Buy###pstonebuy", ImVec2(100.0f, 0))) {
		for (int i = 0; i < 10 * qty_pstone; ++i) {
			EnqueuePurchase(GW::Constants::ItemID::Granite);
			EnqueuePurchase(GW::Constants::ItemID::Dust);
		}
	}

	ImGui::Separator();
	// === Res scroll ===
	ImGui::Image((ImTextureID)tex_resscroll, ImVec2(50, 50),
		ImVec2(1.0f/64, 4.0f/64), ImVec2(56.0f/64, 59.0f/64));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scroll of Resurrection\nFibers and Bones");
	ImGui::SameLine();
	x = ImGui::GetCursorPosX();
	y = ImGui::GetCursorPosY();
	ImGui::Text(GetPrice(price_fibers, 2.5f, price_bones, 2.5f, 250).c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button("Price Check###resscrollpc", ImVec2(100.0f, 0))) {
		EnqueueQuote(GW::Constants::ItemID::Fibers);
		EnqueueQuote(GW::Constants::ItemID::Bones);
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
	if (ImGui::Button("Buy###resscrollbuy", ImVec2(100.0f, 0))) {
		for (int i = 0; i < qty_resscroll; ++i) { // for each scroll
			int qty = (i % 2 == 0 ? 2 : 3);
			for (int j = 0; j < qty; ++j) {
				EnqueuePurchase(GW::Constants::ItemID::Fibers);
				EnqueuePurchase(GW::Constants::ItemID::Bones);
			}
		}
	}

	ImGui::Separator();
	// === generic materials ===
	static int index_mats = 0;
	static int qty_mats = 1;
	static const char* mats[] = {
		"10 Bones",
		"10 Iron Ingots",
		"10 Tanned Hide Squares",
		"10 Scales",
		"10 Chitin Fragments",
		"10 Bolts of Cloth",
		"10 Wood Planks",
		"10 Granite Slabs",
		"10 Piles of Glittering Dust",
		"10 Feathers",
		"10 Fibers",
	};
	ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() -100.0f 
		- ImGui::GetStyle().ItemSpacing.x * 2) / 2);
	ImGui::Combo("###material", &index_mats, mats, 11);
	ImGui::SameLine();
	ImGui::InputInt("###matsqty", &qty_mats);
	if (qty_mats < 1) qty_mats = 1;
	// todo: add count and change layout. maybe popup?
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Buy###materialbuy", ImVec2(100.0f, 0))) {
		DWORD id = 0;
		switch (index_mats) {
		case 0: id = GW::Constants::ItemID::Bones; break;
		case 1: id = GW::Constants::ItemID::Iron; break;
		case 2: id = GW::Constants::ItemID::TannedHideSquares; break;
		case 3: id = GW::Constants::ItemID::Scales; break;
		case 4: id = GW::Constants::ItemID::ChitinFragments; break;
		case 5: id = GW::Constants::ItemID::BoltsOfCloth; break;
		case 6: id = GW::Constants::ItemID::WoodPlanks; break;
		case 7: id = GW::Constants::ItemID::Granite; break;
		case 8: id = GW::Constants::ItemID::Dust; break;
		case 9: id = GW::Constants::ItemID::Feathers; break;
		case 10: id = GW::Constants::ItemID::Fibers; break;
		default: break;
		}
		for (int i = 0; i < qty_mats; ++i) {
			EnqueuePurchase(id);
		}
	}

	ImGui::Separator();
	int to_do = quotequeue.size() + purchasequeue.size();
	int done = max - to_do;
	float progress = 0.0f;
	if (max > 0) progress = (float)(done) / max;
	char buf[256];
	sprintf_s(buf, "%.2f [%d / %d]", progress * 100.0f, done, max);
	ImGui::ProgressBar(progress, ImVec2(ImGui::GetWindowContentRegionWidth()
		- 100.0f - ImGui::GetStyle().ItemSpacing.x, 0), buf);
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(100.0f, 0))) {
		purchasequeue.clear();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel the current price check and purchase queue");

	ImGui::End();
}

int* MaterialsPanel::GetPricePtr(DWORD modelid) {
	switch (modelid) {
	case GW::Constants::ItemID::Feathers: return &price_feathers;
	case GW::Constants::ItemID::Dust:     return &price_dust;
	case GW::Constants::ItemID::Fibers:   return &price_fibers;
	case GW::Constants::ItemID::Bones:    return &price_bones;
	case GW::Constants::ItemID::Iron:     return &price_iron;
	case GW::Constants::ItemID::Granite:  return &price_granite;
	}
	return nullptr;
}

void MaterialsPanel::EnqueueQuote(DWORD modelid) {
	if (quotequeue.empty() && purchasequeue.empty()) max = 0;
	quotequeue.push_back(modelid);
	++max;
}
void MaterialsPanel::EnqueuePurchase(DWORD modelid) {
	if (quotequeue.empty() && purchasequeue.empty()) max = 0;
	purchasequeue.push_back(modelid);
	++max;
}

int MaterialsPanel::RequestQuote(DWORD modelid) {
	GW::Item* item = nullptr;
	GW::ItemArray items = GW::Items().GetItemArray();
	if (!items.valid()) return false;
	for (DWORD i = 0; i < items.size(); ++i) {
		GW::Item* cur = items[i];
		if (cur 
			&& cur->ModelId == modelid 
			&& cur->bag == nullptr 
			&& cur->AgentId == 0) {
			item = cur;
			break;
		}
	}

	if (item == nullptr) { // mark the price as 'cannot compute'
		int* price = GetPricePtr(modelid);
		if (price && *price < 0) *price = PRICE_NOT_AVAILABLE;
		return 0;
	}

	DWORD itemid = item->ItemId;
	GW::QuoteInfo give;
	give.unknown = 0;
	give.itemcount = 0;
	give.itemids = nullptr;
	GW::QuoteInfo recv;
	recv.unknown = 0;
	recv.itemcount = 1;
	recv.itemids = &itemid;
	GW::Merchant().RequestQuote(GW::TransactionType::TraderBuy, give, recv);
	printf("Sent request for %d (%d)\n", modelid, itemid);

	int* price = GetPricePtr(modelid);
	if (price && *price < 0) *price = PRICE_COMPUTING_SENT;

	return itemid;
}

std::string MaterialsPanel::GetPrice(int p1, float m1, int p2, float m2, int extra) const {
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
		sprintf_s(buf, "Price: %g k", (p1 * m1 + p2 * m2 + extra) / 1000.0f);
		return std::string(buf);
	}
}

void MaterialsPanel::FullConsPriceTooltip() const {
	if (ImGui::IsItemHovered()) {
		char buf[256];
		if (price_feathers == PRICE_NOT_AVAILABLE
			|| price_dust == PRICE_NOT_AVAILABLE
			|| price_iron == PRICE_NOT_AVAILABLE
			|| price_bones == PRICE_NOT_AVAILABLE) {
			strcpy_s(buf, "Full Conset Price: (Material not available)");
		} else if (price_feathers < 0
			|| price_dust < 0
			|| price_iron < 0
			|| price_bones < 0) {
			strcpy_s(buf, "Full Conset Price: -");
		} else {
			int price = price_iron * 10 + price_dust * 10 + price_bones * 5 + price_feathers * 5 + 750;
			sprintf_s(buf, "Full Conset Price: %g k", price / 1000.0f);
		}
		ImGui::SetTooltip(buf);
	}
}
