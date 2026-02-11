#pragma once
//#include<numbers>
#include<cmath>
#include "../Visuals/External.h"

struct view_matrix_t
{
	float* operator[](int index)
	{
		return matrix[index];
	}
	const float* operator[](int index) const
	{
		return matrix[index];
	}
	float matrix[4][4];
};

struct Vector
{
public:
	// constructor
	constexpr Vector(
		const float x = 0.f,
		const float y = 0.f,
		const float z = 0.f) noexcept :
		x(x), y(y), z(z) { }

	//operator 重载

	constexpr Vector operator-(const Vector& other) const noexcept
	{
		return Vector{ x - other.x, y - other.y, z - other.z };
	}

	constexpr Vector operator+(const Vector& other) const noexcept
	{
		return Vector{ x + other.x, y + other.y, z + other.z };
	}

	constexpr Vector operator/(const float factor) const noexcept
	{
		return Vector{ x / factor, y / factor, z / factor };
	}

	constexpr Vector operator*(const float factor) const noexcept
	{
		return Vector{ x * factor, y * factor, z * factor };
	}

	//计算三维空间内点(x,y,z)距离坐标轴原点的平方距离
	float CalcDis2Orign3DSqr() const
	{
		return x * x + y * y + z * z;
	}
	//计算三维空间内点(x,y,z)距离坐标轴原点的距离
	float CalcDis2Orign3D() const
	{
		return sqrtf(CalcDis2Orign3DSqr());
	}
	//计算三维空间内点(x,y,z)距离另一个点的平方距离
	float CalcDis2Point3DSqr(const Vector& pos) const
	{
		const float dx = pos.x - x;
		const float dy = pos.y - y;
		const float dz = pos.z - z;
		return dx * dx + dy * dy + dz * dz;
	}
	//计算三维空间内点(x,y,z)距离另一个点的距离
	float CalcDis2Point3D(const Vector& pos) const
	{
		return sqrtf(CalcDis2Point3DSqr(pos));
	}
	//计算二维空间内点(x,y)距离坐标轴原点的平方距离
	float CalculateDistance2DSqr() const
	{
		return x * x + y * y;
	}
	//计算二维空间内点(x,y)距离坐标轴原点的距离
	float CalculateDistance2D() const
	{
		return sqrtf(CalculateDistance2DSqr());
	}
	//计算二维空间内点(x,y)距离另一个点的平方距离
	float CalculateDistanceToPoint2DSqr(const Vector& pos) const
	{
		const float dx = pos.x - x;
		const float dy = pos.y - y;
		return dx * dx + dy * dy;
	}
	//计算二维空间内点(x,y)距离另一个点的距离
	float CalculateDistanceToPoint2D(const Vector& pos) const
	{
		return sqrtf(CalculateDistanceToPoint2DSqr(pos));
	}


	Vector world2screen(view_matrix_t vm) const
	{
		float _x = vm[0][0] * x + vm[0][1] * y + vm[0][2] * z + vm[0][3];
		float _y = vm[1][0] * x + vm[1][1] * y + vm[1][2] * z + vm[1][3];

		float w = vm[3][0] * x + vm[3][1] * y + vm[3][2] * z + vm[3][3];

		if (w < 0.1f) {
			return false;
		}

		float inverseWidth = 1.f / w;

		_x *= inverseWidth;
		_y *= inverseWidth;

		float x = Visual::external.gamewindow.size.x / 2;
		float y = Visual::external.gamewindow.size.y / 2;

		x += 0.5f * _x * Visual::external.gamewindow.size.x + 0.5f;
		y -= 0.5f * _y * Visual::external.gamewindow.size.y + 0.5f;


		return { x,y,w };
	}

	bool IsZero() const
	{
		return x == 0.0f && y == 0.0f && z == 0.0f;
	}

	float x, y, z;
};
