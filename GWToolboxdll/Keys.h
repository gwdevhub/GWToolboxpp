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

static int KeyName(long vkey, char* buf, int len) {
    UINT scan_code = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(MAKELPARAM(0x0001, scan_code), buf, len);
}

static const char* KeyName(long vkey) {
    switch (vkey) {
        case 0: return "None";
        case VK_LBUTTON: return "Left mouse";
        case VK_RBUTTON: return "Right mouse";
        case VK_CANCEL: return "Control-break";
        case VK_MBUTTON: return "Middle mouse";
        case VK_XBUTTON1: return "X1 mouse";
        case VK_XBUTTON2: return "X2 mouse";
        case 0x07: return "VK_0x07";
        case VK_BACK: return "Backspace";
        case VK_TAB: return "Tab";
        case 0x0A: return "VK_0x0A";
        case 0x0B: return "VK_0x0B";
        case VK_CLEAR: return "Clear";
        case VK_RETURN: return "Enter";
        case 0x0E: return "VK_0x0E";
        case 0x0F: return "VK_0x0F";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Control";
        case VK_MENU: return "Alt";
        case VK_PAUSE: return "Pause";
        case VK_CAPITAL: return "Caps Lock";
        case VK_KANA: return "IME";
        case 0x16: return "VK_0x16";
        case VK_JUNJA: return "IME Junja";
        case VK_FINAL: return "IME Final";
        case VK_HANJA: return "IME Hanja/Kanji";
        case 0x1A: return "VK_0x1A";
        case VK_ESCAPE: return "Esc";
        case VK_CONVERT: return "IME Convert";
        case VK_NONCONVERT: return "IME Nonconvert";
        case VK_ACCEPT: return "IME Accept";
        case VK_MODECHANGE: return "IME Mode change";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left";
        case VK_UP: return "Up";
        case VK_RIGHT: return "Right";
        case VK_DOWN: return "Down";
        case VK_SELECT: return "Select";
        case VK_PRINT: return "Print";
        case VK_EXECUTE: return "Execute";
        case VK_SNAPSHOT: return "PrintScreen";
        case VK_INSERT: return "Ins";
        case VK_DELETE: return "Del";
        case VK_HELP: return "Help";
        case 0x30: return "0";
        case 0x31: return "1";
        case 0x32: return "2";
        case 0x33: return "3";
        case 0x34: return "4";
        case 0x35: return "5";
        case 0x36: return "6";
        case 0x37: return "7";
        case 0x38: return "8";
        case 0x39: return "9";
        case 0x3A: return "VK_0x3A";
        case 0x3B: return "VK_0x3B";
        case 0x3C: return "VK_0x3C";
        case 0x3D: return "VK_0x3D";
        case 0x3E: return "VK_0x3E";
        case 0x3F: return "VK_0x3F";
        case 0x40: return "VK_0x40";
        case 0x41: return "A";
        case 0x42: return "B";
        case 0x43: return "C";
        case 0x44: return "D";
        case 0x45: return "E";
        case 0x46: return "F";
        case 0x47: return "G";
        case 0x48: return "H";
        case 0x49: return "I";
        case 0x4A: return "J";
        case 0x4B: return "K";
        case 0x4C: return "L";
        case 0x4D: return "M";
        case 0x4E: return "N";
        case 0x4F: return "O";
        case 0x50: return "P";
        case 0x51: return "Q";
        case 0x52: return "R";
        case 0x53: return "S";
        case 0x54: return "T";
        case 0x55: return "U";
        case 0x56: return "V";
        case 0x57: return "W";
        case 0x58: return "X";
        case 0x59: return "Y";
        case 0x5A: return "Z";
        case VK_LWIN: return "Left Windows";
        case VK_RWIN: return "Right Windows";
        case VK_APPS: return "Applications";
        case 0x5E: return "VK_0x5E";
        case VK_SLEEP: return "Sleep";
        case VK_NUMPAD0: return "Numpad 0";
        case VK_NUMPAD1: return "Numpad 1";
        case VK_NUMPAD2: return "Numpad 2";
        case VK_NUMPAD3: return "Numpad 3";
        case VK_NUMPAD4: return "Numpad 4";
        case VK_NUMPAD5: return "Numpad 5";
        case VK_NUMPAD6: return "Numpad 6";
        case VK_NUMPAD7: return "Numpad 7";
        case VK_NUMPAD8: return "Numpad 8";
        case VK_NUMPAD9: return "Numpad 9";
        case VK_MULTIPLY: return "Multiply";
        case VK_ADD: return "Add";
        case VK_SEPARATOR: return "Separator";
        case VK_SUBTRACT: return "Subtract";
        case VK_DECIMAL: return "Decimal";
        case VK_DIVIDE: return "Divide";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_F13: return "F13";
        case VK_F14: return "F14";
        case VK_F15: return "F15";
        case VK_F16: return "F16";
        case VK_F17: return "F17";
        case VK_F18: return "F18";
        case VK_F19: return "F19";
        case VK_F20: return "F20";
        case VK_F21: return "F21";
        case VK_F22: return "F22";
        case VK_F23: return "F23";
        case VK_F24: return "F24";
        case 0x88: return "VK_0x88";
        case 0x89: return "VK_0x89";
        case 0x8A: return "VK_0x8A";
        case 0x8B: return "VK_0x8B";
        case 0x8C: return "VK_0x8C";
        case 0x8D: return "VK_0x8D";
        case 0x8E: return "VK_0x8E";
        case 0x8F: return "VK_0x8F";
        case VK_NUMLOCK: return "Num Lock";
        case VK_SCROLL: return "Scroll Lock";
        case 0x92: return "VK_0x92";
        case 0x93: return "VK_0x93";
        case 0x94: return "VK_0x94";
        case 0x95: return "VK_0x95";
        case 0x96: return "VK_0x96";
        case 0x97: return "VK_0x97";
        case 0x98: return "VK_0x98";
        case 0x99: return "VK_0x99";
        case 0x9A: return "VK_0x9A";
        case 0x9B: return "VK_0x9B";
        case 0x9C: return "VK_0x9C";
        case 0x9D: return "VK_0x9D";
        case 0x9E: return "VK_0x9E";
        case 0x9F: return "VK_0x9F";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Control";
        case VK_RCONTROL: return "Right Control";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_BROWSER_BACK: return "Browser Back";
        case VK_BROWSER_FORWARD: return "Browser Forward";
        case VK_BROWSER_REFRESH: return "Browser Refresh";
        case VK_BROWSER_STOP: return "Browser Stop";
        case VK_BROWSER_SEARCH: return "Browser Search";
        case VK_BROWSER_FAVORITES: return "Browser Favorites";
        case VK_BROWSER_HOME: return "Browser Home";
        case VK_VOLUME_MUTE: return "Volume Mute";
        case VK_VOLUME_DOWN: return "Volume Down";
        case VK_VOLUME_UP: return "Volume Up";
        case VK_MEDIA_NEXT_TRACK: return "Next Track";
        case VK_MEDIA_PREV_TRACK: return "Prev Track";
        case VK_MEDIA_STOP: return "Media Stop";
        case VK_MEDIA_PLAY_PAUSE: return "Play/Pause";
        case VK_LAUNCH_MAIL: return "Start Mail";
        case VK_LAUNCH_MEDIA_SELECT: return "Media Select";
        case VK_LAUNCH_APP1: return "Launch Application 1";
        case VK_LAUNCH_APP2: return "Launch Application 2";
        case 0xB8: return "VK_0xB8";
        case 0xB9: return "VK_0xB9";
        case VK_OEM_1: return "Oem 1";
        case VK_OEM_PLUS: return "+";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_MINUS: return "-";
        case VK_OEM_PERIOD: return ".";
        case VK_OEM_2: return "Oem 2";
        case VK_OEM_3:
            return "Oem 3";
            // 0xC1-D7 -> reserved
            // 0xD8-DA -> unassigned
        case VK_OEM_4: return "Oem 4";
        case VK_OEM_5: return "Oem 5";
        case VK_OEM_6: return "Oem 6";
        case VK_OEM_7: return "Oem 7";
        case VK_OEM_8: return "Oem 8";
        case 0xE0: return "VK_0xE0"; // reserved
        case 0xE1: return "VK_0xE1"; // OEM specific
        case VK_OEM_102: return "Oem 10";
        case 0xE3: return "VK_0xE3"; // oem specific
        case 0xE4: return "VK_0xE4"; // oem specific
        case VK_PROCESSKEY: return "IME Process";
        case 0xE6: return "VK_0xE6"; // oem specific
        case VK_PACKET:
            return "Packet";
            // 0xE8 -> unassigned
        case 0xE9: return "VK_0xE9";
        case 0xEA: return "VK_0xEA";
        case 0xEB: return "VK_0xEB";
        case 0xEC: return "VK_0xEC";
        case 0xED: return "VK_0xED";
        case 0xEE: return "VK_0xEE";
        case 0xEF: return "VK_0xEF";
        case 0xF0: return "VK_0xF0";
        case 0xF1: return "VK_0xF1";
        case 0xF2: return "VK_0xF2";
        case 0xF3: return "VK_0xF3";
        case 0xF4: return "VK_0xF4";
        case 0xF5: return "VK_0xF5";
        case VK_ATTN: return "Attn";
        case VK_CRSEL: return "CrSel";
        case VK_EXSEL: return "ExSel";
        case VK_EREOF: return "Erase EOF";
        case VK_PLAY: return "Play";
        case VK_ZOOM: return "Zoom";
        case VK_NONAME: return "Noname";
        case VK_PA1: return "PA1";
        case VK_OEM_CLEAR: return "Clear";
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
