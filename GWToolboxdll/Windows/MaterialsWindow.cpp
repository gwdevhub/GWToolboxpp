#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MerchantMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>

#include <Constants/EncStrings.h>

#include <Windows/MaterialsWindow.h>
#include <Modules/GwDatTextureModule.h>
#include <Timer.h>
#include <GWCA/GameEntities/Frame.h>

namespace {

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
    int price[(uint32_t)GW::Constants::MaterialSlot::Count] = {};


    bool quote_pending = false;
    bool trans_pending = false;
    bool trans_complete = false;
    DWORD quote_pending_time = 0;
    DWORD trans_pending_time = 0;

    bool cancelled = false;
    size_t trans_queued = 0;
    size_t trans_done = 0;

    bool manage_gold = false;
    bool use_stock = false;

    GW::HookEntry PostUIMessage_Entry;


    clock_t last_transaction = 0;

    const std::unordered_map<GW::Constants::MaterialSlot, const wchar_t*> material_enc_strings = {
        {GW::Constants::MaterialSlot::BoltofCloth, GW::EncStrings::BoltofCloth},
        {GW::Constants::MaterialSlot::Bone, GW::EncStrings::Bone},
        {GW::Constants::MaterialSlot::ChitinFragment, GW::EncStrings::ChitinFragment},
        {GW::Constants::MaterialSlot::Feather, GW::EncStrings::Feather},
        {GW::Constants::MaterialSlot::GraniteSlab, GW::EncStrings::GraniteSlab},
        {GW::Constants::MaterialSlot::IronIngot, GW::EncStrings::IronIngot},
        {GW::Constants::MaterialSlot::PileofGlitteringDust, GW::EncStrings::PileofGlitteringDust},
        {GW::Constants::MaterialSlot::PlantFiber, GW::EncStrings::PlantFiber},
        {GW::Constants::MaterialSlot::Scale, GW::EncStrings::Scale},
        {GW::Constants::MaterialSlot::TannedHideSquare, GW::EncStrings::TannedHideSquare},
        {GW::Constants::MaterialSlot::WoodPlank, GW::EncStrings::WoodPlank},
        {GW::Constants::MaterialSlot::AmberChunk, GW::EncStrings::AmberChunk},
        {GW::Constants::MaterialSlot::BoltofDamask, GW::EncStrings::BoltofDamask},
        {GW::Constants::MaterialSlot::BoltofLinen, GW::EncStrings::BoltofLinen},
        {GW::Constants::MaterialSlot::BoltofSilk, GW::EncStrings::BoltofSilk},
        {GW::Constants::MaterialSlot::DeldrimorSteelIngot, GW::EncStrings::DeldrimorSteelIngot},
        {GW::Constants::MaterialSlot::Diamond, GW::EncStrings::Diamond},
        {GW::Constants::MaterialSlot::ElonianLeatherSquare, GW::EncStrings::ElonianLeatherSquare},
        {GW::Constants::MaterialSlot::FurSquare, GW::EncStrings::FurSquare},
        {GW::Constants::MaterialSlot::GlobofEctoplasm, GW::EncStrings::GlobofEctoplasm},
        {GW::Constants::MaterialSlot::JadeiteShard, GW::EncStrings::JadeiteShard},
        {GW::Constants::MaterialSlot::LeatherSquare, GW::EncStrings::LeatherSquare},
        {GW::Constants::MaterialSlot::LumpofCharcoal, GW::EncStrings::LumpofCharcoal},
        {GW::Constants::MaterialSlot::MonstrousClaw, GW::EncStrings::MonstrousClaw},
        {GW::Constants::MaterialSlot::MonstrousEye, GW::EncStrings::MonstrousEye},
        {GW::Constants::MaterialSlot::MonstrousFang, GW::EncStrings::MonstrousFang},
        {GW::Constants::MaterialSlot::ObsidianShard, GW::EncStrings::ObsidianShard},
        {GW::Constants::MaterialSlot::OnyxGemstone, GW::EncStrings::OnyxGemstone},
        {GW::Constants::MaterialSlot::RollofParchment, GW::EncStrings::RollofParchment},
        {GW::Constants::MaterialSlot::RollofVellum, GW::EncStrings::RollofVellum},
        {GW::Constants::MaterialSlot::Ruby, GW::EncStrings::Ruby},
        {GW::Constants::MaterialSlot::Sapphire, GW::EncStrings::Sapphire},
        {GW::Constants::MaterialSlot::SpiritwoodPlank, GW::EncStrings::SpiritwoodPlank},
        {GW::Constants::MaterialSlot::SteelIngot, GW::EncStrings::SteelIngot},
        {GW::Constants::MaterialSlot::TemperedGlassVial, GW::EncStrings::TemperedGlassVial},
        {GW::Constants::MaterialSlot::VialofInk, GW::EncStrings::VialofInk},
    };
    std::unordered_map<GW::Constants::MaterialSlot, GuiUtils::EncString> material_names;

