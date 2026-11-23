#include "EmbeddedIcon.h"

#include <cstddef>

__asm__(
    ".data\n"
    ".balign 16\n"
    ".globl gwtb_app_icon_begin\n"
    "gwtb_app_icon_begin:\n"
    ".incbin \"" GWTB_APP_ICON_PATH "\"\n"
    ".globl gwtb_app_icon_end\n"
    "gwtb_app_icon_end:\n"
    ".text\n");

extern "C" const unsigned char gwtb_app_icon_begin[];
extern "C" const unsigned char gwtb_app_icon_end[];

std::string embeddedAppIcon()
{
    return std::string(reinterpret_cast<const char*>(gwtb_app_icon_begin),
                       static_cast<std::size_t>(gwtb_app_icon_end - gwtb_app_icon_begin));
}
