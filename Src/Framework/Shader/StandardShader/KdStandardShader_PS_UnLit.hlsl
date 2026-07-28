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
		// ディゾルブ：消え際に穴が広がって崩れる。
		// ワールド座標をセルに区切って乱数を振り、進行度より小さいセルを消していく。
		// セル単位なので"粒が砕けて散る"見え方になる(滑らかに薄まるのとは別物)。
		// 進行度は粒ごとに違うので、頂点のUV.xに入れて運ぶ。
		// (全粒を1回のドローにまとめているため、定数バッファでは粒ごとに変えられない)
		float dissolve = In.UV.x;
		if (dissolve > 0.0f)
		{
			float3 cell = floor(In.wPos * g_SmokeDissolveScale);
			float  rnd  = frac(sin(dot(cell, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
			if (rnd < dissolve) { discard; }
		}

		// メッシュ煙のトゥーン陰影。
		// 頂点カラーのRGBには「ワールド空間の法線」が入っている(CPU側で変換して格納)。
		// 粒ごとの回転・非一様スケールはCPU側で解決済みなので、ここでは復元するだけでよい。
		//   ※CPUで陰影を焼くとシーンの光(向き・色)の変化に追従できない
		//   ※CPUで段階化すると頂点間補間でなまされ、段差が消える
		float3 N = normalize(In.Color.rgb * 2.0f - 1.0f);

		// 光の向き。シーンの平行光を「真上から」へ寄せる。
		// 本家の煙は上面が明るく下面が暗い＝ほぼ真上からの光として描かれている。
		// 平行光そのままだと横向きの面が明暗どちらにもなり、ハイライトが散らばって汚くなる。
		float3 Ldir = normalize(lerp(normalize(g_DL_Dir), float3(0.0f, -1.0f, 0.0f), g_ToonUpBias));

		// カメラ基準の光。ビュー行列からカメラの世界軸を取り出し、常に「画面の左上手前」から
		// 当たる光を作る。これを混ぜると、カメラを回したときにハイライトの位置も一緒に回る
		// (イラストで常に決まった方向から光を描くのと同じ。本家の煙はこの見え方)。
		if (g_SmokeViewLight > 0.001f)
		{
			float3 camR = normalize(float3(g_mView._11, g_mView._21, g_mView._31)); // 右
			float3 camU = normalize(float3(g_mView._12, g_mView._22, g_mView._32)); // 上
			float3 camF = normalize(float3(g_mView._13, g_mView._23, g_mView._33)); // 前
			// 光源は左上手前 → 光の進む向きはその逆
			float3 viewL = normalize(-(-camR * 0.45f + camU * 0.80f - camF * 0.40f));
			Ldir = normalize(lerp(Ldir, viewL, g_SmokeViewLight));
		}

		float  nTerm = saturate(dot(N, -Ldir));   // 粒ごとの形から出る明るさ

		// 2つ目の光(横上から)。キーライトと反対側の斜め上に置いて、横向きの面にも
		// ハイライトを乗せる。1灯だと片側しか光らず、本家のように面が光らない。
		// 強い方を採用＝両方の光が別々にハイライトを作る(足すと白飛びする)。
		if (g_SmokeFillLight > 0.001f)
		{
			float3 fR = normalize(float3(g_mView._11, g_mView._21, g_mView._31));
			float3 fU = normalize(float3(g_mView._12, g_mView._22, g_mView._32));
			float3 fF = normalize(float3(g_mView._13, g_mView._23, g_mView._33));
			// 右の横上手前から差す(キーライトは左上なので反対側)
			float3 fillL = normalize(-(fR * 0.80f + fU * 0.55f - fF * 0.25f));
			float  fillN = saturate(dot(N, -fillL)) * g_SmokeFillLight;
			nTerm = max(nTerm, fillN);
		}

		// 全粒で共有する明るさ：煙の根元からの高さ。
		// これを混ぜると、重なった粒どうしが同じ高さで同じ色になり1つの塊として繋がる。
		// (粒ごとの法線だけだと、重なるほど明るい頭が並んでボコボコに見える)
		// 境界を水平一直線にしないためのうねり。ワールドXZだけで決まる＝全粒で共有されるので、
		// 一体感は保ったまま、明暗の境目だけが波打つ。
		float wob = (sin(In.wPos.x * g_SmokeWobFreq) * cos(In.wPos.z * g_SmokeWobFreq * 1.37f)
		           + sin(In.wPos.z * g_SmokeWobFreq * 0.61f) * 0.5f) * g_SmokeWobAmp;

		float hTerm = saturate((In.wPos.y + wob - g_SmokeBaseY) / max(g_SmokePlumeH, 0.01f));
		// 根元でも0まで落とさない。0にすると、出たばかりの低い煙は受光量の上限が
		// (1-merge)までしか上がらず、ハイライトのしきい値に届かなくなる。
		hTerm = lerp(0.55f, 1.0f, hTerm);

		// 粒ローカルの高さ：その粒の中心を基準にした上下位置。
		// これで陰影を決めると、粒ごとに水平な明暗の切れ目が入る(手描きのセル画っぽい見え方)。
		// 法線ベースだと境目が表面の曲率に沿って曲がるので、質感がだいぶ変わる。
		float scaleY = length(float3(g_mWorld._21, g_mWorld._22, g_mWorld._23));
		float lTerm  = saturate((In.wPos.y - g_mWorld._42) / max(scaleY, 0.001f) * 0.5f + 0.5f);

		// 粒ごとの陰影(法線 or ローカル高さ) → さらに全粒共有の高さを混ぜて一体化
		float pTerm = lerp(nTerm, lTerm, g_SmokeLocalY);
		float ndl   = lerp(pTerm, hTerm, g_SmokeMerge);

		// 後方グラデーション：発生源から離れるほど色Bへ寄せる。
		// ワールド位置だけで決まるので、どの粒に属するピクセルでも同じ位置なら同じ色＝
		// 粒の境目が出ずに滑らかに繋がる(粒ごとに色を変えるとパッチワークになる)。
		float2 fromOrigin = In.wPos.xz - float2(g_SmokeOriginX, g_SmokeOriginZ);
		float  gradT = saturate(length(fromOrigin) / max(g_SmokeGradDist, 0.01f));
		float3 farCol = float3(g_SmokeColorBR, g_SmokeColorBG, g_SmokeColorBB);

		// 基準色（粒の色に平行光の色を乗せたもの）
		float3 baseTint = lerp(g_BaseColor.rgb, farCol, gradT) * g_DL_Color;

		// 3段に塗り分ける
		//   ① 一番光が当たる面 … 白へ寄せたハイライト
		//   ② 中間            … 基準色そのもの
		//   ③ 光が当たらない面 … 暗い色（黒潰れではなく環境光の色を残す）
		// ハイライトは指定色へ寄せる(既定は白。好きな色にできる)
		float3 hiCol = float3(g_SmokeHiR, g_SmokeHiG, g_SmokeHiB);
		float3 hi   = lerp(baseTint, hiCol, g_ToonWhite);
		float3 mid  = baseTint;
		float3 dark = baseTint * g_ToonDark
		            * lerp(float3(1.0f, 1.0f, 1.0f), g_AmbientLight.rgb, 0.7f);

		// ハイライトの判定だけは高さを混ぜる前の値(pTerm)で行う。
		// 混ぜた値(ndl)で判定すると、高さ項が低い根元では受光量の上限が
		//   (1-merge) + merge*高さ項の下限
		// までしか上がらず、面が光に正対してもハイライトに届かない領域ができてしまう。
		// (カメラを回して低い煙が主に見えると、ハイライトが全部消えたように見える)
		float3 col = dark;
		float  toneLevel = 0.0f;                              // 0=暗 0.5=標準 1=ハイライト
		if (pTerm >= g_ToonHiThr)     { col = hi;  toneLevel = 1.0f; }
		else if (ndl >= g_ToonMidThr) { col = mid; toneLevel = 0.5f; }

		// スクリーン空間のハーフトーン模様（印刷物っぽい質感）。
		// In.Pos.xy はピクセル座標なので、模様は画面に貼り付いたまま＝煙が動いても模様は動かない。
		if (g_SmokePatStrength > 0.001f)
		{
			// 網点は45°に傾けるのが印刷の定石(格子が目立ちにくい)
			float2 sp = float2(In.Pos.x * 0.7071f - In.Pos.y * 0.7071f,
			                   In.Pos.x * 0.7071f + In.Pos.y * 0.7071f);
			sp /= max(g_SmokePatScale, 1.0f);

			float2 c = frac(sp) - 0.5f;
			float  d = length(c) * 2.0f;                      // 0(セル中心)〜1
			float  dotMask = smoothstep(0.95f, 0.30f, d);     // 中心ほど1

			// 暗い面ほど網点を強く出す(明るい面は白く飛ばす＝印刷の階調表現)
			float amt = g_SmokePatStrength * lerp(1.0f, 1.0f - toneLevel, g_SmokePatDarkBias);
			col *= 1.0f - amt * (1.0f - dotMask);
		}

		outColor    = col;
		// 粒ごとの濃さは頂点カラーのアルファで運ぶ(1ドローにまとめている都合)
		baseColor.a = g_BaseColor.a * In.Color.a;
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
