#include "../inc_KdCommon.hlsli"
#include "inc_KdPostProcessShader.hlsli"

// 画面エッジ検出によるアウトライン（深度＋深度から復元した法線）。
//  - 深度の段差＝シルエット（背景や奥のオブジェクトとの境界）に線を出す。
//  - 法線の食い違い＝面の折れ（角）に線を出す。
//  - 面一に連続した面（隣接ボックスの継ぎ目など）は段差も法線差も無いので線が出ない。
Texture2D g_colorTex : register(t0);   // 合成済みシーン色
Texture2D g_depthTex : register(t1);   // 深度バッファ
SamplerState g_ss    : register(s0);

cbuffer cb : register(b0)
{
	float2 g_TexelSize;       // 1/幅, 1/高さ
	float  g_Thickness;       // サンプル間隔（テクセル）＝線の太さ
	float  g_DepthThreshold;  // 深度エッジのしきい値

	float  g_NormalThreshold; // 法線エッジのしきい値
	float  g_EdgeStrength;    // 線の濃さ
	float2 g_olPad;

	float4 g_OutlineColor;    // 線の色（rgb）
};

// UV と深度からビュー空間座標を復元（DoF と同じ方式）
float3 ViewPos(float2 uv)
{
	float d = g_depthTex.Sample(g_ss, uv).r;
	float2 ndc = (uv * 2.0 - 1.0) * float2(1, -1);
	float4 p = mul(float4(ndc, d, 1), g_mProjInv);
	return p.xyz / p.w;
}

float4 main(VSOutput In) : SV_Target0
{
	const float2 uv = In.UV;
	float3 col = g_colorTex.Sample(g_ss, uv).rgb;

	const float2 o = g_TexelSize * g_Thickness;

	const float3 P  = ViewPos(uv);
	const float3 Pr = ViewPos(uv + float2( o.x, 0));
	const float3 Pl = ViewPos(uv + float2(-o.x, 0));
	const float3 Pd = ViewPos(uv + float2(0,  o.y));
	const float3 Pu = ViewPos(uv + float2(0, -o.y));

	// ── 深度エッジ：2階差分（平面では0、段差/折れで反応）→ 距離で正規化 ──
	float depthLap  = abs(Pr.z + Pl.z - 2.0 * P.z) + abs(Pu.z + Pd.z - 2.0 * P.z);
	float depthEdge = depthLap / max(abs(P.z), 1.0);
	float dE = step(g_DepthThreshold, depthEdge);

	// ── 法線エッジ：前方差分と後方差分で復元した法線の食い違い ──
	//   平面（面一）では一致＝0、角やシルエットで食い違う＝線。
	float3 nF = normalize(cross(Pr - P, Pd - P));
	float3 nB = normalize(cross(P - Pl, P - Pu));
	float  normalEdge = 1.0 - saturate(dot(nF, nB));
	float  nE = step(g_NormalThreshold, normalEdge);

	float edge = saturate(max(dE, nE)) * g_EdgeStrength;

	float3 outCol = lerp(col, g_OutlineColor.rgb, edge);
	return float4(outCol, 1);
}
