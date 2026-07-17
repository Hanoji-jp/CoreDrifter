#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

// テクスチャ
Texture2D g_tex : register(t0);
Texture2D g_emissiveTex : register(t2); // 発光テクスチャ

Texture2D g_dissolveTex : register(t11); // ディゾルブマップ

// サンプラ
SamplerState g_ss : register(s0);

float4 main(VSOutputNoLighting In) : SV_Target0
{
	// ディゾルブによる描画スキップ
	float discardValue = g_dissolveTex.Sample(g_ss, In.UV).r;
	if (discardValue < g_dissolveValue)
	{
		discard;
	}

	float4 baseColor = g_tex.Sample(g_ss, In.UV) * In.Color * g_BaseColor;
	float3 outColor = baseColor.rgb;
	
	// Alphaテスト
	if (baseColor.a < 0.05f)
	{
		discard;
	}
	
	// 自己発光色の適応
	if (g_OnlyEmissie)
	{
		outColor = g_emissiveTex.Sample(g_ss, In.UV).rgb * g_Emissive * In.Color.rgb;		
	}
	else
	{
		outColor += g_emissiveTex.Sample(g_ss, In.UV).rgb * g_Emissive * In.Color.rgb;
	}
	
	//------------------------------------------
	// スモーク専用ライティング
	//   板ポリのタイル内ローカルUVから球ドーム(半球)法線を解析復元し、
	//   平行光でトゥーン陰影を掛ける。板なのに丸い塊(モデル)に見せる。
	//   影・PBR・反射は行わない激軽版(大量の煙でも耐える)。
	//------------------------------------------
	if (g_SmokeLit)
	{
		// アトラスのタイル内ローカルUV(0〜1)を復元
		float2 luv = frac(In.UV * float2(g_SmokeSplitX, g_SmokeSplitY));

		// 中心を原点に、円内を半球ドームとして法線を作る
		float nx = luv.x * 2.0f - 1.0f;
		float ny = -(luv.y * 2.0f - 1.0f);
		float r2 = nx * nx + ny * ny;
		float nz = sqrt(saturate(1.0f - r2));

		// ビルボード基底(ワールド行列の各行=camR/camU/camF)から法線をワールドへ
		float3 camR = normalize(float3(g_mWorld._11, g_mWorld._12, g_mWorld._13));
		float3 camU = normalize(float3(g_mWorld._21, g_mWorld._22, g_mWorld._23));
		float3 camF = normalize(float3(g_mWorld._31, g_mWorld._32, g_mWorld._33));
		float3 N = normalize(camR * nx + camU * ny + camF * nz);

		// 平行光でトゥーン陰影(3段)。影側は0にせず持ち上げて黒潰れ防止。低コントラスト。
		float ndl   = saturate(dot(N, normalize(-g_DL_Dir)));
		float band  = floor(ndl * 3.0f + 0.5f) / 3.0f;         // 3段に量子化
		float shade = lerp(0.70f, 1.08f, band);                // 影0.70〜光1.08(控えめ)

		// 縁(nz→0)では陰影を弱め、粒どうしの重なり(つなぎ目)のコントラストを消す。
		// 中心(nz→1)だけ立体感を残し、境界はニュートラルに繋げて見せる。
		shade = lerp(1.0f, shade, nz);

		// 平行光の色味も軽く乗せる(白飛び防止のため控えめ)
		float3 lit = shade * lerp(float3(1.0f, 1.0f, 1.0f), g_DL_Color, 0.25f);
		outColor *= lit;

		//--- ディゾルブ(穴あき) ＋ 不透明度フェード ---
		// presence(In.Color.a=生存率0〜1)。
		//  ① ディゾルブ：消えるほど閾値を上げ、雲アルファ(texA)の薄い所から穴が開いて崩れる。
		//  ② 不透明度フェード：全体を presence で線形に薄くする＝滑らかに消える(ポップ防止)。
		float texA     = g_tex.Sample(g_ss, In.UV).a;
		float presence = In.Color.a;

		float thr      = saturate(1.0f - presence) * g_SmokeErode;   // ディゾルブ閾値
		float dissolve = smoothstep(thr, thr + g_SmokeEdge, texA);   // 穴あきマスク
		baseColor.a    = dissolve * g_SmokePeak * presence;          // ×presenceで不透明度フェード
	}

	// 全体の明度：環境光に1が設定されている場合は影響なし
	// 環境光の不透明度を下げる事により、明度ライトの周り以外は描画されなくなる
	float totalBrightness = g_AmbientLight.a;

	if (totalBrightness < 1.0f)
	{
		//-------------------------
		// 全体の明度への点光源影響
		//-------------------------
		for (int i = 0; i < g_PointLightNum.x; i++)
		{
		// ピクセルから点光への方向
			float3 dir = g_PointLights[i].Pos - In.wPos;
		
		// 距離を算出
			float dist = length(dir);
		
		// 正規化
			dir /= dist;
		
		// 点光の判定以内
			if (dist < g_PointLights[i].Radius)
			{
			// 半径をもとに、距離の比率を求める
				float atte = 1.0 - saturate(dist / g_PointLights[i].Radius);
			
			// 明度の追加
				totalBrightness += (1 - pow(1 - atte, 2)) * g_PointLights[i].IsBright;
			}
		}
	}
	
	//------------------------------------------
	// 高さフォグ
	//------------------------------------------
	if (g_HeightFogEnable && g_FogEnable)
	{
		if (In.wPos.y < g_HeightFogTopValue)
		{
			float distRate = length(In.wPos - g_CamPos);
			distRate = saturate(distRate / g_HeightFogDistance);
			distRate = pow(distRate, 2.0);
			
			float heightRange = g_HeightFogTopValue - g_HeightFogBottomValue;
			float heightRate = 1 - saturate((In.wPos.y - g_HeightFogBottomValue) / heightRange);
			
			float fogRate = heightRate * distRate;
			outColor = lerp(outColor, g_HeightFogColor, fogRate);
		}
	}
	
	//------------------------------------------
	// 距離フォグ
	//------------------------------------------
	if (g_DistanceFogEnable && g_FogEnable)
	{
		float3 vCam = g_CamPos - In.wPos;
		float camDist = length(vCam); // カメラ - ピクセル距離
		
		// フォグ 1(近い)～0(遠い)
		float f = saturate(1.0 / exp(camDist * g_DistanceFogDensity));
		// 適用
		outColor = lerp(g_DistanceFogColor, outColor, f);
	}
	
	// ディゾルブ輪郭発光
	if (g_dissolveValue > 0)
	{
		// 閾値とマスク値の差分で、縁を検出
		if (abs(discardValue - g_dissolveValue) < g_dissolveEdgeRange)
		{
			// 輪郭に発光色追加
			outColor.rgb += g_dissolveEmissive;
		}
	}
	
	totalBrightness = saturate(totalBrightness);
	outColor *= totalBrightness;
	
	return float4(outColor, baseColor.a);
}
