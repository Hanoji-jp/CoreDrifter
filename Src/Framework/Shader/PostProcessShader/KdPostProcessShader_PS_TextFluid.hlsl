#include "../inc_KdCommon.hlsli"
#include "inc_KdPostProcessShader.hlsli"

// 文字テキストエフェクト集：文字を焼いたRT(輝度=文字の形)をマスクに、複数スタイルを
// 手続き的に描く。g_Styleで切替。0=グラデ 1=虹スモーク 2=炎 3=ハーフトーン
// 4=スキャンライン 5=色収差スタック 6=3D押し出し 7=ワープ。
Texture2D    g_textTex : register(t0);
SamplerState g_ss      : register(s0);

cbuffer cb : register(b0)
{
	float2 g_TexelSize;
	float  g_Time;
	float  g_WarpAmp;     // 効果の強さ/量(スタイルで意味変化)

	float  g_WarpFreq;    // 模様/ノイズの細かさ
	float  g_FlowSpeed;   // 流れ/アニメ速さ
	float  g_Dilate;      // トレイルの長さ等
	float  g_Intensity;   // 0-1 効果の強さ(ドリフト量)

	float4 g_CoreColor;   // 主色
	float4 g_FluidColor;  // 副色

	float  g_Style;        // スタイル番号
	float  g_LetterCount;  // 文字数(ハーフトーンの桁ごとグラデ用)
	float  g_RectCX;       // 表示位置(中心X, 画面比)
	float  g_RectCY;       // 表示位置(中心Y)

	float  g_RectW;        // 表示幅(画面比)
	float  g_RectH;        // 表示高さ
	float  g_ShadowAngle;  // ロングシャドウの角度(度。0=右, 45=右下)
	float  g_ShadowLevel;  // (予備)

	float4 g_ShadowColor;  // ロングシャドウの色(rgb=色, a=不透明度)

	// 各グリフの左右エッジ(RT幅で正規化, [0,1])。可変幅(全角/半角混在)の桁ごとグラデ用。
	// g_GlyphEdges[i]=グリフ左端、[i+1]=右端。最大31文字(=32エッジ)。
	float4 g_GlyphEdges[8];
};

// i番目のエッジ(0-31)を取り出す(float4×8を連続float32として添字アクセス)
float glyphEdge(int i) { return g_GlyphEdges[i >> 2][i & 3]; }

// x座標(luv.x)が属する文字の中での桁ごとトーン(左=0.05→右=1.0)。可変幅対応。
// 影も"発生元(文字側)"のxでこれを引く＝影の向きが左右どちらでも破綻しない。
float toneAtX(float x)
{
	int   lc = (int)(g_LetterCount + 0.5);
	float tn = 0.05;
	[loop] for (int gi = 0; gi < lc; ++gi)
	{
		float x0 = glyphEdge(gi);
		float x1 = glyphEdge(gi + 1);
		if (x >= x0 && x < x1) { tn = saturate(0.05 + 0.95 * ((x - x0) / max(x1 - x0, 1e-4))); }
	}
	return tn;
}

// ---- 疑似ノイズ ----
float hash21(float2 p) { p = frac(p * float2(123.34, 345.45)); p += dot(p, p + 34.345); return frac(p.x * p.y); }
float vnoise(float2 p)
{
	float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i), b = hash21(i + float2(1, 0)), c = hash21(i + float2(0, 1)), d = hash21(i + float2(1, 1));
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}
float fbm(float2 p) { float v = 0.0, amp = 0.5; [unroll] for (int i = 0; i < 4; ++i) { v += amp * vnoise(p); p *= 2.0; amp *= 0.5; } return v; }

// ---- パレット ----
float3 palIrid(float t) { return 0.55 + 0.45 * cos(6.2831 * (t + float3(0.0, 0.33, 0.66))); }        // 虹
float3 palFire(float t) { t = saturate(t); return float3(saturate(t * 3.0), saturate(t * 3.0 - 1.2), saturate(t * 5.0 - 4.0)); } // 黒→赤→橙→黄→白

float sampleGlyph(float2 luv)
{
	if (luv.x < 0.0 || luv.x > 1.0 || luv.y < 0.0 || luv.y > 1.0) { return 0.0; }
	float3 c = g_textTex.Sample(g_ss, luv).rgb;
	return max(c.r, max(c.g, c.b));
}
float glyphMask(float2 luv) { return smoothstep(0.45, 0.55, sampleGlyph(luv)); }

