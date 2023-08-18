#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWindow.h>

class MaterialsWindow : public ToolboxWindow {
    MaterialsWindow() = default;
    ~MaterialsWindow() override = default;

    enum Item {
        Essence,
        Grail,
        Armor,
        Powerstone,
        ResScroll,
        Any
    };

    enum Material {
        BoltofCloth,
        Bone,
        ChitinFragment,
        Feather,
        GraniteSlab,
        IronIngot,
        PileofGlitteringDust,
        PlantFiber,
        Scale,
        TannedHideSquare,
        WoodPlank,

        AmberChunk,
        BoltofDamask,
        BoltofLinen,
        BoltofSilk,
        DeldrimorSteelIngot,
        Diamond,
        ElonianLeatherSquare,
        FurSquare,
        GlobofEctoplasm,
        JadeiteShard,
        LeatherSquare,
        LumpofCharcoal,
        MonstrousClaw,
        MonstrousEye,
        MonstrousFang,
        ObsidianShard,
        OnyxGemstone,
        RollofParchment,
        RollofVellum,
        Ruby,
        Sapphire,
        SpiritwoodPlank,
        SteelIngot,
        TemperedGlassVial,
        VialofInk,

        N_MATS
    };

public:
    static MaterialsWindow& Instance()
    {
        static MaterialsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Materials"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_FEATHER_ALT; }

    void Initialize() override;
    void Terminate() override;

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    bool GetIsInProgress() const;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

private:
    [[nodiscard]] static DWORD GetModelID(Material mat);
    static Material GetMaterial(DWORD modelid);
    [[nodiscard]] std::string GetPrice(Material mat1, float fac1,
                                       Material mat2, float fac2, int extra) const;

    void FullConsPriceTooltip() const;

    // returns item id if successful, 0 if error
    DWORD RequestPurchaseQuote(Material material) const;
    static DWORD RequestSellQuote(Material material);

    IDirect3DTexture9** tex_essence = nullptr;
    IDirect3DTexture9** tex_grail = nullptr;
    IDirect3DTexture9** tex_armor = nullptr;
    IDirect3DTexture9** tex_powerstone = nullptr;
    IDirect3DTexture9** tex_resscroll = nullptr;

    // Negative values have special meanings:
    static constexpr auto PRICE_DEFAULT = -1;
    static constexpr auto PRICE_COMPUTING_QUEUE = -2;
    static constexpr auto PRICE_COMPUTING_SENT = -3;
    static constexpr auto PRICE_NOT_AVAILABLE = -4;
    int price[N_MATS] = {};

    // int max = 0;
    [[nodiscard]] GW::Item* GetMerchItem(Material mat) const;
    [[nodiscard]] static GW::Item* GetBagItem(Material mat);

    struct Transaction {
        enum Type { Sell, Buy, Quote };

        Type type;
        uint32_t item_id;
        Material material;

        Transaction(const Type t, const Material mat)
            : type(t)
            , item_id(0)
            , material(mat) { }
    };

    void Cancel();
    void Dequeue();
    void Enqueue(Transaction::Type type, Material mat);
    void EnqueueQuote(Material material);
    void EnqueuePurchase(Material material);
    void EnqueueSell(Material material);

    std::vector<GW::ItemID> merch_items{};

    std::deque<Transaction> transactions{};
    bool quote_pending = false;
    bool trans_pending = false;
    DWORD quote_pending_time = 0;
    DWORD trans_pending_time = 0;

    bool cancelled = false;
    size_t trans_queued = 0;
    size_t trans_done = 0;

    bool manage_gold = false;
    bool use_stock = false;

    GW::HookEntry QuotedItemPrice_Entry;
    GW::HookEntry TransactionDone_Entry;
    GW::HookEntry ItemStreamEnd_Entry;
};