    const GW::Constants::MaterialSlot common_materials[] = {
        GW::Constants::MaterialSlot::Bone,       GW::Constants::MaterialSlot::IronIngot,      GW::Constants::MaterialSlot::TannedHideSquare,
        GW::Constants::MaterialSlot::Scale,      GW::Constants::MaterialSlot::ChitinFragment, GW::Constants::MaterialSlot::BoltofCloth,
        GW::Constants::MaterialSlot::WoodPlank,  GW::Constants::MaterialSlot::GraniteSlab,    GW::Constants::MaterialSlot::PileofGlitteringDust,
        GW::Constants::MaterialSlot::PlantFiber, GW::Constants::MaterialSlot::Feather,
    };
    const GW::Constants::MaterialSlot rare_materials[] = {
        GW::Constants::MaterialSlot::FurSquare,
        GW::Constants::MaterialSlot::BoltofLinen,
        GW::Constants::MaterialSlot::BoltofDamask,
        GW::Constants::MaterialSlot::BoltofSilk,
        GW::Constants::MaterialSlot::GlobofEctoplasm,
        GW::Constants::MaterialSlot::SteelIngot,
        GW::Constants::MaterialSlot::DeldrimorSteelIngot,
        GW::Constants::MaterialSlot::MonstrousClaw,
        GW::Constants::MaterialSlot::MonstrousEye,
        GW::Constants::MaterialSlot::MonstrousFang,
        GW::Constants::MaterialSlot::Ruby,
        GW::Constants::MaterialSlot::Sapphire,
        GW::Constants::MaterialSlot::Diamond,
        GW::Constants::MaterialSlot::OnyxGemstone,
        GW::Constants::MaterialSlot::LumpofCharcoal,
        GW::Constants::MaterialSlot::ObsidianShard,
        GW::Constants::MaterialSlot::TemperedGlassVial,
        GW::Constants::MaterialSlot::LeatherSquare,
        GW::Constants::MaterialSlot::ElonianLeatherSquare,
        GW::Constants::MaterialSlot::VialofInk,
        GW::Constants::MaterialSlot::RollofParchment,
        GW::Constants::MaterialSlot::RollofVellum,
        GW::Constants::MaterialSlot::SpiritwoodPlank,
        GW::Constants::MaterialSlot::AmberChunk,
        GW::Constants::MaterialSlot::JadeiteShard,
    };

    uint32_t CountItemByMaterialSlot(GW::Constants::MaterialSlot material_slot, GW::Constants::Bag bag_start = GW::Constants::Bag::Backpack, GW::Constants::Bag bag_end = GW::Constants::Bag::Bag_2) {
        uint32_t count = 0;
        const auto items = GW::Items::GetItemArray();
        for (const auto item : *items) {
            if (!(item && item->bag && item->bag->bag_id() >= bag_start && item->bag->bag_id() <= bag_end))
                continue;
            if (GW::Items::GetMaterialSlot(item) == material_slot)
                count += item->quantity;
        }
        return count;
    }

    struct Transaction {
        enum Type { Sell, Buy, Quote };
        enum State {
            Idle,
            Quoting,
            ManagingGold,
            Transacting
        };

        State state = State::Idle;
        uint32_t initial_item_count = 0;
        Type type = Type::Buy;
        uint32_t item_id = 0;
        uint32_t price = 0;
        GW::Constants::MaterialSlot material;

        Transaction(const Type t, const GW::Constants::MaterialSlot mat)
            : type(t)
            , material(mat) {
        }
    };
    std::deque<Transaction> transactions{};

    void Dequeue()
    {
        trans_done++;
        transactions.pop_front();
    }

    void Enqueue(Transaction::Type type, GW::Constants::MaterialSlot mat)
    {
        if (transactions.empty()) {
            trans_done = 0;
            trans_queued = 0;
        }
        transactions.emplace_back(type, mat);
        trans_queued++;
        cancelled = false;
    }

