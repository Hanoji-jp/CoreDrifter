#include "../inc_KdCommon.hlsli"
#include "inc_KdPostProcessShader.hlsli"

// 煙の専用RT(アルファ=カバレッジ)から、塊全体のシルエット外周にだけ輪郭線を乗せる。
// 1粒ごとではなく「合成後のシルエット」に線を引くので、内部が線だらけにならず綺麗。
// 出力はアルファ付き＝この後シーンへアルファ合成して煙＋輪郭を重ねる。
Texture2D    g_smokeTex : register(t0);   // 煙色+アルファ(専用RT)
SamplerState g_ss       : register(s0);

cbuffer cb : register(b0)
{
	float2 g_TexelSize;      // 1/幅, 1/高さ
	float  g_Thickness;      // 線の太さ(テクセル)
	float  g_AlphaThreshold; // シルエット判定のアルファしきい値

	float  g_EdgeStrength;   // 線の濃さ
	float  g_BlurRadius;     // 煙のぼかし半径(テクセル。0で無効)
	float2 g_soPad;

	float4 g_OutlineColor;   // 線の色(rgb)＋不透明度(a)
};

float4 main(VSOutput In) : SV_Target0
{
	float2 uv = In.UV;

	// 煙をその場でガウスぼかし(3x3)。板ポリの硬い縁が溶けて柔らかい煙になり、
	// 消える粒も空間的に均されてポップが目立たなくなる。RTを増やさず1パスで完結＝安全。
	float2 b = g_TexelSize * g_BlurRadius;
	float4 c = g_smokeTex.Sample(g_ss, uv) * 4.0f;
	c += g_smokeTex.Sample(g_ss, uv + float2( b.x, 0.0f)) * 2.0f;
	c += g_smokeTex.Sample(g_ss, uv + float2(-b.x, 0.0f)) * 2.0f;
	c += g_smokeTex.Sample(g_ss, uv + float2(0.0f,  b.y)) * 2.0f;
	c += g_smokeTex.Sample(g_ss, uv + float2(0.0f, -b.y)) * 2.0f;
	c += g_smokeTex.Sample(g_ss, uv + float2( b.x,  b.y));
	c += g_smokeTex.Sample(g_ss, uv + float2(-b.x,  b.y));
	c += g_smokeTex.Sample(g_ss, uv + float2( b.x, -b.y));
	c += g_smokeTex.Sample(g_ss, uv + float2(-b.x, -b.y));
	c /= 16.0f;

	float2 o = g_TexelSize * g_Thickness;

	// 近傍アルファの最大値＝シルエットを太らせた(dilate)版
	float aMax = c.a;
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2( o.x, 0.0f)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2(-o.x, 0.0f)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2(0.0f,  o.y)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2(0.0f, -o.y)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2( o.x,  o.y)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2(-o.x,  o.y)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2( o.x, -o.y)).a);
	aMax = max(aMax, g_smokeTex.Sample(g_ss, uv + float2(-o.x, -o.y)).a);

	// カバレッジ(内側=1 / 外側=0)。step だと薄い煙がしきい値を横切った瞬間に
	// 輪郭がパッと消えるので、smoothstep で柔らかく判定してフェードさせる。
	float band   = g_AlphaThreshold * 0.6f;
	float covC   = smoothstep(g_AlphaThreshold - band, g_AlphaThreshold + band, c.a);
	float covMax = smoothstep(g_AlphaThreshold - band, g_AlphaThreshold + band, aMax);
	float ring   = saturate(covMax - covC) * g_EdgeStrength;
	// 薄くなった煙では輪郭自体も弱める(溶ける煙の線が最後にパッと消えるのを防ぐ)
	ring *= smoothstep(0.0f, g_AlphaThreshold, aMax);

	// 専用RTへ「over」合成した煙色はプリマルチ状態なので、ストレートアルファへ割り戻す。
	// (この後シーンへ通常αで合成するため。割り戻さないと二重に掛かって暗くなる)
	float3 straight = c.rgb / max(c.a, 1e-4f);

	// 内側は煙色、外周リングは輪郭色
	float3 rgb = lerp(straight, g_OutlineColor.rgb, ring);
	// アルファ：内側は煙のアルファ、外周リングは輪郭の不透明度
	float  a   = max(c.a, ring * g_OutlineColor.a);

	return float4(rgb, a);
}
