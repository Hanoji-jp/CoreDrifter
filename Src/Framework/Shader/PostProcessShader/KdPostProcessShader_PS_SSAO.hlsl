#include "../inc_KdCommon.hlsli"
#include "inc_KdPostProcessShader.hlsli"

// 画面空間の環境遮蔽（SSAO）。
//
// ■ 何をしているか
//   その画素の周りに点を撒いて、「自分より手前に別の面があるか」を数える。
//   多いほどそこは何かに囲まれている＝光が回り込まない＝暗い。
//
//   タイヤと地面の間、ドアの隙間、バンパーの奥。
//   そういう「物と物が近い所」に影が落ちる。
//
// ■ なぜ要るか
//   平行光の影は形の大きな面にしか落ちない。
//   物が触れている所の陰は、光源の影では出せない。
//   これが無いと、車が地面に置かれているのではなく
//   浮いた絵を重ねたように見える。
//
// ■ 法線は深度から作る
//   法線バッファを別に持たないので、隣の画素との位置の差から面の向きを出す。
//   輪郭線の処理と同じやり方。
Texture2D g_colorTex : register(t0);   // 合成済みシーン色
Texture2D g_depthTex : register(t1);   // 深度バッファ
SamplerState g_ss    : register(s0);

cbuffer cb : register(b0)
{
	float2 g_TexelSize;   // 1/幅, 1/高さ
	float  g_Radius;      // 見る範囲(ビュー空間の距離)
	float  g_Bias;        // 自己遮蔽よけ。面のざらつきを拾わないための下駄

	float  g_Strength;    // 効き具合
	float  g_MaxDist;     // これより遠い面は無視する。遠景が手前を暗くしないため
	float2 g_aoPad;
};

// UV と深度からビュー空間座標を復元
float3 ViewPos(float2 uv)
{
	float d = g_depthTex.Sample(g_ss, uv).r;
	float2 ndc = (uv * 2.0 - 1.0) * float2(1, -1);
	float4 p = mul(float4(ndc, d, 1), g_mProjInv);
	return p.xyz / p.w;
}

// 画素ごとにばらつく値。
//
// 同じ向きに点を撒くと、遮蔽の縞が画面に出る。
// 画素ごとに回して散らすと、縞が砂目に変わって目立たない
float Hash(float2 p)
{
	return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// 半球へ撒く点。円盤状に並べて、法線側へ持ち上げる
static const float2 k_Disk[12] =
{
	float2( 0.000,  1.000), float2( 0.866,  0.500),
	float2( 0.866, -0.500), float2( 0.000, -1.000),
	float2(-0.866, -0.500), float2(-0.866,  0.500),
	float2( 0.500,  0.400), float2( 0.400, -0.500),
	float2(-0.500, -0.400), float2(-0.400,  0.500),
	float2( 0.700, -0.100), float2(-0.700,  0.100),
};

float4 main(VSOutput In) : SV_Target0
{
	const float2 uv = In.UV;

	float3 col = g_colorTex.Sample(g_ss, uv).rgb;

	// 空など、何も書かれていない所は遮蔽を出さない
	float depth = g_depthTex.Sample(g_ss, uv).r;
	if (depth >= 0.9999) { return float4(col, 1); }

	float3 P = ViewPos(uv);

	// 法線を隣の画素から作る。
	// 面の傾きが分かればよいので、精度は要らない
	float3 dx = ddx(P);
	float3 dy = ddy(P);
	float3 N  = normalize(cross(dx, dy));

	// 画素ごとに点の撒き方を回す
	float  ang = Hash(uv * 1024.0) * 6.2831853;
	float  ca  = cos(ang);
	float  sa  = sin(ang);

	// ビュー空間の半径を画面上の大きさへ直す。
	// 奥の物ほど画面では小さくなるので、割る
	float px = g_Radius / max(-P.z, 0.001);

	float occ = 0.0;

	[unroll]
	for (int i = 0; i < 12; ++i)
	{
		float2 d = k_Disk[i];

		// 回す
		float2 r = float2(d.x * ca - d.y * sa, d.x * sa + d.y * ca);

		float2 suv = uv + r * px * g_TexelSize * 512.0;

		float3 S = ViewPos(suv);

		// 自分から見たその点の向き
		float3 v = S - P;

		float len = length(v);
		if (len < 0.0001) { continue; }

		// 面の裏側にある点は数えない。
		// 数えると、平らな面でも半分が遮蔽になって全体が暗くなる
		float ndv = dot(N, v / len);

		// 遠すぎる点は無視。
		// 入れると、背景が手前の物の縁を黒くする(縁が汚れる)
		float fall = saturate(1.0 - len / g_MaxDist);

		occ += saturate(ndv - g_Bias) * fall;
	}

	occ = saturate(occ / 12.0 * g_Strength);

	return float4(col * (1.0 - occ), 1);
}
