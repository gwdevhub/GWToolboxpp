#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include "KeyboardLanguageFix.h"

/*
* 在游戏有机会分配键盘布局之前，尝试分配 GW 键盘布局地址。
* 一旦 en-us 布局被加载，恢复原始键盘布局。
*
* 问题说明：
* 在 Windows 8 之后的 Windows 版本中，LoadKeyboardLayoutA 会在使用前安装键盘语言。
*
* 这对用户来说是个问题，因为当您有超过 1 种语言时，按 Win + SPACE 会使您当前的操作失去焦点，
* 并显示一个烦人的语言选择器。
* 玩家通常喜欢使用这个快捷键，但现在无法再使用了，因为 GW 安装了一种额外的语言，
* 强制切换器执行其操作。
*
* 一旦 en-US 键盘布局被 Windows 偷偷安装，唯一移除它的方法是进入设置，
* 安装完整的 en-US 语言包，然后全部卸载！
*
* 参见：https://superuser.com/questions/1680608/how-to-get-rid-of-us-language-in-windows-11
*
* 解决方案：
* 如果未找到语言，我们实际上不希望 GW 安装它 - 如果已有则加载它，
* 否则继续使用操作系统当前使用的语言。
*
* 副作用：
* 聊天选项卡上的标签会出错 - GW 仍然认为我们正在使用 en-US，
* 因此在 en-GB 键盘布局上按 SHIFT + " 实际上会打开公会聊天（SHIFT + @）
*
*/

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
