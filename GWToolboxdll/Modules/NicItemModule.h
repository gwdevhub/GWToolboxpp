#pragma once
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/UIMgr.h>
#include <ToolboxModule.h>

class NicItemModule final : public ToolboxModule {
public:
    struct NicItem {
        bool decoding_en_name;
        std::wstring name;
        const wchar_t* enc_name;
        static void OnNameDecoded(void* param, wchar_t* s)
        {
            auto ctx = (NicItem*)param;
            ctx->name = s;
            ctx->decoding_en_name = false;
        }
        NicItem(const wchar_t* name)
        {
            enc_name = name;
            decoding_en_name = true;
            GW::UI::AsyncDecodeStr(name, OnNameDecoded, this, GW::Constants::Language::English);
        }
    };

    static NicItemModule& Instance() {
        static NicItemModule instance;

        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Nic Items Module"; }
    [[nodiscard]] const char* Description() const override { return "Extends Guild Wars with functionality for nic items"; }

    bool IsNicItem(uint32_t item_id);
    const NicItem* GetNicItem(uint32_t item_id);
    void Initialize() override;
    void Terminate() override;
};
