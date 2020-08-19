#include <immintrin.h>
#include <DirectXMath.h>

class FVector
{
public:
	float x;
	float y;
	float z;
//	float w;
	FVector()
	{
		x = 0;
		y = 0;
		z = 0;
	}
	FVector(float _x,float _y,float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
	FVector(__m128 b)
	{
		x = b.m128_f32[0];
		y = b.m128_f32[1];
		z = b.m128_f32[2];
	}
	operator __m128() const
	{
		return {x,y,z,0};
	};
	float Dot(FVector b)
	{
		return this->x*b.x + this->y*b.y + this->z*b.z;
	}
};

class FRotator
{
public:
	float Pitch;
	float Yaw;
	float Roll;

	FRotator()
	{
		Pitch = 0;
		Yaw = 0;
		Roll = 0;
	}
	FRotator(float _Pitch, float _Yaw, float _Roll)
	{
		Pitch = _Pitch;
		Yaw = _Yaw;
		Roll = _Roll;
	}
	operator __m128() const
	{
		return { Pitch,Yaw,Roll,0 };
	};
	FRotator(__m128 b)
	{
		Pitch = b.m128_f32[0];
		Yaw = b.m128_f32[1];
		Roll = b.m128_f32[2];
	}
	float Dot(FRotator b)
	{
		return this->Pitch*b.Pitch + this->Yaw*b.Yaw + this->Roll*b.Roll;
	}
	FRotator normalize()
	{
		float length = sqrtf(this->Dot(*this));
		return FRotator{
			Pitch/ length,
			Yaw / length,
			Roll / length
		};
	}
};

#pragma pack(push, 16)
class FTransform
{
public:
	__m128 Rotation;
	FVector Translation;
	FVector Scale3D;
};
#pragma pack(pop)

FVector WorldToScreen(__m128 WorldLocation, DirectX::XMMATRIX cam);