#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// Outline vertex shader (Genshin-style inverted hull)
//  - same skinning / world transform as the Lit VS
//  - extrude along the normal in VIEW space, scaled by view depth
//    so the outline thickness stays constant on screen (distance compensation)
//  - rendered with front-face culling so only the back-faces form the silhouette
//================================

// Outline parameters (VS slot b4)
cbuffer cbOutline : register(b4)
{
	float  g_OutlineWidth;     // world-space extrude width
	float  g_OutlineDepthBias; // push outline slightly toward far (NDC) to hide seams
	float2 g_olPad;
};

VSOutputNoLighting main(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float4 color : COLOR,
	float3 normal : NORMAL0,
	float3 tangent : TANGENT,
	uint4 skinIndex : SKININDEX,
	float4 skinWeight : SKINWEIGHT,
	float3 smoothNormal : NORMAL1	// 位置で溶接した平均法線（板割れ防止）
)
{
	// skinning (must match the Lit VS so the outline follows the animation)
	if (g_IsSkinMeshObj)
	{
		row_major float4x4 mBones = 0;
		[unroll]
		for (int i = 0; i < 4; i++)
		{
			mBones += g_mBones[skinIndex[i]] * skinWeight[i];
		}
		pos          = mul(pos, mBones);
		smoothNormal = mul(smoothNormal, (float3x3) mBones);
	}

	// local -> world （押し出しにはスムース法線を使う）
	float4 wpos = mul(pos, g_mWorld);
	float3 wN   = normalize(mul(smoothNormal, (float3x3) g_mWorld));

	// world -> view, then extrude along the view-space normal by a constant
	// WORLD width. With perspective this makes the outline thinner on screen
	// when far away and thicker when near (natural look; avoids the huge
	// outline on distant, small characters).
	float4 vpos = mul(wpos, g_mView);
	float3 vN   = normalize(mul(wN, (float3x3) g_mView));
	vpos.xyz += vN * g_OutlineWidth;

	VSOutputNoLighting Out;
	Out.Pos   = mul(vpos, g_mProj);
	// 深度を少し奥へ（NDC）。隣接する同じくらいの深さの面には隠れて継ぎ目に線が出ない。
	Out.Pos.z += g_OutlineDepthBias * Out.Pos.w;
	Out.wPos  = wpos.xyz;
	Out.UV    = uv * g_UVTiling + g_UVOffset;
	Out.Color = color;
	return Out;
}
