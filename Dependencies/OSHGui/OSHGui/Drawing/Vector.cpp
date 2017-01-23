#include "Vector.hpp"

#include <cmath>

namespace OSHGui
{
	namespace Drawing
	{
		Vector::Vector()
			: x(0.0f),
			  y(0.0f),
			  z(0.0f)
		{
	
		}
		//---------------------------------------------------------------------------
		Vector::Vector(float x, float y, float z)
			: x(x),
			  y(y),
			  z(z)
		{

		}
		//---------------------------------------------------------------------------
		float Vector::Length() const
		{
			return std::sqrtf(x * x + y * y + z * z);
		}
		//---------------------------------------------------------------------------
		float Vector::DotProduct(const Vector &v) const
		{
			return x * v.x + y * v.y + z * v.z;
		}
		//---------------------------------------------------------------------------
		Vector Vector::Normalize() const
		{
			auto length = Length();
			if (length != 0.0f)
			{
				return *this / length;
			}
			else
			{
				return Vector(1.f, 1.f, 1.f);
			}
		}
		//---------------------------------------------------------------------------
		float Vector::DegreesBetweenVector(const Vector &v) const
		{
			return (180.0f / 3.141f) * std::acosf(DotProduct(v) / (Length() * v.Length()));
		}
		//---------------------------------------------------------------------------
		Vector& Vector::operator=(float rhs)
		{
			x = y = z = rhs;

			return *this;
		}
		//---------------------------------------------------------------------------
		float Vector::operator[](unsigned int element)
		{
			return (&x)[element];
		}
		//---------------------------------------------------------------------------
		Vector::operator float *()
		{
			return &x;
		}
		//---------------------------------------------------------------------------
		Vector::operator const float *() const
		{
			return &x;
		}
		//---------------------------------------------------------------------------
		const Vector Vector::operator-() const
		{
			return Vector(-x, -y, -z);
		}
		//---------------------------------------------------------------------------
		Vector& Vector::operator+=(const Vector &rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;

			return *this;
		}
		//---------------------------------------------------------------------------
		Vector& Vector::operator-=(const Vector &rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;

			return *this;
		}
		//---------------------------------------------------------------------------
		Vector& Vector::operator*=(float rhs)
		{
			x *= rhs;
			y *= rhs;
			z *= rhs;

			return *this;
		}
		//---------------------------------------------------------------------------
		Vector& Vector::operator/=(float rhs)
		{
			x /= rhs;
			y /= rhs;
			z /= rhs;

			return *this;
		}
		//---------------------------------------------------------------------------
		bool operator==(const Vector &lhs, const Vector &rhs)
		{
			return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
		}
		//---------------------------------------------------------------------------
		bool operator<(const Vector &lhs, const Vector &rhs)
		{
			return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z;
		}
		//---------------------------------------------------------------------------
		const Vector operator+(const Vector &lhs, const Vector &rhs)
		{
			Vector ret(lhs);
			ret += rhs;
			return ret;
		}
		//---------------------------------------------------------------------------
		const Vector operator-(const Vector &lhs, const Vector &rhs)
		{
			Vector ret(lhs);
			ret -= rhs;
			return ret;
		}
		//---------------------------------------------------------------------------
		const Vector operator*(float lhs, const Vector &rhs)
		{
			Vector ret(rhs);
			ret *= lhs;
			return ret;
		}
		//---------------------------------------------------------------------------
		const Vector operator*(const Vector &lhs, float val)
		{
			Vector ret(lhs);
			ret *= val;
			return ret;
		}
		//---------------------------------------------------------------------------
		const float operator*(const Vector &lhs, const Vector &rhs)
		{
			return lhs.DotProduct(rhs);
		}
		//---------------------------------------------------------------------------
		const Vector operator/(const Vector &lhs, float rhs)
		{
			Vector ret(lhs);
			ret /= rhs;
			return ret;
		}
		//---------------------------------------------------------------------------
	}
}