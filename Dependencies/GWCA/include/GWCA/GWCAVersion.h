#pragma once

#define GWCA_VERSION_MAJOR 1
#define GWCA_VERSION_MINOR 0
#define GWCA_VERSION_PATCH 13
#define GWCA_VERSION_BUILD 0
#define GWCA_VERSION "1.0.13.0"

namespace GWCA {
    constexpr int VersionMajor = GWCA_VERSION_MAJOR;
    constexpr int VersionMinor = GWCA_VERSION_MINOR;
    constexpr int VersionPatch = GWCA_VERSION_PATCH;
    constexpr int VersionBuild = GWCA_VERSION_BUILD;
    constexpr const char* Version = GWCA_VERSION;
}
