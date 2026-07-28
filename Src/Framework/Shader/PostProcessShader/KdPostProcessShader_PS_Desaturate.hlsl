#include "inc_KdPostProcessShader.hlsli"

Texture2D g_inputTex : register(t0);
SamplerState g_ss : register(s0);

cbuffer cb : register(b0)
{
	float g_saturation;   // 0=グレースケール, 1=フルカラー
	float3 _pad;
};

float4 main(VSOutput In) : SV_Target0
{
	float4 c = g_inputTex.Sample(g_ss, In.UV);
	// 輝度(Rec.601)へ寄せてから彩度で戻す
	float luma = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
	c.rgb = lerp(float3(luma, luma, luma), c.rgb, g_saturation);
	return c;
}