    void EnqueueQuote(const GW::Constants::MaterialSlot material)
    {
        Enqueue(Transaction::Quote, material);
    }

    void EnqueuePurchase(const GW::Constants::MaterialSlot material)
    {
        Enqueue(Transaction::Buy, material);
    }

    void EnqueueSell(const GW::Constants::MaterialSlot material)
    {
        Enqueue(Transaction::Sell, material);
    }

    Transaction* GetCurrentTransaction() {
        return transactions.size() ? &transactions[0] : nullptr;
    }

    GW::UI::Frame* GetVendorFrame() {
        return GW::UI::GetFrameByLabel(L"Vendor");
    }

    void Cancel(const std::string& error = "") {
        if(error.size())
            Log::Warning(error.c_str());
        while (transactions.size())
            Dequeue();
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kVendorQuote: {
            const auto packet = (GW::UI::UIPacket::kVendorQuote*)wParam;
            const auto current_transaction = GetCurrentTransaction();
            if (!current_transaction)
                return;
            if (current_transaction->item_id != packet->item_id) {
                Cancel("Received quote for unexpected item");
                return;
            }
            current_transaction->price = packet->price;

            const auto mat = GW::Items::GetMaterialSlot(GW::Items::GetItemById(packet->item_id));
            if ((uint32_t)mat < _countof(price)) {
                price[(uint32_t)mat] = packet->price;
            }
        } break;
        case GW::UI::UIMessage::kVendorTransComplete: {
            const auto current_transaction = GetCurrentTransaction();
            if (current_transaction && current_transaction->state == Transaction::State::Transacting)
                trans_complete = true;
        } break;
        }
    }

    GW::Item* GetVendorItem(const GW::Constants::MaterialSlot material, const GW::Merchant::TransactionType type)
    {

        std::vector<uint32_t> vendor_items;
        vendor_items.resize(GW::Merchant::GetMerchantItems(type));
        vendor_items.resize(GW::Merchant::GetMerchantItems(type, vendor_items.size(), vendor_items.data()));

        for (auto item_id : vendor_items) {
            const auto item = GW::Items::GetItemById(item_id);
            if (GW::Items::GetMaterialSlot(item) == material) return item;
        }
        return nullptr;
    }

    DWORD RequestPurchaseQuote(const GW::Constants::MaterialSlot material)
    {
        GW::Item* item = GetVendorItem(material, GW::Merchant::TransactionType::TraderBuy);
        if (!(item && RequestQuote(GW::Merchant::TransactionType::TraderBuy, item->item_id))) return 0;
        return item->item_id;
    }

    DWORD RequestSellQuote(const GW::Constants::MaterialSlot material)
    {
        GW::Item* item = GetVendorItem(material, GW::Merchant::TransactionType::TraderSell);
        if (!(item && RequestQuote(GW::Merchant::TransactionType::TraderSell, item->item_id))) return 0;
        return item->item_id;
    }

    bool TryTransaction(Transaction* trans) {
        if (!(trans && GetVendorFrame() && trans->item_id && trans->price && GW::Items::GetItemById(trans->item_id)))
            return false;
        GW::Merchant::TransactionInfo give, recv;
        switch (trans->type) {
        case Transaction::Buy:
        case Transaction::Sell: {
            return GW::Merchant::TransactItems();
        } break;
        }
        return false;
    }

    const int GetPrice(GW::Constants::MaterialSlot mat) {
        return (uint32_t)mat < _countof(price) ? price[(uint32_t)mat] : PRICE_DEFAULT;
    }

    const std::string GetPrice(const GW::Constants::MaterialSlot mat1, const float fac1,
        const GW::Constants::MaterialSlot mat2, const float fac2, const int extra)
    {
        const auto p1 = GetPrice(mat1);
        const auto p2 = GetPrice(mat2);
        if (p1 == PRICE_NOT_AVAILABLE || p2 == PRICE_NOT_AVAILABLE) {
            return "Price: (Material not available)";
        }
        if (p1 == PRICE_DEFAULT || p2 == PRICE_DEFAULT) {
            return "Price:  -";
        }
        if (p1 == PRICE_COMPUTING_SENT || p2 == PRICE_COMPUTING_SENT) {
            return "Price: Computing (request sent)";
        }
        if (p1 == PRICE_COMPUTING_QUEUE || p2 == PRICE_COMPUTING_QUEUE) {
            return "Price: Computing (in queue)";
        }
        return std::format("Price: {:.2f} k", (p1 * fac1 + p2 * fac2 + extra) / 1000.0f);
    }


    void FullConsPriceTooltip()
    {
        if (!ImGui::IsItemHovered())
            return;
        const auto feather = GetPrice(GW::Constants::MaterialSlot::Feather);
        const auto dust = GetPrice(GW::Constants::MaterialSlot::PileofGlitteringDust);
        const auto iron = GetPrice(GW::Constants::MaterialSlot::IronIngot);
        const auto bone = GetPrice(GW::Constants::MaterialSlot::Bone);
        if (feather == PRICE_NOT_AVAILABLE || iron == PRICE_NOT_AVAILABLE || dust == PRICE_NOT_AVAILABLE || bone == PRICE_NOT_AVAILABLE) {
            ImGui::SetTooltip("Full Conset Price: (Material not available)");
            return;
        }
        if (feather < 0 || iron < 0 || dust < 0 || bone < 0) {
            ImGui::SetTooltip("Full Conset Price: -");
            return;
        }

        const auto tooltip = std::format("Full Conset Price: {} k", (iron * 10 + dust * 10 + bone * 5 + feather * 5 + 750) / 1000.f);
        ImGui::SetTooltip(tooltip.c_str());
    }
}

