/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_VECTOR_HPP
#define OSHGUI_DRAWING_VECTOR_HPP

#include "../Exports.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		class OSHGUI_EXPORT Vector
		{
		public:
			Vector();
			Vector(float x, float y, float z);

			float Length() const;
			float DotProduct(const Vector &dot) const;
			Vector Normalize() const;
			float DegreesBetweenVector(const Vector &to) const;
	
			Vector& operator=(float val);
	
			float operator[](unsigned int element);
			operator float *();
			operator const float *() const;

			const Vector operator-() const;

			friend bool operator==(const Vector &lhs, const Vector &rhs);
			friend bool operator<(const Vector &lhs, const Vector &rhs);

			Vector& operator+=(const Vector& rhs);
			Vector& operator-=(const Vector& rhs);
			Vector& operator*=(float rhs);
			Vector& operator/=(float rhs);

			float x;
			float y;
			float z;
		};

		bool operator==(const Vector &lhs, const Vector &rhs);
		inline bool operator!=(const Vector &lhs, const Vector &rhs) { return !(lhs == rhs); }
		bool operator<(const Vector &lhs, const Vector &rhs);
		inline bool operator>(const Vector &lhs, const Vector &rhs) { return rhs < lhs; }
		inline bool operator>=(const Vector &lhs, const Vector &rhs) { return !(rhs < lhs); }
		inline bool operator<=(const Vector &lhs, const Vector &rhs) { return !(rhs > lhs); }
		const Vector operator+(const Vector &lhs, const Vector &rhs);
		const Vector operator-(const Vector &lhs, const Vector &rhs);
		const Vector operator*(float lhs, const Vector &rhs);
		const Vector operator*(const Vector &lhs, float rhs);
		const float operator*(const Vector &lhs, const Vector &rhs);
		const Vector operator/(const Vector &lhs, float rhs);
	}
}

#endif