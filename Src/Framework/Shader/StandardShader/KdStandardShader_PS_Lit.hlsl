#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

// モデル描画用テクスチャ
Texture2D g_baseTex       : register(t0);   // ベースカラーテクスチャ
Texture2D g_metalRoughTex : register(t1);   // メタリック(b)/ラフネス(g)テクスチャ
Texture2D g_emissiveTex   : register(t2);   // 発光テクスチャ
Texture2D g_normalTex     : register(t3);   // 法線マップ
Texture2D g_grassTex       : register(t4);   // 芝生テクスチャ（トリプレーナー）
Texture2D g_grassNormalTex : register(t5);   // 芝生法線マップ（盛り上がり表現）
Texture2D g_grassEdgeTex   : register(t6);   // 草エッジ（境目）テクスチャ

// 特殊処理用テクスチャ
Texture2D g_dirShadowMap  : register(t10);  // 平行光シャドウマップ
Texture2D g_dissolveTex   : register(t11);  // ディゾルブマップ
Texture2D g_environmentTex: register(t12);  // 反射景マップ

// サンプラ
SamplerState           g_ss    : register(s0); // 通常テクスチャ用
SamplerComparisonState g_ssCmp : register(s1); // 比較サンプラ（シャドウ用）

//=============================================================
// 定数
//=============================================================
static const float PI = 3.14159265358979f;

//-------------------------------------------------------------
// 宇宙空間の見た目強化パラメータ
//-------------------------------------------------------------
// リムライト（縁光）：シルエットを光で縁取り、宇宙の浮遊感を出す
static const float k_RimPower    = 3.0f;   // 縁の鋭さ（大きいほど縁だけ光る）
static const float k_RimStrength = 0.7f;   // 縁光の強さ
// 擬似環境反射（スペキュラIBL近似）：滑らかな面に宇宙が映り込む
static const float k_EnvReflectUpMul   = 4.0f;  // 上方向（星空側）の反射の明るさ倍率
static const float k_EnvReflectDownMul = 0.8f;  // 下方向（暗い宇宙）の反射の明るさ倍率

//=============================================================
// トーンマッピング / 色変換
//=============================================================

// セル(トゥーン)ランプ：明るさ t を数段に量子化する。境界はわずかに滑らかにしてジャギを抑える。
// シーン全体をアニメ調のフラットな陰影にして、暗い輪郭を際立たせる用途。
float ToonRamp(float t)
{
	t = saturate(t);
	// しきい値（暗→明）と各段の明るさ（3段：影 / 中 / 光）
	const float t0 = 0.25f;   // 影→中 の境界
	const float t1 = 0.60f;   // 中→光 の境界
	const float lvShadow = 0.32f;
	const float lvMid    = 0.66f;
	const float lvLit    = 1.00f;
	const float aa = 0.03f;   // 境界の柔らかさ

	float s0 = smoothstep(t0 - aa, t0 + aa, t);
	float s1 = smoothstep(t1 - aa, t1 + aa, t);
	float lv = lvShadow;
	lv = lerp(lv, lvMid, s0);
	lv = lerp(lv, lvLit, s1);
	return lv;
}

// ACES フィルミックトーンマッピング近似（HDR → LDR、FORZA等の映える階調）
float3 ACESFilm(float3 x)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

//=============================================================
// Poisson Disk サンプル点（16点）
//=============================================================
static const float2 g_PoissonDisk[16] =
{
	float2(-0.94201624f, -0.39906216f),
	float2( 0.94558609f, -0.76890725f),
	float2(-0.09418410f, -0.92938870f),
	float2( 0.34495938f,  0.29387760f),
	float2(-0.91588581f,  0.45771432f),
	float2(-0.81544232f, -0.87912464f),
	float2(-0.38277543f,  0.27676845f),
	float2( 0.97484398f,  0.75648379f),
	float2( 0.44323325f, -0.97511554f),
	float2( 0.53742981f, -0.47373420f),
	float2(-0.26496911f, -0.41893023f),
	float2( 0.79197514f,  0.19090188f),
	float2(-0.24188840f,  0.99706507f),
	float2(-0.81409955f,  0.91437590f),
	float2( 0.19984126f,  0.78641367f),
	float2( 0.14383161f, -0.14100790f),
};

