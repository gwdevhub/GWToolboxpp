#pragma once

#define GWCA_VERSION_MAJOR 2
#define GWCA_VERSION_MINOR 5
#define GWCA_VERSION_PATCH 4
#define GWCA_VERSION_BUILD 0
#define GWCA_VERSION "2.5.4.0"

namespace GWCA {
    constexpr int VersionMajor = GWCA_VERSION_MAJOR;
    constexpr int VersionMinor = GWCA_VERSION_MINOR;
    constexpr int VersionPatch = GWCA_VERSION_PATCH;
    constexpr int VersionBuild = GWCA_VERSION_BUILD;
    constexpr const char* Version = GWCA_VERSION;
}