// 方向トレイル：flow方向へなびく煙(うねり付き)。戻り値=濃度。
float trailField(float2 luv, float2 flow)
{
	float2 fperp = float2(-flow.y, flow.x);
	float  maxLen = g_Dilate * g_Intensity;
	float  trail = 0.0;
	float2 pos = luv;
	[unroll] for (int i = 1; i <= 22; ++i)
	{
		float t = (float)i / 22.0;
		float curl = (fbm(pos * g_WarpFreq + g_Time * g_FlowSpeed) - 0.5) * (g_WarpAmp * 22.0);
		float2 d = normalize(flow + fperp * curl);
		pos -= d * (maxLen / 22.0);
		trail = max(trail, sampleGlyph(pos) * (1.0 - t));
	}
	return trail;
}

float4 main(VSOutput In) : SV_Target0
{
	// 表示矩形はcbufferから(ImGuiで位置/サイズ調整)
	float2 RECT_MIN = float2(g_RectCX - g_RectW * 0.5, g_RectCY - g_RectH * 0.5);
	float2 RECT_MAX = float2(g_RectCX + g_RectW * 0.5, g_RectCY + g_RectH * 0.5);
	float2 luv = (In.UV - RECT_MIN) / (RECT_MAX - RECT_MIN);
	int style = (int)(g_Style + 0.5);

	// ===== 1: 虹スモーク(下へ流れる) BREATHE =====
	if (style == 1)
	{
		float mask  = glyphMask(luv);
		float trail = trailField(luv, float2(0.12, 1.0));     // 下へ
		float dens  = max(mask, trail);
		float3 tcol = palIrid(luv.y * 1.2 + g_Time * 0.15);   // 虹の帯
		float3 rgb  = lerp(tcol, float3(1, 1, 1), mask);      // 文字=白 / 煙=虹
		return float4(rgb, smoothstep(0.0, 0.5, dens) * g_CoreColor.a);
	}
	// ===== 2: 炎(横へ流れる) DESIGN =====
	if (style == 2)
	{
		float mask  = glyphMask(luv);
		float trail = trailField(luv, float2(1.0, -0.2));      // 右上へ
		float dens  = max(mask, trail);
		float heat  = saturate(trail * 1.3 + fbm(luv * 8.0 + g_Time * g_FlowSpeed * 2.0) * 0.25);
		float3 tcol = palFire(heat);
		float3 rgb  = lerp(tcol, float3(1, 1, 1), mask * 0.85);
		return float4(rgb, smoothstep(0.0, 0.45, dens) * g_CoreColor.a);
	}
	// ===== 3: ハーフトーン(ドット) PANTER =====
	if (style == 3)
	{
		float mask = glyphMask(luv);

		// 矩形アスペクト(以降のドット正方セル化＋影の45°方向補正に使う)
		float rectWpx = g_RectW / g_TexelSize.x;
		float rectHpx = g_RectH / g_TexelSize.y;
		float aspect  = rectHpx / rectWpx;

		// ロングシャドウ：文字から指定角度へ一本の長い影を伸ばす(フラットデザイン風)。
		// 影方向へ少しずつ遡って文字に当たれば影＝斜めに繋がった帯になる。
		//   角度=g_ShadowAngle(度) / 長さ=g_Dilate / 濃さ=g_ShadowLevel。
		float  sa   = radians(g_ShadowAngle);
		float2 sdir = normalize(float2(cos(sa), sin(sa) / max(aspect, 1e-3)));  // 画面上の角度になるようY補正
		float  slen = max(g_Dilate, 0.001);
		float  longSh = 0.0;
		float2 shadowSrc = luv;   // 影の"発生元"(文字側)の座標。トーンをここから引く。
		[loop] for (int i = 1; i <= 48; ++i)
		{
			float  t  = (float)i / 48.0;
			float2 sp = luv - sdir * slen * t;
			float  m  = glyphMask(sp);
			if (m > longSh) { longSh = m; shadowSrc = sp; }   // 帯の中は一律＋発生元を記録
		}

		// ハーフトーンのドット：矩形のアスペクトを補正して"正方セル"にし、真円ドットにする。
		float N = g_WarpFreq * 34.0;                    // ドット密度(WarpFreqで調整。高密度)
		float2 cc   = float2(luv.x, luv.y * aspect) * N;
		float2 cell = frac(cc) - 0.5;
		float  dd   = length(cell);

		// ドット径：文字ごとのトーンで決まる(左=小→右=大。最大0.74=ドット同士が接するくらい)。
		//   前面 = 自分の位置のトーン / 影 = 発生元(文字側)のトーン
		//   ＝影の向きが右下でも真左でも、隣の桁の高トーンを拾わず破綻しない。
		float  frontTone   = toneAtX(luv.x);
		float  shadowTone  = toneAtX(shadowSrc.x);
		float  frontRadius = 0.14 + 0.60 * frontTone;
		float  shadowRadius= 0.14 + 0.60 * shadowTone;
		float  frontShape  = smoothstep(frontRadius,  frontRadius  - 0.05, dd);
		float  shadowShape = smoothstep(shadowRadius, shadowRadius - 0.05, dd);

		// 前面=ハーフトーンのドット(主色) / 影=発生元トーンのドット(専用の影色)。
		float3 frontCol  = g_CoreColor.rgb;
		float3 shadowCol = g_ShadowColor.rgb;

		float frontDot   = frontShape  * mask;                     // 文字の中だけドット
		float shadowDot  = shadowShape * (1.0 - mask);             // 文字の外に影のドット

		// 影帯の中はフェード無し(一律の濃さ)。長さ(slen)で帯の太さだけが決まる。
		float frontA  = frontDot  * g_CoreColor.a;                 // 前面の不透明度
		float shadowA = shadowDot * longSh * g_ShadowColor.a;      // 影の不透明度(帯の中は一律)

		float3 rgb = (frontA >= shadowA) ? frontCol : shadowCol;
		float  a   = max(frontA, shadowA);
		return float4(rgb, a);
	}
	// ===== 4: スキャンライン(横縞) PRAGUE =====
	if (style == 4)
	{
		float mask  = glyphMask(luv);
		float lines = 0.5 + 0.5 * sin(luv.y * (g_WarpFreq * 40.0) + g_Time * g_FlowSpeed * 4.0);
		float lm    = smoothstep(0.35, 0.65, lines);
		return float4(g_CoreColor.rgb, mask * lm * g_CoreColor.a);
	}
	// ===== 5: 色収差スタック STACKED / CYBERTRON =====
	if (style == 5)
	{
		float off = g_WarpAmp * 0.6;
		float r = glyphMask(luv - float2(off, 0.0));
		float g = glyphMask(luv);
		float b = glyphMask(luv + float2(off, 0.0));
		float echo = 0.0;                                     // 背後のエコー(残像)
		[unroll] for (int k = 1; k <= 6; ++k) { echo = max(echo, glyphMask(luv + float2(0.0, (float)k * 0.045)) * (1.0 - (float)k * 0.15)); }
		float3 rgb = float3(max(r, echo * 0.35), g * 0.95, max(b, echo * 0.55));
		float  a   = max(max(r, g), max(b, echo * 0.6));
		return float4(rgb, a * g_CoreColor.a);
	}
	// ===== 6: 3D押し出しポップ CREDITS / CARTOON =====
	if (style == 6)
	{
		float mask = glyphMask(luv);
		float ext  = 0.0;                                     // 右下へ押し出し影
		[unroll] for (int k = 1; k <= 10; ++k) { ext = max(ext, glyphMask(luv + float2((float)k * 0.006, (float)k * 0.009))); }
		float3 front = lerp(g_CoreColor.rgb, g_CoreColor.rgb * 0.75, saturate(luv.y)); // 前面グラデ
		float3 rgb   = (mask > 0.5) ? front : g_FluidColor.rgb;                        // 側面=副色
		return float4(rgb, max(mask, ext) * g_CoreColor.a);
	}
	// ===== 7: ワープ(うねり変形) EXPLORE / PRAGUE =====
	if (style == 7)
	{
		float2 w;
		w.x = fbm(luv * g_WarpFreq + g_Time * g_FlowSpeed) - 0.5;
		w.y = fbm(luv * g_WarpFreq + 5.3 + g_Time * g_FlowSpeed) - 0.5;
		float mask = glyphMask(luv + w * (g_WarpAmp * 3.0));
		return float4(g_CoreColor.rgb, mask * g_CoreColor.a);
	}

	// ===== 0: グラデ＋スクロール模様(既定) SATORI / GOLDEN =====
	float mask = glyphMask(luv);
	if (mask <= 0.001) { return float4(0.0, 0.0, 0.0, 0.0); }
	float3 grad = lerp(g_CoreColor.rgb, g_FluidColor.rgb, saturate(luv.y));
	float2 fuv  = luv * g_WarpFreq + float2(g_Time * g_FlowSpeed, g_Time * g_FlowSpeed * 0.35);
	float  pat  = saturate(0.5 + (fbm(fuv) - 0.5) * (1.0 + g_WarpAmp * 12.0));
	return float4(grad * (0.55 + 0.75 * pat), mask * g_CoreColor.a);
}