//=============================================================
// PBR ヘルパー関数 (Cook-Torrance GGX)
//=============================================================

// GGX 法線分布関数 (NDF)
float D_GGX(float NdotH, float roughness)
{
	float a  = roughness * roughness;
	float a2 = a * a;
	float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
	return a2 / (PI * d * d + 1e-7f);
}

// Smith GGX 幾何減衰 (Schlick 近似)
float G_SmithGGX(float NdotV, float NdotL, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;
	float gv = NdotV / (NdotV * (1.0f - k) + k + 1e-7f);
	float gl = NdotL / (NdotL * (1.0f - k) + k + 1e-7f);
	return gv * gl;
}

// Schlick フレネル近似
float3 F_Schlick(float VdotH, float3 F0)
{
	return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

// Cook-Torrance BRDF で1方向ライトの輝度を返す
// lightDir : 光源への方向（正規化済み）
// viewDir  : カメラへの方向（正規化済み）
// N        : ワールド法線（正規化済み）
// albedo   : 拡散ベースカラー
// F0       : フレネル反射率（金属なら baseColor、非金属なら 0.04）
// roughness, metallic
float3 CookTorrance(float3 lightDir, float3 viewDir, float3 N,
					float3 albedo, float3 F0, float roughness, float metallic,
					float3 lightColor, float attenuation)
{
	float3 H     = normalize(viewDir + lightDir);
	float NdotL  = saturate(dot(N, lightDir));
	float NdotV  = saturate(dot(N, viewDir));
	float NdotH  = saturate(dot(N, H));
	float VdotH  = saturate(dot(viewDir, H));

	// Specular BRDF
	float  D  = D_GGX(NdotH, roughness);
	float  G  = G_SmithGGX(NdotV, NdotL, roughness);
	float3 F  = F_Schlick(VdotH, F0);
	float3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 1e-7f);

	// Diffuse: 金属は拡散なし
	float3 kD = (1.0f - F) * (1.0f - metallic);
	float3 diffuse = kD * albedo / PI;

	return (diffuse + specular) * lightColor * NdotL * attenuation;
}

//=============================================================
// PCSS ソフトシャドウ
//  1) ブロッカー検索で平均遮蔽深度を求める
//  2) 遮蔽物との距離から半影(ペナンブラ)サイズを推定
//  3) 推定半径で PCF フィルタ（接地は鋭く・遠方は柔らかく）
//=============================================================

// 光源の見かけサイズ（大きいほど影の縁がボケる）
static const float k_LightSize     = 5.0f;
// ブロッカー検索半径（テクセル単位）
static const float k_BlockerSearch = 4.0f;
// 半影の最小/最大半径（テクセル単位）
static const float k_MinPenumbra   = 1.0f;
static const float k_MaxPenumbra   = 12.0f;

float CalcShadow(float3 wPos, float3 wN)
{
	// 法線オフセットバイアス（シャドウアクネ対策）
	float3 offsetPos = wPos + wN * 0.05f;
	float4 liPos = mul(float4(offsetPos, 1.0f), g_DL_mLightVP);
	liPos.xyz /= liPos.w;

	// シャドウマップ範囲外は影なし
	if (abs(liPos.x) > 1.0f || abs(liPos.y) > 1.0f || liPos.z > 1.0f)
		return 1.0f;

	float2 uv = liPos.xy * float2(1.0f, -1.0f) * 0.5f + 0.5f;
	float  z  = liPos.z;

	float w, h;
	g_dirShadowMap.GetDimensions(w, h);
	float2 texelSize = float2(1.0f / w, 1.0f / h);

	// 深度バイアス（自己遮蔽防止）
	static const float k_DepthBias = 0.0015f;
	float zBiased = z - k_DepthBias;

	//------------------------------------------
	// 1) ブロッカー検索：自分より手前にある遮蔽物の平均深度
	//------------------------------------------
	float blockerSum   = 0.0f;
	float blockerCount = 0.0f;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 offset = g_PoissonDisk[i] * texelSize * k_BlockerSearch;
		float  sampleDepth = g_dirShadowMap.Sample(g_ss, uv + offset).r;
		if (sampleDepth < zBiased)
		{
			blockerSum   += sampleDepth;
			blockerCount += 1.0f;
		}
	}

	// 遮蔽物がなければ完全に明るい
	if (blockerCount < 0.5f)
		return 1.0f;

	float avgBlocker = blockerSum / blockerCount;

	//------------------------------------------
	// 2) 半影サイズの推定（接地ほど小さく、離れるほど大きく）
	//------------------------------------------
	float penumbra = (z - avgBlocker) / max(avgBlocker, 1e-4f) * k_LightSize;
	float radius   = clamp(penumbra * (w / 2048.0f) + k_MinPenumbra,
						   k_MinPenumbra, k_MaxPenumbra);

	//------------------------------------------
	// 3) 推定半径で PCF（比較サンプラ）
	//------------------------------------------
	float shadow = 0.0f;
	[unroll]
	for (int j = 0; j < 16; ++j)
	{
		float2 offset = g_PoissonDisk[j] * texelSize * radius;
		shadow += g_dirShadowMap.SampleCmpLevelZero(g_ssCmp, uv + offset, zBiased);
	}
	return shadow / 16.0f;
}

