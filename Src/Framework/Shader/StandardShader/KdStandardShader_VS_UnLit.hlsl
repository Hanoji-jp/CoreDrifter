#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// 頂点シェーダ：半透明
//================================
VSOutputNoLighting main(
	float4 pos : POSITION,		// 頂点座標
	float2 uv : TEXCOORD0,		// テクスチャUV座標
	float4 color : COLOR,		// 頂点カラー
	float3 normal : NORMAL,		// 法線
	float3 tangent : TANGENT,	// 接線
	uint4 skinIndex : SKININDEX,	// スキンメッシュのボーンインデックス
	float4 skinWeight : SKINWEIGHT	// ボーンの影響度
)
{
	VSOutputNoLighting Out;

	// スキニング（Lit と同じ。これが無いと発光が本体のモーションとズレる）
	if (g_IsSkinMeshObj)
	{
		row_major float4x4 mBones = 0;
		[unroll]
		for (int i = 0; i < 4; i++)
		{
			mBones += g_mBones[skinIndex[i]] * skinWeight[i];
		}
		pos = mul(pos, mBones);
		normal = mul(normal, (float3x3) mBones);
	}

	// 座標変換
	Out.Pos = mul(pos, g_mWorld);		// ローカル座標系 -> ワールド座標系へ変換
	Out.wPos = Out.Pos.xyz;				// ワールド座標を別途保存
	Out.Pos = mul(Out.Pos, g_mView);	// ワールド座標系 -> ビュー座標系へ変換
	Out.Pos = mul(Out.Pos, g_mProj);	// ビュー座標系 -> 射影座標系へ変換

	// UV座標
	Out.UV = uv * g_UVTiling + g_UVOffset;

	// 色
	Out.Color = color;

	// 出力
	return Out;
}
