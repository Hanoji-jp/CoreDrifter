#include "inc_KdStandardShader.hlsli"

Texture2D g_tex : register(t0);

Texture2D g_dissolveTex : register(t11); // ディゾルブマップ

SamplerState g_ss : register(s0);

float4 main(VSOutputGenShadow  In) : SV_TARGET
{ 
	// ディゾルブによる描画スキップ
	float discardValue = g_dissolveTex.Sample(g_ss, In.UV).r;
	if (discardValue < g_dissolveValue)
	{
		discard;
	}

	// Alphaテスト：Alpha値が一定以下のピクセルは描画処理を飛ばす
	float4 texCol = g_tex.Sample(g_ss, In.UV);

	// 塗り分けのときは頂点色を透明度として読まない。
	//
	// 頂点色は4層の重み(R=岩 G=土 B=草 A=舗装)なので、
	// 舗装でない所は A=0 になる。そのまま透明度として読むと、
	// 地面が影を落とさなくなる
	const float vtxAlpha = (g_SplatEnable > 0.5f) ? 1.0f : In.Color.a;

	if (texCol.a * vtxAlpha < 0.05f)
	{
		discard;
	}

	return float4(In.pPos.z / In.pPos.w, 0.0f, 0.0f, 1.0f);
}
