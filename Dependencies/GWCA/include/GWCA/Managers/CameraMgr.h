#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct Vec3f;
    struct Camera;
    struct Module;

    extern Module CameraModule;

    namespace CameraMgr {
        // ==== Camera ====
        GWCA_API Camera *GetCamera();

        // Change max zoom dist
        GWCA_API bool SetMaxDist(float dist = 900.0f);

        GWCA_API bool SetFieldOfView(float fov);

        // Manual computation of the position of the Camera. (As close as possible to the original)
        GWCA_API Vec3f ComputeCamPos(float dist = 0); // 2.f is the first person dist (const by gw)
        GWCA_API bool UpdateCameraPos();

        GWCA_API float GetFieldOfView();
        GWCA_API float GetYaw();

        // ==== Camera patches ====
        // Unlock camera & return the new state of it
        GWCA_API bool UnlockCam(bool flag);
        GWCA_API bool GetCameraUnlock();

        // Enable or Disable the fog & return the state of it
        GWCA_API bool SetFog(bool flag);
    };
}
