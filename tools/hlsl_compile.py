#!/usr/bin/env python3
"""Compile an HLSL shader to a C header, standing in for fxc.exe on non-Windows hosts.

fxc.exe is the only Microsoft compiler that still emits d3dbc (shader model 1-3)
bytecode, and it is a Windows binary -- so a Linux build would otherwise need wine
purely for the twelve small SM3 shaders under GWToolboxdll/Widgets/Minimap/Shaders.
dxc is not an alternative: it dropped every profile below SM6.

vkd3d-shader (wine's own HLSL compiler, usable as a native Linux library) does target
d3dbc, so this script drives vkd3d-compiler and then wraps its raw bytecode in the
same `const BYTE <name>[] = {...}` header that fxc's /Fh + /Vn produce.

It accepts the subset of fxc's command line that GWToolboxdll/CMakeLists.txt uses, so
the CMake custom command is identical either way:

    hlsl_compile.py --vkd3d-compiler <path> /nologo /E main /T ps_3_0 /Vn name \
        /O3 /Zi /Fh out.h shader.hlsl

Caveat worth knowing: vkd3d-shader has no optimiser for SM1-3, so /O3 is accepted and
ignored and the emitted bytecode is longer than fxc's. Register allocation is
unaffected (explicit `register(cN)` bindings are honoured, and vkd3d places its own
literals after them), which is what the renderer's hardcoded SetVertexShaderConstantF
indices actually depend on.
"""

import argparse
import os
import subprocess
import sys
import tempfile

BYTES_PER_ROW = 12
VALUELESS_FXC_FLAGS = {"/nologo", "/Od", "/O0", "/O1", "/O2", "/O3", "/Zi", "/Zpr", "/Zpc"}


def parse_fxc_arguments(argv):
    entry, profile, variable, output, source = "main", None, None, None, None
    index = 0
    while index < len(argv):
        argument = argv[index]
        if argument == "/E":
            entry = argv[index + 1]
            index += 2
        elif argument == "/T":
            profile = argv[index + 1]
            index += 2
        elif argument == "/Vn":
            variable = argv[index + 1]
            index += 2
        elif argument == "/Fh":
            output = argv[index + 1]
            index += 2
        elif argument in VALUELESS_FXC_FLAGS:
            index += 1
        else:
            # Not an else-branch on startswith("/"): a POSIX source path starts with '/' too.
            source = argument
            index += 1
    return entry, profile, variable, output, source


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--vkd3d-compiler", default=os.environ.get("VKD3D_COMPILER", "vkd3d-compiler"))
    options, fxc_argv = parser.parse_known_args()

    entry, profile, variable, output, source = parse_fxc_arguments(fxc_argv)
    missing = [name for name, value in (("/T", profile), ("/Vn", variable), ("/Fh", output), ("source", source)) if not value]
    if missing:
        sys.exit(f"hlsl_compile: missing required argument(s): {', '.join(missing)}")

    bytecode_fd, bytecode_path = tempfile.mkstemp(suffix=".bin")
    os.close(bytecode_fd)
    try:
        result = subprocess.run(
            [options.vkd3d_compiler, "-x", "hlsl", "-p", profile, "-b", "d3dbc",
             "-e", entry, "-o", bytecode_path, source],
            capture_output=True, text=True)
        if result.returncode:
            sys.stderr.write(result.stdout + result.stderr)
            sys.exit(result.returncode)
        with open(bytecode_path, "rb") as bytecode_file:
            bytecode = bytecode_file.read()
    finally:
        os.unlink(bytecode_path)

    if not bytecode:
        sys.exit(f"hlsl_compile: {options.vkd3d_compiler} produced no output for {source}")

    rows = [", ".join(f"{byte:3d}" for byte in bytecode[offset:offset + BYTES_PER_ROW])
            for offset in range(0, len(bytecode), BYTES_PER_ROW)]
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, "w", encoding="utf-8") as header:
        header.write(f"const BYTE {variable}[] =\n{{\n    " + ",\n    ".join(rows) + "\n};\n")


if __name__ == "__main__":
    main()
