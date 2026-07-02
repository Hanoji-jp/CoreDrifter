#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

//================================
// Outline pixel shader
//  - outputs a flat, darkened color for the silhouette.
//  - g_BaseColor already = material base color * colRate, so passing a
//    dark colRate from C++ (e.g. 0.15) gives a darkened version of the
//    object's own color (Genshin-style tinted outline), not pure black.
//================================
float4 main(VSOutputNoLighting In) : SV_Target
{
	float4 col = g_BaseColor;
	col.a = 1.0;
	return col;
}