void MaterialsWindow::Update(const float)
{
    const auto trans = GetCurrentTransaction();
    if (!trans)
        return;
    if (!GetVendorFrame()) {
        trans->state = Transaction::State::Idle;
        return;
    }
    switch (trans->state) {
    case Transaction::State::Idle: {
        const auto item_id = trans->type == Transaction::Sell ? RequestSellQuote(trans->material) : RequestPurchaseQuote(trans->material);
        if (!item_id) {
            Cancel("Failed to quote for item");
            return;
        }
        last_transaction = TIMER_INIT();
        trans->item_id = item_id;
        trans->state = Transaction::State::Quoting;
    } break;
    case Transaction::State::Quoting: {
        if (trans->price == 0) {
            if (TIMER_DIFF(last_transaction) > 3000)
                Cancel("Timeout waiting for quote");
            return;
        }
        const auto gold_on_character = GW::Items::GetGoldAmountOnCharacter();
        const auto gold_in_storage = GW::Items::GetGoldAmountInStorage();
        switch (trans->type) {
        case Transaction::Type::Quote: {
            Dequeue();
            return;
        } break;
        case Transaction::Type::Buy: {
            if (gold_on_character < trans->price) {
                if (!(manage_gold && gold_on_character + gold_in_storage >= trans->price && GW::Items::WithdrawGold())) {
                    Cancel("Not enough gold");
                    return; // Don't cancel, we're out of gold
                }
            }
        } break;
        case Transaction::Type::Sell: {
            if (gold_on_character + trans->price > 99999) {
                if (!(manage_gold && gold_in_storage < 999999 - trans->price && GW::Items::DepositGold())) {
                    Cancel("Too much gold");
                    return; // Don't cancel, we're out of gold
                }
            }
        } break;
        }
        last_transaction = TIMER_INIT();
        trans->state = Transaction::State::ManagingGold;
    } break;
    case Transaction::State::ManagingGold: {
        const auto gold_on_character = GW::Items::GetGoldAmountOnCharacter();
        switch (trans->type) {
        case Transaction::Type::Buy: {
            if (gold_on_character < trans->price) {
                if (TIMER_DIFF(last_transaction) > 3000)
                    Cancel("Timeout waiting for gold");
                return;
            }
        } break;
        case Transaction::Type::Sell: {
            if (gold_on_character + trans->price > 99999) {
                if (TIMER_DIFF(last_transaction) > 3000)
                    Cancel("Timeout waiting for gold");
                return;
            }
        } break;
        }
        trans->initial_item_count = CountItemByMaterialSlot(trans->material);
        if (!TryTransaction(trans)) {
            Cancel("Failed to transact item after quote");
            return;
        }
        trans->state = Transaction::State::Transacting;
    } break;
    case Transaction::State::Transacting: {
        if (trans_complete && CountItemByMaterialSlot(trans->material) != trans->initial_item_count) {
            trans_complete = false;
            Dequeue();
            return;
        }
        if (TIMER_DIFF(last_transaction) > 3000)
            Cancel("Timeout waiting for transaction");
        return;
    } break;
    }
}

