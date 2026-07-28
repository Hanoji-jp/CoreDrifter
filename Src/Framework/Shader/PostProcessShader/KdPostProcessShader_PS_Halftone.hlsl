#include "inc_KdPostProcessShader.hlsli"

// 画面全体に印刷物のハーフトーン(網点)を掛ける。
// 暗い所ほど網点を強く出す＝コミック/シルクスクリーン風の階調表現。
// 明るい所は白く飛ばして紙の白さを残す。
Texture2D g_inputTex : register(t0);
SamplerState g_ss : register(s0);

cbuffer cb : register(b0)
{
	float2 g_ScreenSize;   // 画面サイズ(px)
	float  g_Scale;        // 網点の周期(px)。小さいほど細かい
	float  g_Strength;     // 網点の濃さ(0=無効)

	float  g_DarkBias;     // 暗い所ほど強く出す量(0=一律)
	float3 g_pad;
};

float4 main(VSOutput In) : SV_Target0
{
	float4 c = g_inputTex.Sample(g_ss, In.UV);

	if (g_Strength <= 0.001f) { return c; }

	// ピクセル座標。網点は45°に傾けるのが印刷の定石(格子が目立たずモアレも出にくい)
	float2 px = In.UV * g_ScreenSize;
	float2 sp = float2(px.x * 0.7071f - px.y * 0.7071f,
	                   px.x * 0.7071f + px.y * 0.7071f);
	sp /= max(g_Scale, 1.0f);

	float2 cell = frac(sp) - 0.5f;
	float  d    = length(cell) * 2.0f;              // 0(セル中心)〜1
	float  dotMask = smoothstep(0.95f, 0.30f, d);   // 中心ほど1

	// 明るさ：暗い所ほど網点を効かせる
	float lum = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
	float amt = g_Strength * lerp(1.0f, 1.0f - lum, g_DarkBias);

	c.rgb *= 1.0f - amt * (1.0f - dotMask);
	return c;
}
