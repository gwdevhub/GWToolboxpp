#pragma once

#include <GWCA/GameContainers/GamePos.h>

namespace GW {
    struct Camera {
        /* +h0000 */ uint32_t look_at_agent_id;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ float h0008;
        /* +h000C */ float h000C;
        /* +h0010 */ float max_distance;
        /* +h0014 */ float h0014;
        /* +h0018 */ float yaw; // left/right camera angle, radians w/ origin @ east
        /* +h001C */ float pitch; // up/down camera angle, range of [-1,1]
        /* +h0020 */ float distance; // current distance from players head.
        /* +h0024 */ uint32_t h0024[4];
        /* +h0034 */ float yaw_right_click;
        /* +h0038 */ float yaw_right_click2;
        /* +h003C */ float pitch_right_click;
        /* +h0040 */ float distance2;
        /* +h0044 */ float acceleration_constant;
        /* +h0048 */ float time_since_last_keyboard_rotation; // In seconds it seems.
        /* +h004C */ float time_since_last_mouse_rotation;
        /* +h0050 */ float time_since_last_mouse_move;
        /* +h0054 */ float time_since_last_agent_selection;
        /* +h0058 */ float time_in_the_map;
        /* +h005C */ float time_in_the_district;
        /* +h0060 */ float yaw_to_go;
        /* +h0064 */ float pitch_to_go;
        /* +h0068 */ float dist_to_go;
        /* +h006C */ float max_distance2;
        /* +h0070 */ float h0070[2];
        /* +h0078 */ Vec3f position;
        /* +h0084 */ Vec3f camera_pos_to_go;
        /* +h0090 */ Vec3f cam_pos_inverted;
        /* +h009C */ Vec3f cam_pos_inverted_to_go;
        /* +h00A8 */ Vec3f look_at_target;
        /* +h00B4 */ Vec3f look_at_to_go;
        /* +h00C0 */ float field_of_view;
        /* +h00C4 */ float field_of_view2;
        // ...

        float GetYaw()          const { return yaw; }
        float GetPitch()        const { return pitch; }

        /// \brief This is not the FoV that GW uses to render
        /// see GW::Render::GetFieldOfView()
        float GetFieldOfView()  const { return field_of_view; }

        void SetYaw(float _yaw) {
            yaw_to_go = _yaw;
            yaw = _yaw;
        }
        float GetCurrentYaw() const
        {
            const Vec3f dir = position - look_at_target;
            const float curtan = atan2(dir.y, dir.x);
            constexpr float pi = 3.141592741f;
            float curyaw;
            if (curtan >= 0) {
                curyaw = curtan - pi;
            }
            else {
                curyaw = curtan + pi;
            }
            return curyaw;
        }

        void SetPitch(float _pitch) {
            pitch_to_go = _pitch;
        }

        float GetCameraZoom()     const { return distance; }
        Vec3f GetLookAtTarget()   const { return look_at_target; }
        Vec3f GetCameraPosition() const { return position; }

        void SetCameraPos(Vec3f newPos) {
            this->position.x = newPos.x;
            this->position.y = newPos.y;
            this->position.z = newPos.z;
        }

        void SetLookAtTarget(Vec3f newPos) {
            this->look_at_target.x = newPos.x;
            this->look_at_target.y = newPos.y;
            this->look_at_target.z = newPos.z;
        }
    };
}
