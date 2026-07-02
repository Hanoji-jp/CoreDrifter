#pragma once
#include <cmath>
#include <DirectXMath.h>

struct KdEase
{
	inline float InSine(float t)
	{
		return 1.0f - cosf((t * DirectX::XM_PI) * 0.5f);
	}

	inline float OutSine(float t)
	{
		return sinf((t * DirectX::XM_PI) * 0.5f);
	}

	inline float InOutSine(float t)
	{
		return -(cosf(DirectX::XM_PI * t) - 1.0f) * 0.5f;
	}

	// 指数イージング：入口・出口で急激に加減速、中間が爆速
	// ワープ・テレポート・高速移動系に最適
	inline float InOutExpo(float t)
	{
		if (t <= 0.0f) { return 0.0f; }
		if (t >= 1.0f) { return 1.0f; }
		if (t < 0.5f)
		{
			return powf(2.0f, 20.0f * t - 10.0f) * 0.5f;
		}
		return (2.0f - powf(2.0f, -20.0f * t + 10.0f)) * 0.5f;
	}

	// 3次イージング：InOutSine より強め、InOutExpo より穏やか
	inline float InOutCubic(float t)
	{
		if (t < 0.5f)
		{
			return 4.0f * t * t * t;
		}
		const float u = -2.0f * t + 2.0f;
		return 1.0f - u * u * u * 0.5f;
	}

	inline float OutBounce(float t)
	{
		constexpr float n1 = 7.5625f;
		constexpr float d1 = 2.75f;

		if (t < 1.0f / d1)
		{
			return n1 * t * t;
		}
		else if (t < 2.0f / d1)
		{
			return n1 * (t -= 1.5f / d1) * t + 0.75f;
		}
		else if (t < 2.5f / d1)
		{
			return n1 * (t -= 2.25f / d1) * t + 0.9375f;
		}
		else
		{
			return n1 * (t -= 2.625f / d1) * t + 0.984375f;
		}
	}
};
