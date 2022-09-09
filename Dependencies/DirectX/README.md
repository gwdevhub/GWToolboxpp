# Microsoft.DXSDK.D3DX

Copyright (c) 2002-2021 Microsoft Corporation. All rights reserved.

This package contains the headers, import libraries, and runtime DLLs for the [deprecated](https://docs.microsoft.com/en-us/windows/win32/directx-sdk--august-2009-) D3DX9, D3DX10, and D3DX11 utilities libraries. These versions match the legacy DirectX SDK (June 2010) release. In combination with [XAudio2Redist](https://aka.ms/XAudio2Redist), using [XInput 1.4 or XInput 9.1.0](https://aka.ms/Mu9kn7), and the Windows SDK, it's intended to remove any need to use the legacy DirectX SDK when targeting Windows 7 or later.

This package *does not* rely on the DirectSetup / DXSETUP.EXE / DXWEBSETUP.EXE legacy redistributable (a.k.a. the "DirectX End-User Runtime"). These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

> For Direct3D 11 users, the recommendation is to switch from D3DX11 to one of many open-source replacements to remove the need for this legacy NuGet package. For more information, see [Living without D3DX](https://aka.ms/Kfsdiu).

> The DirectX 12 utility library D3DX12 is header-only and has no library file or runtime DLL. You obtain the latest version from [GitHub](https://raw.githubusercontent.com/microsoft/DirectX-Headers/main/include/directx/d3dx12.h).

## Notices

All content and source code for this package are subject to the terms of the LICENSE file. You are given permission to redistribute the following binary files 'application local' unmodified with your application:

```
build\native\release\bin\x64\D3DCompiler_43.dll
build\native\release\bin\x64\D3DX9_43.dll
build\native\release\bin\x64\d3dx10_43.dll
build\native\release\bin\x64\d3dx11_43.dll
build\native\release\bin\x86\D3DCompiler_43.dll
build\native\release\bin\x86\D3DX9_43.dll
build\native\release\bin\x86\d3dx10_43.dll
build\native\release\bin\x86\d3dx11_43.dll
```

Debug versions of the above libraries can be used for testing, but should **not** be shipped with your product:

```
build\native\debug\bin\x64\D3DX9d_43.dll
build\native\debug\bin\x64\d3dx10d_43.dll
build\native\debug\bin\x64\d3dx11d_43.dll
build\native\debug\bin\x86\D3DX9d_43.dll
build\native\debug\bin\x86\d3dx10d_43.dll
build\native\debug\bin\x86\d3dx11d_43.dll
```

See the NOTICE file for third-party notices.

The ``D3DX_DXGIFormatConvert.inl`` header (and no other header in this package) is available under the [MIT License](http://opensource.org/licenses/MIT).

## Platform support

These DLLs support Windows 7, Windows 8.0, Windows 8.1, and Windows 10 for both x86 and x64 native versions.

D3DX9, D3DX10, and D3DX11 are *not* supported for use in Universal Windows Platform (UWP) apps or on Xbox.

There is no ARM or ARM64 native version of D3DX9, D3DX10, and/or D3DX11 available.

The DirectX Developer Runtime (i.e. the Direct3D "DEBUG device") enabled by ``D3D11_CREATE_DEVICE_DEBUG`` and/or ``D3D10_CREATE_DEVICE_FLAG`` flags is not provided in this package. For Windows 10, this is installed via the "Graphics Tools" Windows optional feature. For Windows 7, Windows 8.1, and Windows 10 this is installed by the Windows SDK. The Debug versions of the D3DX9, D3XD10, and D3DX11 DLLs are included. For more information see [Microsoft Docs](https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-layers).

> The Direct3D 9 DirectX Runtime is not available for Windows 8.0, Windows 8.1, or Windows 10. It is available on Windows 7 via the legacy DirectX SDK.

## Documentation

Documentation for legacy D3DX is found on **Microsoft Docs**

* [D3DX (Direct3D 9)](https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dx)
* [D3DX (Direct3D 10)](https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-reference-d3dx10)
* [D3DX11](https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d11-graphics-reference-d3dx11)
* [HLSL](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-unpacking-packing-dxgi-format)

## Release Notes

* Use of the D3DX9 shader flag ``D3DXSHADER_USE_LEGACY_D3DX9_31_DLL`` is not supported. This flag loads the ``D3DX9_31.DLL`` (October 2006) version of D3DX9 for legacy HLSL Pixel Shader 1.x support, and is not included in this package.

* The DLLs in this NuGet package have been SHA-256 Authenticode signed. The original legacy DirectX SDK (June 2010) DLLs were SHA-1 signed. Microsoft has deprecated use of SHA-1 signing. See [Microsoft Docs](https://docs.microsoft.com/en-us/lifecycle/announcements/sha-1-signed-content-retired).

* The "D3DCompiler_43.dll" is provided in this package as it is used by D3DX when using ``D3DXCompileShader ``, ``D3DX10CompileFromFile``, ``D3DX11CompileFromFile``, etc. It is recommended to use the D3DCompiler API directly using the header, import library, and runtime DLL from the Windows SDK (i.e. "D3DCompiler_47.dll" which is included in Windows 8.1 and Windows 10, and is available for 'application local' deployment for Windows 7 support). Direct use of "D3DCompiler_43.dll" is *not* supported by this package.

* The GUID constants from ``D3DX9.H`` are not included in the Windows 10 SDK ``dxguid.lib`` library. To define them, in one of your code modules use this pattern:

```
#undef DEFINE_GUID
#include <initguid.h>
#include <d3dx9.h>
```

* If building with the ``v1xx_xp`` Platform Toolset which uses the Windows 7.1A SDK for Windows XP SP3 / Windows Server 2003 SP1 compatibility, use Microsoft.DXSDK.D3DX version 9.29.952.7 or use the legacy DirectX SDK.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

# Version History

9.29.952.8 (April 2021) - Minor header updates for better integration with the modern Windows SDK

9.29.952.7 (March 2021) - Removed dxfile.h, d3dxof.lib X-File (legacy)

9.29.952.3 (February 2021) - First release on nuget.org
