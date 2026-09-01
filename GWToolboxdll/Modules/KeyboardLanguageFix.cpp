#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include "KeyboardLanguageFix.h"

void KeyboardLanguageFix::Initialize()
{
    ToolboxModule::Initialize();

    const auto en_us_keyboard_name = "00000409";

    HKL* address = *(HKL**)GW::Scanner::Find("\x81\xe6\xff\xff\xff\x7f\x85\xc0", "xxxxxxxx", -0x4);
#ifdef _DEBUG
    ASSERT(address && "找不到键盘布局地址");
#endif
    if (!address) {
        Log::Error("找不到键盘布局地址");
        return;
    }
    Log::Log("键盘布局地址 %p, %p", address, *address);
    if (*address) {
        Log::Log("无法拦截键盘布局覆盖；Guild Wars 已设置键盘语言");
        return;
    }
    // 通过 GetKeyboardLayoutList 尝试检测 en-US 是否已在操作系统中安装
    HKL loaded_languages[0xff];
    const auto keyboard_languages_count = GetKeyboardLayoutList(_countof(loaded_languages), loaded_languages);
    if (!keyboard_languages_count) {
        Log::Error("GetKeyboardLayoutList 失败，GetLastError = %#08x", GetLastError());
        return;
    }
    const auto current_keyboard_layout = GetKeyboardLayout(0);
    char layout_name[KL_NAMELENGTH];
    for (auto i = 0; i < keyboard_languages_count; i++) {
        if (!ActivateKeyboardLayout(loaded_languages[i], 0)) {
            Log::Error("ActivateKeyboardLayout 失败，GetLastError = %#08x", GetLastError());
            return;
        }
        if (!GetKeyboardLayoutNameA(layout_name)) {
            Log::Error("GetKeyboardLayoutNameA 失败，GetLastError = %#08x", GetLastError());
            return;
        }
        if (strcmp(layout_name, en_us_keyboard_name) == 0) {
            // 找到 en-US，但不要调用 LoadKeyboardLayoutA，以防 Windows 设置了替代语言。
            // *address = LoadKeyboardLayoutA(en_us_keyboard_name, 0);
            return;
        }
    }
    // 到了这里，说明 en-US 尚未安装，但我们不希望 Windows 将其添加为新语言，
    // 因此保持使用当前语言。
    ActivateKeyboardLayout(current_keyboard_layout, 0);
    *address = current_keyboard_layout;
}
