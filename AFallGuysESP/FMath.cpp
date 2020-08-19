#include "FMath.h"
using namespace DirectX;
using namespace std;

FVector WorldToScreen(__m128 WorldLocation, XMMATRIX cam)
{
	FVector Screenlocation = FVector{ 0, 0, 0 };

	FVector translationVector = FVector{ cam.r[3] };
	FVector up = FVector{ cam.r[1] };
	FVector right = FVector{ cam.r[0] };
	float w = translationVector.Dot(WorldLocation) + cam.r[3].m128_f32[3];

	float y = up.Dot(WorldLocation) + cam.r[1].m128_f32[3];
	float x = right.Dot(WorldLocation) + cam.r[0].m128_f32[3];

	Screenlocation.x = (1.f + x / w) / 2;
	Screenlocation.y = (1.f - y / w) / 2;
	Screenlocation.z = w;

	return Screenlocation;
}

