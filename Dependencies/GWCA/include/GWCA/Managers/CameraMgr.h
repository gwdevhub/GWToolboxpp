#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/GamePos.h>

namespace GW {
    struct Vec3f;
    struct Camera;
    struct Module;

    extern Module CameraModule;

    namespace CameraMgr {
        // ==== Camera ====
        Camera *GetCamera();

        // Change max zoom dist
        bool SetMaxDist(float dist = 900.0f);

        bool SetFieldOfView(float fov);

        // Manual computation of the position of the Camera. (As close as possible to the original)
        Vec3f ComputeCamPos(float dist = 0); // 2.f is the first person dist (const by gw)
        bool UpdateCameraPos();

        float GetFieldOfView();
        float GetYaw();

        // Camera patches -- unlock the camera and return its new state.
        bool UnlockCam(bool flag);
        bool GetCameraUnlock();

        // Enable or Disable the fog & return the state of it
        bool SetFog(bool flag);
    };
}
// C Interop API

namespace GW::CameraMgr {
    extern "C" GWCA_API Camera* __cdecl GW_CameraMgr_GetCamera() noexcept;
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_SetMaxDist(float dist) noexcept;
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_SetFieldOfView(float fov) noexcept;
#ifdef _MSC_VER
    // C linkage fixes the symbol name; the C++ record return is covered by cross-compiler ABI tests.
#pragma warning(push)
#pragma warning(disable: 4190)
#endif
    extern "C" GWCA_API Vec3f __cdecl GW_CameraMgr_ComputeCamPos(float dist) noexcept;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_UpdateCameraPos() noexcept;
    extern "C" GWCA_API float __cdecl GW_CameraMgr_GetFieldOfView() noexcept;
    extern "C" GWCA_API float __cdecl GW_CameraMgr_GetYaw() noexcept;
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_UnlockCam(bool flag) noexcept;
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_GetCameraUnlock() noexcept;
    extern "C" GWCA_API bool __cdecl GW_CameraMgr_SetFog(bool flag) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::CameraMgr {
    inline Camera* GetCamera()
    {
        return GWCA_CALL_C(GW_CameraMgr_GetCamera);
    }

    inline bool SetMaxDist(float dist)
    {
        return GWCA_CALL_C(GW_CameraMgr_SetMaxDist, dist);
    }

    inline bool SetFieldOfView(float fov)
    {
        return GWCA_CALL_C(GW_CameraMgr_SetFieldOfView, fov);
    }

    inline Vec3f ComputeCamPos(float dist)
    {
        return GWCA_CALL_C(GW_CameraMgr_ComputeCamPos, dist);
    }

    inline bool UpdateCameraPos()
    {
        return GWCA_CALL_C(GW_CameraMgr_UpdateCameraPos);
    }

    inline float GetFieldOfView()
    {
        return GWCA_CALL_C(GW_CameraMgr_GetFieldOfView);
    }

    inline float GetYaw()
    {
        return GWCA_CALL_C(GW_CameraMgr_GetYaw);
    }

    inline bool UnlockCam(bool flag)
    {
        return GWCA_CALL_C(GW_CameraMgr_UnlockCam, flag);
    }

    inline bool GetCameraUnlock()
    {
        return GWCA_CALL_C(GW_CameraMgr_GetCameraUnlock);
    }

    inline bool SetFog(bool flag)
    {
        return GWCA_CALL_C(GW_CameraMgr_SetFog, flag);
    }
}
#endif