bool MaterialsWindow::GetIsInProgress() const
{
    return !transactions.empty();
}

void MaterialsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    if (material_names.empty()) {
        material_names.reserve(material_enc_strings.size());
        for (const auto& [slot, enc] : material_enc_strings) {
            material_names.emplace(slot, enc); // EncString(const wchar_t*) — constructs in-place
        }
        for (auto& m : material_names) {
            m.second.string();
        }
    }

    // @Cleanup: these need to be GW::Constants::ModelFileID insted of hard coded here
    tex_essence = GwDatTextureModule::LoadTextureFromFileId(0x458A7);
    tex_grail = GwDatTextureModule::LoadTextureFromFileId(0x24BB);
    tex_armor = GwDatTextureModule::LoadTextureFromFileId(0x458A4);
    tex_powerstone = GwDatTextureModule::LoadTextureFromFileId(0x17000);
    tex_resscroll = GwDatTextureModule::LoadTextureFromFileId(0x38458);

    for (int& i : price) {
        i = PRICE_DEFAULT;
    }

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kVendorQuote,
        GW::UI::UIMessage::kVendorItems,
        GW::UI::UIMessage::kVendorWindow,
        GW::UI::UIMessage::kVendorTransComplete,
    };
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&PostUIMessage_Entry, message_id, OnPostUIMessage, 0x4000);
    }

}

void MaterialsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    Cancel();
    GW::UI::RemoveUIMessageCallback(&PostUIMessage_Entry);
}

void MaterialsWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(manage_gold);
    LOAD_BOOL(use_stock);
}

void MaterialsWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(manage_gold);
    SAVE_BOOL(use_stock);
}

void MaterialsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        constexpr auto stock_start = GW::Constants::Bag::Backpack;
        constexpr auto stock_end = GW::Constants::Bag::Storage_14;

        // note: textures are 64 x 64, but both off-center
        // and with a bunch of empty space. We want to center the image
        // while minimizing the rescaling

        // === Essence ===
        ImGui::Image(*tex_essence, ImVec2(50, 50),
                     ImVec2(4.0f / 64, 9.0f / 64), ImVec2(47.0f / 64, 52.0f / 64));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Essence of Celerity\n%s and %s", material_names[GW::Constants::MaterialSlot::Feather].string().c_str(), material_names[GW::Constants::MaterialSlot::PileofGlitteringDust].string().c_str());
        }
        ImGui::SameLine();
        float x = ImGui::GetCursorPosX();
        float y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GW::Constants::MaterialSlot::Feather, 5.0f, GW::Constants::MaterialSlot::PileofGlitteringDust, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##essence", ImVec2(100.0f, 0))) {
            EnqueueQuote(GW::Constants::MaterialSlot::Feather);
            EnqueueQuote(GW::Constants::MaterialSlot::PileofGlitteringDust);
        }
        float h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_essence = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###essenceqty", &qty_essence);
        if (qty_essence < 1) {
            qty_essence = 1;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##essence", ImVec2(100.0f, 0))) {
            const int feather_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::Feather, stock_start, stock_end);
            const int dust_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::PileofGlitteringDust, stock_start, stock_end);

            const int qty = 5 * qty_essence;
            for (int i = 0; i < qty; i++) {
                if (!use_stock || i < qty - feather_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::Feather);
                }
                if (!use_stock || i < qty - dust_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::PileofGlitteringDust);
                }
            }
        }

        /* @Cleanup: Using GW::Items::GetItemFormula and spoofing a GW::Item* with the formula id that matches the cons,
         we can programatically find out:
         a) what materials are needed and how many
         b) cost in gold on top of materials needed
         c) cost in skill points if applicable

         If we do this, we can rip this module in half by just looping a lambda for the below code.
         */

        ImGui::Separator();
        // === Grail ===
        ImGui::Image(*tex_grail, ImVec2(50, 50),
                     ImVec2(3.0f / 64, 11.0f / 64), ImVec2(49.0f / 64, 57.0f / 64));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grail of Might\n%s and %s", material_names[GW::Constants::MaterialSlot::IronIngot].string().c_str(), material_names[GW::Constants::MaterialSlot::PileofGlitteringDust].string().c_str());
        }
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GW::Constants::MaterialSlot::IronIngot, 5.0f, GW::Constants::MaterialSlot::PileofGlitteringDust, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##grail", ImVec2(100.0f, 0))) {
            EnqueueQuote(GW::Constants::MaterialSlot::IronIngot);
            EnqueueQuote(GW::Constants::MaterialSlot::PileofGlitteringDust);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_grail = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###grailqty", &qty_grail);
        if (qty_grail < 1) {
            qty_grail = 1;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##grail", ImVec2(100.0f, 0))) {
            const int iron_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::IronIngot, stock_start, stock_end);
            const int dust_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::PileofGlitteringDust, stock_start, stock_end);

            const int qty = 5 * qty_grail;
            for (int i = 0; i < qty; i++) {
                if (!use_stock || i < qty - iron_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::IronIngot);
                }
                if (!use_stock || i < qty - dust_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::PileofGlitteringDust);
                }
            }
        }

        ImGui::Separator();
        // === Armor ===
        ImGui::Image(*tex_armor, ImVec2(50, 50),
                     ImVec2(0, 1.0f / 64), ImVec2(59.0f / 64, 60.0f / 64));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Armor of Salvation\n%s and %s", material_names[GW::Constants::MaterialSlot::IronIngot].string().c_str(), material_names[GW::Constants::MaterialSlot::Bone].string().c_str());
        }
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GW::Constants::MaterialSlot::IronIngot, 5.0f, GW::Constants::MaterialSlot::Bone, 5.0f, 250).c_str());
        FullConsPriceTooltip();
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##armor", ImVec2(100.0f, 0))) {
            EnqueueQuote(GW::Constants::MaterialSlot::IronIngot);
            EnqueueQuote(GW::Constants::MaterialSlot::Bone);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_armor = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###armorqty", &qty_armor);
        if (qty_armor < 1) {
            qty_armor = 1;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##armor", ImVec2(100.0f, 0))) {
            const int iron_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::IronIngot, stock_start, stock_end);
            const int bone_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::Bone, stock_start, stock_end);

            const int qty = 5 * qty_armor;
            for (int i = 0; i < qty; i++) {
                if (!use_stock || i < qty - iron_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::IronIngot);
                }
                if (!use_stock || i < qty - bone_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::Bone);
                }
            }
        }

        ImGui::Separator();
        // === Powerstone ===
        ImGui::Image(*tex_powerstone, ImVec2(50, 50),
                     ImVec2(0, 6.0f / 64), ImVec2(54.0f / 64, 60.0f / 64));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Powerstone of Courage\n%s and %s", material_names[GW::Constants::MaterialSlot::GraniteSlab].string().c_str(), material_names[GW::Constants::MaterialSlot::PileofGlitteringDust].string().c_str());
        }
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GW::Constants::MaterialSlot::GraniteSlab, 10.0f, GW::Constants::MaterialSlot::PileofGlitteringDust, 10.0f, 1000).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##pstone", ImVec2(100.0f, 0))) {
            EnqueueQuote(GW::Constants::MaterialSlot::GraniteSlab);
            EnqueueQuote(GW::Constants::MaterialSlot::PileofGlitteringDust);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_pstone = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###pstoneqty", &qty_pstone);
        if (qty_pstone < 1) {
            qty_pstone = 1;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##pstone", ImVec2(100.0f, 0))) {
            const int granite_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::GraniteSlab, stock_start, stock_end);
            const int dust_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::PileofGlitteringDust, stock_start, stock_end);

            const int qty = 10 * qty_pstone;
            for (int i = 0; i < qty; i++) {
                if (!use_stock || i < qty - granite_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::GraniteSlab);
                }
                if (!use_stock || i < qty - dust_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::PileofGlitteringDust);
                }
            }
        }

        ImGui::Separator();
        // === Res scroll ===
        ImGui::Image(*tex_resscroll, ImVec2(50, 50),
                     ImVec2(1.0f / 64, 4.0f / 64), ImVec2(56.0f / 64, 59.0f / 64));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Scroll of Resurrection\n%s and %s", material_names[GW::Constants::MaterialSlot::PlantFiber].string().c_str(), material_names[GW::Constants::MaterialSlot::Bone].string().c_str());
        }
        ImGui::SameLine();
        x = ImGui::GetCursorPosX();
        y = ImGui::GetCursorPosY();
        ImGui::Text(GetPrice(GW::Constants::MaterialSlot::PlantFiber, 2.5f, GW::Constants::MaterialSlot::Bone, 2.5f, 250).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Price Check##resscroll", ImVec2(100.0f, 0))) {
            EnqueueQuote(GW::Constants::MaterialSlot::PlantFiber);
            EnqueueQuote(GW::Constants::MaterialSlot::Bone);
        }
        h = ImGui::GetCurrentContext()->LastItemData.Rect.GetHeight();
        static int qty_resscroll = 1;
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(y + h + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushItemWidth(-100.0f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputInt("###resscrollqty", &qty_resscroll);
        if (qty_resscroll < 1) {
            qty_resscroll = 1;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Buy##resscroll", ImVec2(100.0f, 0))) {
            const int fiber_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::PlantFiber, stock_start, stock_end);
            const int bone_stock = CountItemByMaterialSlot(GW::Constants::MaterialSlot::Bone, stock_start, stock_end);

            const int qty = static_cast<int>(std::ceil(2.5f * static_cast<float>(qty_resscroll)));
            for (int i = 0; i < qty; i++) {
                if (!use_stock || i < qty - fiber_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::PlantFiber);
                }
                if (!use_stock || i < qty - bone_stock / 10) {
                    EnqueuePurchase(GW::Constants::MaterialSlot::Bone);
                }
            }
        }

        ImGui::Separator();

        constexpr float width2 = 100.0f;
        const float width1 = ImGui::GetContentRegionAvail().x - width2 - 100.0f - ImGui::GetStyle().ItemSpacing.x * 2;

        // === Common materials ===
        static int common_idx = 0;
        static int common_qty = 1;

        ImGui::PushItemWidth(width1);
        ImGui::Combo("##commoncombo", &common_idx,
            [](void*, int n) {
                return material_names[common_materials[n]].string().c_str();
            },
            nullptr, _countof(common_materials)
        );
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(width2);
        ImGui::InputInt("##commonqty", &common_qty);
        ImGui::PopItemWidth();
        if (common_qty < 1) {
            common_qty = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Buy##common", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            const auto mat = common_materials[common_idx];
            const int material_stock = CountItemByMaterialSlot(mat, stock_start, stock_end);
            const int to_buy = common_qty - (use_stock ? material_stock / 10 : 0);
            for (int i = 0; i < to_buy; i++) {
                EnqueuePurchase(mat);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Sell##common", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            const auto mat = common_materials[common_idx];
            for (int i = 0; i < common_qty; i++) {
                EnqueueSell(mat);
            }
        }

        // === Rare materials ===
        static int rare_idx = 0;
        static int rare_qty = 1;

        ImGui::PushItemWidth(width1);
        ImGui::Combo("##rarecombo", &rare_idx,
            [](void*, int n) {
                return material_names[rare_materials[n]].string().c_str();
            }, nullptr, _countof(rare_materials));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(width2);
        ImGui::InputInt("##rareqty", &rare_qty);
        ImGui::PopItemWidth();
        if (rare_qty < 1) {
            rare_qty = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Buy##rare", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            const auto mat = rare_materials[rare_idx];
            const int material_stock = CountItemByMaterialSlot(mat, stock_start, stock_end);
            const int to_buy = rare_qty - (use_stock ? material_stock : 0);
            for (int i = 0; i < to_buy; i++) {
                EnqueuePurchase(mat);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Sell##rare", ImVec2(50.0f - ImGui::GetStyle().ItemSpacing.x / 2, 0))) {
            const auto mat = rare_materials[rare_idx];
            for (int i = 0; i < rare_qty; i++) {
                EnqueueSell(mat);
            }
        }

        ImGui::Separator();
        float progress = 0.0f;
        if (trans_queued > 0) {
            progress = static_cast<float>(trans_done) / static_cast<float>(trans_queued);
        }
        auto status = "";
        if (cancelled) {
            status = "Cancelled";
        }
        else if (trans_done < trans_queued) {
            status = "Working";
        }
        else {
            status = "Ready";
        }
        ImGui::Text("%s [%d / %d]", status, trans_done, trans_queued);
        ImGui::SameLine(width1 + ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ItemSpacing.x);
        ImGui::ProgressBar(progress, ImVec2(width2, 0));
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0))) {
            Cancel();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cancel the current queue of operations");
        }
        DrawSettingsInternal();
    }
    ImGui::End();
}

void MaterialsWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::CheckboxWithHelp("Automatically manage gold", &manage_gold, "It will automatically withdraw and deposit gold while buying materials");
    ImGui::CheckboxWithHelp("Use stock", &use_stock, "Will take materials in inventory and storage into account when buying materials");
}
