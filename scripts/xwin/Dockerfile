# Linux cross-build image for GWToolboxdll (x86 Windows target).
#
# GWToolboxdll is injected into the (32-bit) Guild Wars process, so it must be built
# x86/Win32 against the MSVC ABI. clang targets i686-pc-windows-msvc directly, and xwin
# fetches the MSVC CRT + Windows SDK headers/libs from Microsoft's official installer
# manifests (the same ones Visual Studio's installer uses, under the VS Build Tools EULA)
# -- so no Microsoft binary ever has to execute. lld-link, llvm-rc, llvm-lib and llvm-mt
# replace link/rc/lib/mt, and vkd3d-shader replaces fxc for the SM3 shaders.
FROM ubuntu:24.04

# autoconf/automake/libtool/bison/flex/libjson-perl/libvulkan-dev are all here for vkd3d,
# which builds from source (see setup-toolchain.sh). Its configure hard-requires Vulkan even
# though only libvkd3d-shader is wanted and nothing here renders with Vulkan.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gnupg \
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
        libvulkan-dev \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# NOT Ubuntu's clang: 24.04 ships 18, and the MSVC STL that xwin fetches refuses it outright
# ("error STL1000: Unexpected compiler version, expected Clang 19.0.0 or newer"), so the image
# would build and then fail on the first translation unit. Take a pinned build from LLVM's own
# apt repo instead. Ubuntu also never ships a clang-cl binary - it is the same executable as
# clang, selected by argv[0] - so symlink it, along with the llvm-* tools the toolchain names.
ARG LLVM_VERSION=21
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key -o /etc/apt/trusted.gpg.d/llvm.asc \
    && echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-${LLVM_VERSION} main" > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang-${LLVM_VERSION} lld-${LLVM_VERSION} llvm-${LLVM_VERSION} \
    && ln -sf "/usr/lib/llvm-${LLVM_VERSION}/bin/clang" /usr/bin/clang-cl \
    && for tool in clang lld-link llvm-rc llvm-lib llvm-mt llvm-ar; do \
           ln -sf "/usr/lib/llvm-${LLVM_VERSION}/bin/${tool}" "/usr/bin/${tool}"; \
       done \
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
COPY setup-toolchain.sh /opt/xwin/setup-toolchain.sh
RUN chmod +x /opt/xwin/setup-toolchain.sh && /opt/xwin/setup-toolchain.sh

ENV XWIN_SDK=/opt/xwin-toolchain/xwin-sdk
ENV VKD3D_COMPILER=/opt/xwin-toolchain/vkd3d-1.19/vkd3d-compiler

COPY build-in-container.sh /opt/build-in-container.sh
RUN chmod +x /opt/build-in-container.sh

WORKDIR /src
CMD ["/opt/build-in-container.sh"]
