# Headless Wine + real MSVC (x86) toolchain for cross-compiling GWToolboxdll on Linux.
#
# GWToolboxdll is injected into the (32-bit) Guild Wars process, so it must be built
# x86/Win32 with an actual MSVC v143+ toolset (CMakeLists.txt enforces both). Since MSVC
# isn't redistributable, we can't ship prebuilt binaries here - instead we use
# mstorsjo/msvc-wine, which downloads the real MSVC + Windows SDK components straight
# from Microsoft's official installer manifests (same ones Visual Studio's installer
# uses) under the VS Build Tools EULA, then wraps cl/link/lib/rc as thin scripts that
# run the real Windows binaries under Wine. From CMake's point of view this is
# indistinguishable from a native MSVC install.
FROM ubuntu:24.04

ARG MSVC_WINE_REF=514f8ea34842cd6d831804d0e9658d3a32870ae1

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gpg \
        wget \
        python3 \
        msitools \
        git \
        unzip \
        zip \
        tar \
        build-essential \
        ninja-build \
        pkg-config \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 24.04's bundled wine (9.0) has broken mspdbsrv named-pipe IPC: any cl.exe invocation
# with /FS (which CMake adds unconditionally for MSVC >= 18.0, regardless of generator or
# debug-info settings) fails with "fatal error C1902: Program database manager mismatch".
# WineHQ's own build fixes this - use it instead of the distro package.
RUN mkdir -pm755 /etc/apt/keyrings \
    && wget -O /etc/apt/keyrings/winehq-archive.key https://dl.winehq.org/wine-builds/winehq.key \
    && wget -NP /etc/apt/sources.list.d/ https://dl.winehq.org/wine-builds/ubuntu/dists/noble/winehq-noble.sources \
    && dpkg --add-architecture i386 \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --install-recommends winehq-staging \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# The repo's CMakeLists.txt requires >= 3.29 (Ubuntu 24.04's apt cmake is 3.28); grab the
# same Kitware release the maintainers build with (see CMakeLists.txt's CMake 4.3 comment).
ARG CMAKE_VERSION=4.3.3
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
        | tar -xz -C /opt \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest" /usr/local/bin/ctest \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cpack" /usr/local/bin/cpack

# Initialize the wine prefix once at build time and bake it into the image layer -
# avoids re-running wineboot (and its first-run overhead) on every `docker run`.
# Ubuntu's wine64 package only ships a "wine" binary (no separate "wine64"); msvc-wine's
# own scripts fall back the same way, so we mirror that here.
RUN $(command -v wine64 || command -v wine) wineboot --init && while pgrep wineserver > /dev/null; do sleep 1; done

WORKDIR /opt/msvc-wine
RUN git clone https://github.com/mstorsjo/msvc-wine.git . && git checkout "${MSVC_WINE_REF}"

# This project always builds 32-bit (x86 target), but we still need the x64 *target*
# component too: install.sh only builds msvctricks.exe (a small helper the wrapper scripts
# use to reliably reap wine child processes/pipes) when a Hostx64/x64/cl.exe exists, since
# it's shared across all bin/<arch> wrappers. Without it, the bin/x86 wrapper falls back to
# a plain shell-pipe implementation that can hang (ninja waiting forever on a defunct child
# holding a fifo open) - so pull x64 as well purely to get msvctricks.exe built.
RUN PYTHONUNBUFFERED=1 ./vsdownload.py --accept-license --architecture x86 x64 --dest /opt/msvc \
    && ./install.sh /opt/msvc

# Real MSVC toolchain, wrapped for wine, now lives here. Deliberately NOT setting
# CC/CXX=cl globally: vcpkg also needs a native x64-linux compiler (g++, from
# build-essential) to build its own host tools (e.g. detect_compiler); the project's
# own CMake preset (see CMakePresets.json's "wine" preset) sets CMAKE_C/CXX_COMPILER=cl
# explicitly for the actual x86-windows target build.
ENV PATH="/opt/msvc/bin/x86:${PATH}"

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

COPY entrypoint.sh /opt/entrypoint.sh
RUN chmod +x /opt/entrypoint.sh

# GWToolboxdll compiles a few HLSL shaders with fxc.exe (from the Windows SDK, not wrapped
# by msvc-wine itself); drop in a wrapper alongside cl/link/rc following the same pattern.
COPY fxc /opt/msvc/bin/x86/fxc
RUN chmod +x /opt/msvc/bin/x86/fxc

WORKDIR /src
ENTRYPOINT ["/opt/entrypoint.sh"]
CMD ["bash"]
