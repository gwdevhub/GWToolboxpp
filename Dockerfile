# Builds GWToolboxdll from the sources copied in, then exports bin/ to the host. No volumes: the
# sources go in with COPY, and the outputs come out with --output, so the build works the same on
# any host. The toolchain comes from Dockerfile.toolchain; build.sh builds it first.
ARG BUILDER_IMAGE=gwtoolbox-builder:latest
FROM ${BUILDER_IMAGE} AS build

ARG CONFIG=RelWithDebInfo
ARG TARGET=GWToolboxdll
ARG CMAKE_ARGS=""

WORKDIR /src
COPY . .

# build-xwin persists between builds, so only changed files compile. An interrupted compile leaves
# an empty object that looks up to date, so empty objects are removed first to make them compile again.
RUN --mount=type=cache,target=/src/build-xwin,sharing=locked \
    --mount=type=cache,target=/root/.cache/vcpkg,sharing=locked \
    TOOLCHAIN_ROOT=/opt/xwin-toolchain ./scripts/xwin/setup-toolchain.sh >/dev/null \
 && find build-xwin \( -name '*.o' -o -name '*.obj' \) -size 0 -delete \
 && cmake --preset xwin -DCMAKE_BUILD_TYPE="$CONFIG" $CMAKE_ARGS \
 && cmake --build build-xwin --target "$TARGET" -j"$(nproc)"

FROM scratch AS export
COPY --from=build /src/bin/ /
