# Linux cross-build image for GWToolboxdll (x86 Windows target).
#
# GWToolboxdll is injected into the (32-bit) Guild Wars process, so it must be built
# x86/Win32 against the MSVC ABI. clang targets i686-pc-windows-msvc directly, and xwin
# fetches the MSVC CRT + Windows SDK headers/libs from Microsoft's official installer
# manifests (the same ones Visual Studio's installer uses, under the VS Build Tools EULA)
# -- so no Microsoft binary ever has to execute. lld-link, llvm-rc, llvm-lib and llvm-mt
# replace link/rc/lib/mt, and vkd3d-shader replaces fxc for the SM3 shaders.
FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        lld \
        llvm \
        curl \
        git \
        ninja-build \
        pkg-config \
        python3 \
        tar \
        xz-utils \
        zip \
        unzip \
        build-essential \
        autoconf \
        automake \
        libtool \
        bison \
        flex \
        libjson-perl \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# The repo's CMakeLists.txt requires >= 3.29; Ubuntu 24.04's apt cmake is 3.28.
ARG CMAKE_VERSION=4.3.3
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
        | tar -xz -C /opt \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest" /usr/local/bin/ctest \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cpack" /usr/local/bin/cpack

# vcpkg still builds its own host tools with the native g++ (build-essential above); the
# x86-windows target build uses clang via the preset's chainloaded toolchain.
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Bake the SDK and vkd3d into the image rather than fetching ~800MB on every run. The
# header-casing symlink pass in setup-toolchain.sh reads the repo, which isn't present at
# image build time, so it re-runs (cheaply) from the entrypoint against the mounted source.
ENV TOOLCHAIN_ROOT=/opt/xwin-toolchain
COPY setup-toolchain.sh /opt/setup-toolchain.sh
RUN mkdir -p /src/scripts/xwin \
    && cp /opt/setup-toolchain.sh /src/scripts/xwin/setup-toolchain.sh \
    && chmod +x /src/scripts/xwin/setup-toolchain.sh \
    && /src/scripts/xwin/setup-toolchain.sh \
    && rm -rf /src/scripts

ENV XWIN_SDK=/opt/xwin-toolchain/xwin-sdk
ENV VKD3D_COMPILER=/opt/xwin-toolchain/vkd3d-1.19/vkd3d-compiler

COPY build-in-container.sh /opt/build-in-container.sh
RUN chmod +x /opt/build-in-container.sh

WORKDIR /src
CMD ["/opt/build-in-container.sh"]
