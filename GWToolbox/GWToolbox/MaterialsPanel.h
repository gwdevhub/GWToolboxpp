#pragma once

#include <deque>

#include "ToolboxPanel.h"

class MaterialsPanel : public ToolboxPanel {
	enum Item {
		Essence,
		Grail,
		Armor,
		Powerstone,
		ResScroll,
		Any
	};

public:
	const char* Name() const override { return "Materials Panel"; }
	const char* TabButtonText() const override { return "Materials"; }

	MaterialsPanel(IDirect3DDevice9* pDevice);
	~MaterialsPanel();
	
	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

private:
	int* GetPricePtr(DWORD modelid);
	std::string GetPrice(int p1, float m1,
		int p2, float m2, int extra) const;

	void EnqueueQuote(DWORD modelid);
	void EnqueuePurchase(DWORD modelid);

	void FullConsPriceTooltip() const;

	// returns item id if successful, 0 if error
	int RequestQuote(DWORD modelid);

	IDirect3DTexture9* tex_essence = nullptr;
	IDirect3DTexture9* tex_grail = nullptr;
	IDirect3DTexture9* tex_armor = nullptr;
	IDirect3DTexture9* tex_powerstone = nullptr;
	IDirect3DTexture9* tex_resscroll = nullptr;

	// Negative values have special meanings:
	static const int PRICE_DEFAULT = -1;
	static const int PRICE_COMPUTING_QUEUE = -2;
	static const int PRICE_COMPUTING_SENT = -3;
	static const int PRICE_NOT_AVAILABLE = -4;
	int price_feathers = -1;
	int price_dust = -1;
	int price_fibers = -1;
	int price_bones = -1;
	int price_iron = -1;
	int price_granite = -1;

	int max = 0;
	std::deque<DWORD> quotequeue;
	std::deque<DWORD> purchasequeue;

	DWORD last_request_itemid = 0;
	DWORD last_request_price = 0;
	enum RequestType {
		None,
		Quote,
		Purchase
	} last_request_type = None;
};
