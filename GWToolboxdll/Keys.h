#pragma once

#define VK_0        0x30
#define VK_1        0x31
#define VK_2        0x32
#define VK_3        0x33
#define VK_4        0x34
#define VK_5        0x35
#define VK_6        0x36
#define VK_7        0x37
#define VK_8        0x38
#define VK_9        0x39
#define VK_A        0x41
#define VK_B        0x42
#define VK_C        0x43
#define VK_D        0x44
#define VK_E        0x45
#define VK_F        0x46
#define VK_G        0x47
#define VK_H        0x48
#define VK_I        0x49
#define VK_J        0x4A
#define VK_K        0x4B
#define VK_L        0x4C
#define VK_M        0x4D
#define VK_N        0x4E
#define VK_O        0x4F
#define VK_P        0x50
#define VK_Q        0x51
#define VK_R        0x52
#define VK_S        0x53
#define VK_T        0x54
#define VK_U        0x55
#define VK_V        0x56
#define VK_W        0x57
#define VK_X        0x58
#define VK_Y        0x59
#define VK_Z        0x5A
#include "imgui_impl_win32.h"

static int KeyName(long vkey, char* buf, int len) {
    UINT scan_code = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(MAKELPARAM(0x0001, scan_code), buf, len);
}

static const char* KeyName(long vkey) {
    switch (vkey) {
        case ImGuiMouseButton_Left: return "Left mouse";
        case ImGuiMouseButton_Right: return "Right mouse";
        case ImGuiMouseButton_Middle: return "Middle mouse";
        case ImGuiMouseButton_X1: return "X1 mouse";
        case ImGuiMouseButton_X2: return "X2 mouse";
        case ImGuiKey_Backspace: return "Backspace";
        case ImGuiKey_Tab: return "Tab";
        case ImGuiKey_Enter: return "Enter";
        case ImGuiKey_KeypadEnter: return "Num Enter";
        case ImGuiKey_ModShift: return "Shift";
        case ImGuiKey_ModCtrl: return "Control";
        case ImGuiKey_ModAlt: return "Alt";
        case ImGuiKey_ModSuper: return "Win";
        case ImGuiKey_Pause: return "Pause";
        case ImGuiKey_CapsLock: return "Caps Lock";
        case ImGuiKey_Escape: return "Esc";
        case ImGuiKey_Space: return "Space";
        case ImGuiKey_PageUp: return "Page Up";
        case ImGuiKey_PageDown: return "Page Down";
        case ImGuiKey_End: return "End";
        case ImGuiKey_Home: return "Home";
        case ImGuiKey_LeftArrow: return "Left";
        case ImGuiKey_UpArrow: return "Up";
        case ImGuiKey_RightArrow: return "Right";
        case ImGuiKey_DownArrow: return "Down";
        case ImGuiKey_PrintScreen: return "Print";
        case ImGuiKey_Insert: return "Ins";
        case ImGuiKey_Delete: return "Del";
        case ImGuiKey_Menu: return "Apps/Menu";
        case ImGuiKey_0: return "0";
        case ImGuiKey_1: return "1";
        case ImGuiKey_2: return "2";
        case ImGuiKey_3: return "3";
        case ImGuiKey_4: return "4";
        case ImGuiKey_5: return "5";
        case ImGuiKey_6: return "6";
        case ImGuiKey_7: return "7";
        case ImGuiKey_8: return "8";
        case ImGuiKey_9: return "9";
        case ImGuiKey_A: return "A";
        case ImGuiKey_B: return "B";
        case ImGuiKey_C: return "C";
        case ImGuiKey_D: return "D";
        case ImGuiKey_E: return "E";
        case ImGuiKey_F: return "F";
        case ImGuiKey_G: return "G";
        case ImGuiKey_H: return "H";
        case ImGuiKey_I: return "I";
        case ImGuiKey_J: return "J";
        case ImGuiKey_K: return "K";
        case ImGuiKey_L: return "L";
        case ImGuiKey_M: return "M";
        case ImGuiKey_N: return "N";
        case ImGuiKey_O: return "O";
        case ImGuiKey_P: return "P";
        case ImGuiKey_Q: return "Q";
        case ImGuiKey_R: return "R";
        case ImGuiKey_S: return "S";
        case ImGuiKey_T: return "T";
        case ImGuiKey_U: return "U";
        case ImGuiKey_V: return "V";
        case ImGuiKey_W: return "W";
        case ImGuiKey_X: return "X";
        case ImGuiKey_Y: return "Y";
        case ImGuiKey_Z: return "Z";
        case ImGuiKey_Keypad0: return "Numpad 0";
        case ImGuiKey_Keypad1: return "Numpad 1";
        case ImGuiKey_Keypad2: return "Numpad 2";
        case ImGuiKey_Keypad3: return "Numpad 3";
        case ImGuiKey_Keypad4: return "Numpad 4";
        case ImGuiKey_Keypad5: return "Numpad 5";
        case ImGuiKey_Keypad6: return "Numpad 6";
        case ImGuiKey_Keypad7: return "Numpad 7";
        case ImGuiKey_Keypad8: return "Numpad 8";
        case ImGuiKey_Keypad9: return "Numpad 9";
        case ImGuiKey_F1: return "F1";
        case ImGuiKey_F2: return "F2";
        case ImGuiKey_F3: return "F3";
        case ImGuiKey_F4: return "F4";
        case ImGuiKey_F5: return "F5";
        case ImGuiKey_F6: return "F6";
        case ImGuiKey_F7: return "F7";
        case ImGuiKey_F8: return "F8";
        case ImGuiKey_F9: return "F9";
        case ImGuiKey_F10: return "F10";
        case ImGuiKey_F11: return "F11";
        case ImGuiKey_F12: return "F12";
        case ImGuiKey_KeypadMultiply: return "Multiply";
        case ImGuiKey_KeypadAdd: return "Add";
        case ImGuiKey_KeypadSubtract: return "Subtract";
        case ImGuiKey_KeypadDecimal: return "Decimal";
        case ImGuiKey_KeypadDivide: return "Divide";
        case ImGuiKey_NumLock: return "Num Lock";
        case ImGuiKey_ScrollLock: return "Scroll Lock";
        case ImGuiKey_LeftShift: return "Left Shift";
        case ImGuiKey_RightShift: return "Right Shift";
        case ImGuiKey_LeftCtrl: return "Left Control";
        case ImGuiKey_RightCtrl: return "Right Control";
        case ImGuiKey_LeftAlt: return "Left Alt";
        case ImGuiKey_RightAlt: return "Right Alt";
        case ImGuiKey_LeftSuper: return "Left Win";
        case ImGuiKey_RightSuper: return "Right Win";
        default: return "Unknown";
    }
}

typedef long Key;

constexpr LONG ModKey_Shift = 0x10000;
constexpr LONG ModKey_Control = 0x20000;
constexpr LONG ModKey_Alt = 0x40000;

inline int ModKeyName(char* buf, size_t bufsz, LONG mod, LONG vkey, const char* ifempty = "") {
    char vkey_c[32];
    if (vkey > 0) {
        if (!KeyName(vkey, vkey_c, _countof(vkey_c)))
            snprintf(vkey_c, _countof(vkey_c), "%s", KeyName(vkey));
    }
    return snprintf(buf, bufsz, "%s%s%s%s",
        (mod & ModKey_Control) ? "Control + " : "",
        (mod & ModKey_Alt) ? "Alt + " : "",
        (mod & ModKey_Shift) ? "Shift + " : "",
        vkey > 0 ? vkey_c : ifempty);
}
