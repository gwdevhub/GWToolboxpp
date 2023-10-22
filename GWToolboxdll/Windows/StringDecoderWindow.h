#pragma once

#include <ToolboxWindow.h>

class StringDecoderWindow : public ToolboxWindow {
    StringDecoderWindow() = default;

    ~StringDecoderWindow() override
    {
        delete[] encoded;
        encoded = nullptr;
    }

public:
    static StringDecoderWindow& Instance()
    {
        static StringDecoderWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "String Decoder"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;

    void Decode();
    void DecodeFromMapId();
    void Send() const;
    std::wstring GetEncodedString() const;

    static void PrintEncStr(const wchar_t* enc_str);

private:
    int encoded_id = 0;
    char* encoded = nullptr;
    const size_t encoded_size = 8192;
    std::wstring decoded;
    int map_id = 0;
};
