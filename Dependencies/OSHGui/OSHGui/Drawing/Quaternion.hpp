/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_QUATERNION_HPP
#define OSHGUI_DRAWING_QUATERNION_HPP

#include <cmath>

namespace OSHGui
{
	namespace Drawing
	{
		class OSHGUI_EXPORT Quaternion
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 */
			inline Quaternion(float _w = 1.0f, float _x = 0.0f, float _y = 0.0f, float _z = 0.0f):
				w(_w),
				x(_x),
				y(_y),
				z(_z)
			{
			
			}

			inline bool operator==(const Quaternion &v) const
			{
				return (w == v.w) && (x == v.x) && (y == v.y) && (z == v.z);
			}

			inline bool operator!=(const Quaternion &v) const
			{
				return !(*this == v);
			}

			inline Quaternion operator-() const
			{
				return Quaternion(-w, -x, -y, -z);
			}

			inline Quaternion operator*(float v) const
			{
				return Quaternion(w * v, x * v, y * v, z * v);
			}

			inline friend Quaternion operator*(float v, const Quaternion &q)
			{
				return Quaternion(v * q.w, v * q.x, v * q.y, v * q.z);
			}

			inline float Dot(const Quaternion &v) const
			{
				return w * v.w + x * v.x + y * v.y + z * v.z;
			}

			inline Quaternion operator+(const Quaternion &v) const
			{
				return Quaternion(w + v.w, x + v.x, y + v.y, z + v.z);
			}

			inline Quaternion operator*(const Quaternion &v) const
			{
				return Quaternion(
					w * v.w - x * v.x - y * v.y - z * v.z,
					w * v.x + x * v.w + y * v.z - z * v.y,
					w * v.y + y * v.w + z * v.x - x * v.z,
					w * v.z + z * v.w + x * v.y - y * v.x
				);
			}

			/**
			 * Berechnet die Länge des Quaternion.
			 *
			 * \return Länge
			 */
			inline float Length() const
			{
				return std::sqrt((w * w) + (x * x) + (y * y) + (z * z));
			}

			/**
			 * Normalisiert das Quaternion.
			 */
			inline float Normalise()
			{
				const float len = Length();
				const float factor = 1.0f / len;
				*this = *this * factor;

				return len;
			}

			static Quaternion EulerAnglesRadians(const float x, const float y, const float z)
			{
				// the order of rotation:
				// 1) around Z
				// 2) around Y
				// 3) around X
				// even though it's passed in reverse...

				const float sin_z_2 = sinf(0.5f * z);
				const float sin_y_2 = sinf(0.5f * y);
				const float sin_x_2 = sinf(0.5f * x);

				const float cos_z_2 = cosf(0.5f * z);
				const float cos_y_2 = cosf(0.5f * y);
				const float cos_x_2 = cosf(0.5f * x);

				return Quaternion(
					cos_z_2 * cos_y_2 * cos_x_2 + sin_z_2 * sin_y_2 * sin_x_2,
					cos_z_2 * cos_y_2 * sin_x_2 - sin_z_2 * sin_y_2 * cos_x_2,
					cos_z_2 * sin_y_2 * cos_x_2 + sin_z_2 * cos_y_2 * sin_x_2,
					sin_z_2 * cos_y_2 * cos_x_2 - cos_z_2 * sin_y_2 * sin_x_2
				);
			}
			//---------------------------------------------------------------------------
			static Quaternion EulerAnglesDegrees(const float x, const float y, const float z)
			{
				static const float d2r = (4.0f * std::atan2(1.0f, 1.0f)) / 180.0f;

				return EulerAnglesRadians(x * d2r, y * d2r, z * d2r);
			}
			//---------------------------------------------------------------------------

			float w;
			float x;
			float y;
			float z;
		};
	}
}

#endif