//=============================================================
// ピクセルシェーダ
//=============================================================
float4 main(VSOutput In) : SV_Target0
{
	//------------------------------------------
	// ディゾルブ
	//------------------------------------------
	float discardValue = g_dissolveTex.Sample(g_ss, In.UV).r;
	if (discardValue < g_dissolveValue)
		discard;

	//------------------------------------------
	// ベースカラー
	//------------------------------------------
	float4 baseColor;
	if (g_UseTriplanar)
	{
		float3 blend = pow(abs(normalize(In.wN)), 4.0f);
		blend /= (blend.x + blend.y + blend.z + 1e-5f);
		float3 scaledPos = In.wPos * g_TriplanarScale;
		float4 cx = g_baseTex.Sample(g_ss, scaledPos.yz);
		float4 cy = g_baseTex.Sample(g_ss, scaledPos.xz);
		float4 cz = g_baseTex.Sample(g_ss, scaledPos.xy);
		baseColor = (cx * blend.x + cy * blend.y + cz * blend.z) * g_BaseColor * In.Color;
	}
	else
	{
		baseColor = g_baseTex.Sample(g_ss, In.UV) * g_BaseColor * In.Color;
	}
	if (baseColor.a < 0.05f)
		discard;

	//------------------------------------------
	// 芝生ブレンド（法線が重力上方向を向いている面に芝テクスチャをブレンド）
	//------------------------------------------
	if (g_UseGrass)
	{
		// ワールド法線と重力上方向のdotで上向き度合いを計算
		float upDot = dot(normalize(In.wN), normalize(g_GravityUpDir));
		// smoothstep でブレンド境界を滑らかに
		float grassBlend = smoothstep(
			1.0f - g_GrassBlendSharpness * 0.5f,
			1.0f,
			pow(saturate(upDot), g_GrassBlendSharpness));

		// 芝テクスチャをトリプレーナーでサンプリング
		float3 blend = pow(abs(normalize(In.wN)), 4.0f);
		blend /= (blend.x + blend.y + blend.z + 1e-5f);
		float3 gScaled = In.wPos * g_GrassTriplanarScale;
		float4 gx = g_grassTex.Sample(g_ss, gScaled.yz);
		float4 gy = g_grassTex.Sample(g_ss, gScaled.xz);
		float4 gz = g_grassTex.Sample(g_ss, gScaled.xy);
		float4 grassColor = gx * blend.x + gy * blend.y + gz * blend.z;

		baseColor.rgb = lerp(baseColor.rgb, grassColor.rgb, grassBlend);
	}

	//------------------------------------------
	// エッジ（境目）テクスチャブレンド
	// upDot が EdgeWidth の帯域（草と側面の境目）に専用テクスチャをオーバーレイ
	//------------------------------------------
	if (g_UseGrassEdge)
	{
		float upDot = dot(normalize(In.wN), normalize(g_GravityUpDir));

		// エッジ帯域：upDot が (grassThreshold - edgeWidth) 〜 grassThreshold の範囲
		// grassThreshold = 草が始まる upDot の値（草ブレンドの開始点と一致させる）
		float grassStart = 1.0f - g_GrassBlendSharpness * 0.5f;
		float edgeCenter = grassStart;								// 帯の中心
		float halfWidth  = g_GrassEdgeWidth * 0.5f;

		// 帯域の中心に向かって強くなる山形のブレンド率
		float edgeBlend = 1.0f - saturate(abs(upDot - edgeCenter) / max(halfWidth, 1e-5f));
		edgeBlend = smoothstep(0.0f, 1.0f, edgeBlend);

		if (edgeBlend > 0.001f)
		{
			// エッジテクスチャをトリプレーナーでサンプリング
			float3 tpBlend  = pow(abs(normalize(In.wN)), 4.0f);
			tpBlend /= (tpBlend.x + tpBlend.y + tpBlend.z + 1e-5f);
			float3 eScaled  = In.wPos * g_GrassEdgeTexScale;
			float4 ex = g_grassEdgeTex.Sample(g_ss, eScaled.yz);
			float4 ey = g_grassEdgeTex.Sample(g_ss, eScaled.xz);
			float4 ez = g_grassEdgeTex.Sample(g_ss, eScaled.xy);
			float4 edgeColor = ex * tpBlend.x + ey * tpBlend.y + ez * tpBlend.z;

			baseColor.rgb = lerp(baseColor.rgb, edgeColor.rgb, edgeBlend);
		}
	}

	//------------------------------------------
	// 全面エッジブレンド（Box本体の全面に薄暗いテクスチャを上書き）
	//------------------------------------------
	if (g_UseGrassEdge && g_FullEdgeStrength > 0.001f)
	{
		float3 tpBlend = pow(abs(normalize(In.wN)), 4.0f);
		tpBlend /= (tpBlend.x + tpBlend.y + tpBlend.z + 1e-5f);
		float3 eScaled = In.wPos * g_GrassEdgeTexScale;
		float4 ex = g_grassEdgeTex.Sample(g_ss, eScaled.yz);
		float4 ey = g_grassEdgeTex.Sample(g_ss, eScaled.xz);
		float4 ez = g_grassEdgeTex.Sample(g_ss, eScaled.xy);
		float4 fullEdgeColor = ex * tpBlend.x + ey * tpBlend.y + ez * tpBlend.z;
		// 暗め補正（薄暗い印象にする）
		fullEdgeColor.rgb *= 0.75f;
		baseColor.rgb = lerp(baseColor.rgb, fullEdgeColor.rgb, g_FullEdgeStrength);
	}

	//------------------------------------------
	float3 vCam    = g_CamPos - In.wPos;
	float  camDist = length(vCam);
	vCam = normalize(vCam);

	//------------------------------------------
	// ワールド法線（法線マップ適用）
	//------------------------------------------
	float3 wN = g_normalTex.Sample(g_ss, In.UV).rgb * 2.0f - 1.0f;
	{
		row_major float3x3 mTBN =
		{
			normalize(In.wT),
			normalize(In.wB),
			normalize(In.wN),
		};
		wN = mul(wN, mTBN);
	}
	wN = normalize(wN);

	//------------------------------------------
	// 芝生法線ブレンド（上向き面だけ草の凹凸法線に差し替えて盛り上がり表現）
	//------------------------------------------
	if (g_UseGrass && g_UseGrassNormal)
	{
		float upDot      = dot(normalize(In.wN), normalize(g_GravityUpDir));
		float grassBlend = smoothstep(
			1.0f - g_GrassBlendSharpness * 0.5f,
			1.0f,
			pow(saturate(upDot), g_GrassBlendSharpness));

		if (grassBlend > 0.001f)
		{
			// 草法線をトリプレーナーでサンプリング
			float3 tpBlend  = pow(abs(normalize(In.wN)), 4.0f);
			tpBlend /= (tpBlend.x + tpBlend.y + tpBlend.z + 1e-5f);
			float3 gScaled  = In.wPos * g_GrassTriplanarScale;
			float3 gnx = g_grassNormalTex.Sample(g_ss, gScaled.yz).rgb * 2.0f - 1.0f;
			float3 gny = g_grassNormalTex.Sample(g_ss, gScaled.xz).rgb * 2.0f - 1.0f;
			float3 gnz = g_grassNormalTex.Sample(g_ss, gScaled.xy).rgb * 2.0f - 1.0f;
			float3 grassN = normalize(gnx * tpBlend.x + gny * tpBlend.y + gnz * tpBlend.z);

			// ワールド空間に変換
			row_major float3x3 mTBN2 =
			{
				normalize(In.wT),
				normalize(In.wB),
				normalize(In.wN),
			};
			grassN = normalize(mul(grassN, mTBN2));

			// 草ブレンド率でモデル法線と合成
			wN = normalize(lerp(wN, grassN, grassBlend));
		}
	}
	//   面の法線ではなく「球中心 → ピクセル」方向を法線として使う。
	//   ポリゴン数に関係なく完全に滑らかな陰影になる。
	//------------------------------------------
	if (g_SphereNormal)
	{
		// ワールド行列の平行移動成分が球中心
		float3 sphereCenter = float3(g_mWorld._41, g_mWorld._42, g_mWorld._43);
		wN = normalize(In.wPos - sphereCenter);
	}

	//------------------------------------------
	// PBR マテリアルパラメータ
	//------------------------------------------
	float4 mr       = g_metalRoughTex.Sample(g_ss, In.UV);
	float  metallic  = saturate(mr.b * g_Metallic);
	float  roughness = saturate(mr.g * g_Roughness);
	roughness = max(roughness, 0.04f); // 完全鏡面防止

	// F0: 非金属=0.04、金属=ベースカラー
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);
	// 拡散アルベド: 金属は0
	float3 albedo = baseColor.rgb * (1.0f - metallic);

	//------------------------------------------
	// シャドウ（法線オフセット PCSS）
	//------------------------------------------
	float shadow = CalcShadow(In.wPos, wN);
	// 影の最小明度（0で真っ黒、上げるほど影が薄くなる）。環境光で持ち上がる
	shadow = lerp(0.15f, 1.0f, shadow);

	//------------------------------------------
	// ライティング計算
	//------------------------------------------
	float3 outColor = 0.0f;

	// ---- 平行光 (トゥーン/セルシェード) ----
	// 明るさを段階化してフラットなアニメ調に。これで輪郭が際立つ。
	{
		float3 lightDir = normalize(-g_DL_Dir);

		// 自己陰影＋落ち影をまとめて段階化
		float lightAmt = saturate(dot(wN, lightDir)) * shadow;
		float toon     = ToonRamp(lightAmt);
		outColor += albedo * g_DL_Color * toon;

		// トゥーンスペキュラ：くっきりした1段のハイライト
		float3 H   = normalize(vCam + lightDir);
		float  ndh = saturate(dot(wN, H));
		float  shininess = exp2(lerp(8.0f, 1.0f, roughness));   // 粗いほど鈍い
		float  spec      = pow(ndh, shininess);
		float  specToon  = smoothstep(0.48f, 0.52f, spec) * (1.0f - roughness);
		outColor += g_DL_Color * specToon * shadow * 0.6f;
	}

	// ---- 点光 (PBR Cook-Torrance) ----
	float totalBrightness = g_AmbientLight.a;
	for (int i = 0; i < g_PointLightNum.x; ++i)
	{
		float3 toLight = g_PointLights[i].Pos - In.wPos;
		float  dist    = length(toLight);
		if (dist >= g_PointLights[i].Radius)
			continue;

		float3 lightDir = toLight / dist;

		// 距離減衰（逆二乗）
		float  atte = 1.0f - saturate(dist / g_PointLights[i].Radius);
		atte *= atte;

		// 明度ライト寄与
		float atteFull = 1.0f - saturate(dist / g_PointLights[i].Radius);
		totalBrightness += (1.0f - pow(1.0f - atteFull, 2.0f)) * g_PointLights[i].IsBright;

		outColor += CookTorrance(lightDir, vCam, wN,
								 albedo, F0, roughness, metallic,
								 g_PointLights[i].Color, atte);
	}

	// ---- 半球環境光（空色 / 地面色で影が黒くなるのを防ぐ）----
	{
		float hemi    = dot(wN, float3(0.0f, 1.0f, 0.0f)) * 0.5f + 0.5f;
		float3 ambient = lerp(g_AmbientLight.rgb * 0.4f,
							  g_AmbientLight.rgb,
							  hemi);
		outColor += ambient * baseColor.rgb * baseColor.a;
	}

	// ---- 擬似環境反射（スペキュラ IBL 近似）----
	//   反射ベクトルの上下成分で「星空（上）／暗い宇宙（下）」を擬似的に映し込む。
	//   ラフネスが低い（つるつる）ほど強く、金属ほど色付きで反射する。
	{
		float3 R       = reflect(-vCam, wN);
		float  upFac   = R.y * 0.5f + 0.5f; // 上向き=1, 下向き=0
		// 環境色を上下でブレンド（上は星空寄りに明るく、下は暗く）
		float3 envColor = g_AmbientLight.rgb *
						  lerp(k_EnvReflectDownMul, k_EnvReflectUpMul, upFac);

		// フレネル：浅い角度ほど反射が強い
		float  NdotV    = saturate(dot(wN, vCam));
		float3 fresnel  = F0 + (max(1.0f - roughness, F0) - F0) *
						  pow(saturate(1.0f - NdotV), 5.0f);

		// 粗い面は反射をぼかす＝弱める　金属ほど強く反射
		float  glossy   = (1.0f - roughness) * (0.4f + 0.6f * metallic);
		// 影の中では反射も弱める（影が反射光で洗い流されるのを防ぐ）
		outColor += envColor * fresnel * glossy * lerp(0.4f, 1.0f, shadow);
	}

	// ---- リムライト（縁光）：宇宙空間の浮遊感・立体感を強調 ----
	{
		float  NdotV = saturate(dot(wN, vCam));
		float  rim   = pow(1.0f - NdotV, k_RimPower);
		// 平行光の当たっている側ほど縁が強く光る（逆光リムの自然さ）
		float  backLit = saturate(dot(wN, normalize(-g_DL_Dir))) * 0.5f + 0.5f;
		// 太陽光由来の縁光なので、影の中では消す（影を洗い流さない）
		outColor += rim * k_RimStrength * backLit * g_DL_Color * shadow;
	}

	// ---- エミッシブ ----
	float3 emissive = g_emissiveTex.Sample(g_ss, In.UV).rgb * g_Emissive * In.Color.rgb;
	if (g_OnlyEmissie)
		outColor = emissive;
	else
		outColor += emissive;

	//------------------------------------------
	// フォグ
	//------------------------------------------
	if (g_HeightFogEnable && g_FogEnable)
	{
		if (In.wPos.y < g_HeightFogTopValue)
		{
			float distRate   = saturate(length(In.wPos - g_CamPos) / g_HeightFogDistance);
			distRate = pow(distRate, 2.0f);
			float heightRange = g_HeightFogTopValue - g_HeightFogBottomValue;
			float heightRate  = 1.0f - saturate((In.wPos.y - g_HeightFogBottomValue) / heightRange);
			outColor = lerp(outColor, g_HeightFogColor, heightRate * distRate);
		}
	}
	if (g_DistanceFogEnable && g_FogEnable)
	{
		float f = saturate(1.0f / exp(camDist * g_DistanceFogDensity));
		outColor = lerp(g_DistanceFogColor, outColor, f);
	}

	//------------------------------------------
	// ディゾルブ輪郭発光
	//------------------------------------------
	if (g_dissolveValue > 0.0f)
	{
		if (abs(discardValue - g_dissolveValue) < g_dissolveEdgeRange)
			outColor += g_dissolveEmissive;
	}

	//------------------------------------------
	// 明度クランプ
	//------------------------------------------
	totalBrightness = saturate(totalBrightness);
	outColor *= totalBrightness;

	//------------------------------------------
	// トーンマッピング（HDR→LDR、ハイライトの飛びを抑え階調を残す）
	//------------------------------------------
	static const float k_Exposure = 1.1f;
	outColor = ACESFilm(outColor * k_Exposure);

	return float4(outColor, baseColor.a);
}
