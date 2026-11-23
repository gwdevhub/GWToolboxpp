#!/usr/bin/env python3
"""Compile an HLSL shader to a C header, standing in for fxc.exe where it isn't installed.

On Windows without the Windows SDK, --d3dcompiler calls D3DCompile in d3dcompiler_47.dll, which ships with
Windows itself and is the compiler fxc.exe wraps, so its output matches fxc's. On Linux, vkd3d-compiler is used.

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
import ctypes
import os
import subprocess
import sys
import tempfile

BYTES_PER_ROW = 12
VALUELESS_FXC_FLAGS = {"/nologo", "/Od", "/O0", "/O1", "/O2", "/O3", "/Zi", "/Zpr", "/Zpc"}
D3DCOMPILE_FLAGS = {"/Zi": 1 << 0, "/Od": 1 << 2, "/Zpr": 1 << 3, "/Zpc": 1 << 4,
                    "/O0": 1 << 14, "/O1": 0, "/O2": (1 << 14) | (1 << 15), "/O3": 1 << 15}


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


def compile_with_d3dcompiler(entry, profile, source, fxc_argv):
    compiler = ctypes.WinDLL("d3dcompiler_47.dll")
    flags = 0
    for argument in fxc_argv:
        flags |= D3DCOMPILE_FLAGS.get(argument, 0)
    with open(source, "rb") as source_file:
        text = source_file.read()
    code, errors = ctypes.c_void_p(), ctypes.c_void_p()
    result = compiler.D3DCompile(text, len(text), source.encode(), None, None, entry.encode(), profile.encode(),
                                 flags, 0, ctypes.byref(code), ctypes.byref(errors))

    def blob_bytes(blob):
        # ID3DBlob vtable: 0 QueryInterface, 1 AddRef, 2 Release, 3 GetBufferPointer, 4 GetBufferSize.
        vtable = ctypes.cast(ctypes.cast(blob, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
        method = lambda index, restype: ctypes.WINFUNCTYPE(restype, ctypes.c_void_p)(vtable[index])
        data = ctypes.string_at(method(3, ctypes.c_void_p)(blob), method(4, ctypes.c_size_t)(blob))
        method(2, ctypes.c_ulong)(blob)
        return data

    message = blob_bytes(errors).decode(errors="replace") if errors.value else ""
    if result < 0:
        sys.exit(f"hlsl_compile: D3DCompile failed for {source} (0x{result & 0xFFFFFFFF:08X})\n{message}")
    return blob_bytes(code)


def compile_with_vkd3d(vkd3d_compiler, entry, profile, source):
    bytecode_fd, bytecode_path = tempfile.mkstemp(suffix=".bin")
    os.close(bytecode_fd)
    try:
        result = subprocess.run(
            [vkd3d_compiler, "-x", "hlsl", "-p", profile, "-b", "d3dbc",
             "-e", entry, "-o", bytecode_path, source],
            capture_output=True, text=True)
        if result.returncode:
            sys.stderr.write(result.stdout + result.stderr)
            sys.exit(result.returncode)
        with open(bytecode_path, "rb") as bytecode_file:
            return bytecode_file.read()
    finally:
        os.unlink(bytecode_path)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--vkd3d-compiler", default=os.environ.get("VKD3D_COMPILER", "vkd3d-compiler"))
    parser.add_argument("--d3dcompiler", action="store_true")
    options, fxc_argv = parser.parse_known_args()

    entry, profile, variable, output, source = parse_fxc_arguments(fxc_argv)
    missing = [name for name, value in (("/T", profile), ("/Vn", variable), ("/Fh", output), ("source", source)) if not value]
    if missing:
        sys.exit(f"hlsl_compile: missing required argument(s): {', '.join(missing)}")

    if options.d3dcompiler:
        bytecode = compile_with_d3dcompiler(entry, profile, source, fxc_argv)
    else:
        bytecode = compile_with_vkd3d(options.vkd3d_compiler, entry, profile, source)
    if not bytecode:
        sys.exit(f"hlsl_compile: no bytecode produced for {source}")

    rows = [", ".join(f"{byte:3d}" for byte in bytecode[offset:offset + BYTES_PER_ROW])
            for offset in range(0, len(bytecode), BYTES_PER_ROW)]
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, "w", encoding="utf-8") as header:
        header.write(f"const BYTE {variable}[] =\n{{\n    " + ",\n    ".join(rows) + "\n};\n")


if __name__ == "__main__":
    main()
