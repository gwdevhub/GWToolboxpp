#pragma once

#include <GWCA\Managers\MerchantMgr.h>

#include <deque>

#include "ToolboxWindow.h"

class MaterialsWindow : public ToolboxWindow {
	enum Item {
		Essence,
		Grail,
		Armor,
		Powerstone,
		ResScroll,
		Any
	};
	enum Material {
		BoltofCloth, Bone, ChitinFragment,
		Feather, GraniteSlab, IronIngot,
		PileofGlitteringDust, PlantFiber,
		Scale, TannedHideSquare, WoodPlank,

		AmberChunk, BoltofDamask, BoltofLinen,
		BoltofSilk, DeldrimorSteelIngot,
		Diamond, ElonianLeatherSquare, FurSquare,
		GlobofEctoplasm, JadeiteShard, LeatherSquare,
		LumpofCharcoal, MonstrousClaw, MonstrousEye,
		MonstrousFang, ObsidianShard, OnyxGemstone,
		RollofParchment, RollofVellum, Ruby,
		Sapphire, SpiritwoodPlank, SteelIngot,
		TemperedGlassVial, VialofInk,
		
		N_MATS
	};

	MaterialsWindow() {};
	~MaterialsWindow() {};
public:
	static MaterialsWindow& Instance() {
		static MaterialsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Materials"; }

	void Initialize() override;
	void Terminate() override;

	void LoadSettings(CSimpleIni* ini) override;
	
	// Update. Will always be called every frame.
	void Update(float delta) override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

private:
	DWORD GetModelID(Material mat) const;
	Material GetMaterial(DWORD modelid);
	std::string GetPrice(Material mat1, float fac1,
		Material mat2, float fac2, int extra) const;

	void EnqueueQuote(Material material);
	void EnqueuePurchase(Material material);
	void EnqueueSell(Material material);

	void FullConsPriceTooltip() const;

	// returns item id if successful, 0 if error
	DWORD RequestPurchaseQuote(Material material);
	DWORD RequestSellQuote(Material material);

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
	int price[N_MATS];

	int max = 0;
	std::deque<Material> quotequeue;
	std::deque<Material> purchasequeue;
	std::deque<Material> sellqueue;
	bool cancelled = false;
	int cancelled_progress = 0;

	DWORD last_request_itemid = 0;
	DWORD last_request_price = 0;
	enum RequestType {
		None,
		Quote,
		Purchase,
		Sell
	} last_request_type = None;
};
