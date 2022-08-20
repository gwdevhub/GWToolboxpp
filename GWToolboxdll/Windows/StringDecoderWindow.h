#pragma once

#include <ToolboxWindow.h>

class StringDecoderWindow : public ToolboxWindow {
    StringDecoderWindow() {};
    ~StringDecoderWindow();
public:
    static StringDecoderWindow& Instance() {
        static StringDecoderWindow instance;
        return instance;
    }

    const char* Name() const override { return "String Decoder"; }
    const char8_t* Icon() const override { return ICON_FA_LOCK_OPEN; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;

    void Decode();
    void DecodeFromMapId();
    void Send();
    static void PrintEncStr(const wchar_t* enc_str);
    std::wstring GetEncodedString();

private:
    int encoded_id = 0;
    char* encoded = 0;
    const size_t encoded_size = 8192;
    std::wstring decoded;
    int map_id = 0;
};
