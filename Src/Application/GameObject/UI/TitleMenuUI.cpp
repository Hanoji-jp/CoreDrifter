#include "TitleMenuUI.h"

namespace
{
	// メニュー項目(左寄せ)
	const char* kMenuLabels[UIConst::MenuCount] =
	{
		"PLAY", "GARAGE", "SETTINGS", "STATISTICS", "QUIT"
	};

	// メニューの起点(デザイン座標, 左上原点)。CSS実測に忠実。
	const float kMenuTopDy  = 380.0f;   // 1行目の上端
	const float kMenuLeftDx = 54.0f;    // 左端
	const float kMenuRowDy  = 51.0f;    // 行送り(padding15*2+font21)
	const float kMenuWDx    = 400.0f;   // 行幅(デザインpx)
}

void TitleMenuUI::Init()
{
	m_sel = 0;
}

void TitleMenuUI::Update()
{
	// 上下キーで選択移動(押した瞬間だけ)
	const bool up = (GetAsyncKeyState(VK_UP)   & 0x8000) != 0 || (GetAsyncKeyState('W') & 0x8000) != 0;
	const bool dn = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 || (GetAsyncKeyState('S') & 0x8000) != 0;

	if (up && !m_prevUp) { m_sel = (m_sel + UIConst::MenuCount - 1) % UIConst::MenuCount; }
	if (dn && !m_prevDn) { m_sel = (m_sel + 1) % UIConst::MenuCount; }

	m_prevUp = up;
	m_prevDn = dn;
}

// ════════════ 描画ヘルパー(すべてデザイン座標 1536x864 基準) ════════════

void TitleMenuUI::DrawText(int fontId, float dLeft, float dTop, float pxHeight,
	const char* str, const Math::Color& col)
{
	auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, false);
	if (!sprite || sprite->GetTexList().empty()) { return; }
	// DrawFontはグリフのテクスチャ高(=セル高)ぶん上へ描く。em高(小=上寄り)とセル高(大=下寄り)の
	// 中点で下げると、キャップ上端が概ねdTopに揃う。
	float gh = pxHeight;
	if (sprite->GetTexList()[0]->FontTex)
	{
		const float cell = static_cast<float>(sprite->GetTexList()[0]->FontTex->GetInfo().Height);
		gh = (pxHeight + cell) * 0.5f;
	}
	const Math::Vector2 pos = { MapX(dLeft), MapY(dTop) - gh };
	KdShaderManager::Instance().m_spriteShader.DrawFont(sprite, pos, &col, 0);
}

// 1文字ずつ描いて字送りにトラッキングを加える(DrawFontは字幅ぶんしか進まないため)。
float TitleMenuUI::DrawTextTracked(int fontId, float dLeft, float dTop, float pxHeight,
	const char* str, const Math::Color& col, float trackDesign)
{
	auto& fm = KdFontManager::Instance();
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	float x  = MapX(dLeft);
	const float track = trackDesign * UIConst::Scale;
	// em高とセル高の中点で下げる(DrawTextと同じ基準)。
	float yb = MapY(dTop) - pxHeight;
	{
		auto probe = fm.CreateFontTexture(fontId, "M", false);
		if (probe && !probe->GetTexList().empty() && probe->GetTexList()[0]->FontTex)
		{
			const float cell = static_cast<float>(probe->GetTexList()[0]->FontTex->GetInfo().Height);
			yb = MapY(dTop) - (pxHeight + cell) * 0.5f;
		}
	}
	for (const char* p = str; *p; ++p)
	{
		char one[2] = { *p, '\0' };
		auto s = fm.CreateFontTexture(fontId, one, false);
		float w = 0.0f;
		if (s && !s->GetTexList().empty() && s->GetTexList()[0]->FontTex)
		{
			const Math::Vector2 pos = { x, yb };
			sp.DrawFont(s, pos, &col, 0);
			w = static_cast<float>(s->GetTexList()[0]->FontTex->GetInfo().Width);
		}
		x += w + track;
	}
	return x - track;   // 末尾のトラッキングは含めない
}

float TitleMenuUI::MeasureText(int fontId, const char* str, float trackDesign)
{
	auto& fm = KdFontManager::Instance();
	float w = 0.0f;
	const float track = trackDesign * UIConst::Scale;
	for (const char* p = str; *p; ++p)
	{
		char one[2] = { *p, '\0' };
		auto s = fm.CreateFontTexture(fontId, one, false);
		if (s && !s->GetTexList().empty() && s->GetTexList()[0]->FontTex)
		{
			w += static_cast<float>(s->GetTexList()[0]->FontTex->GetInfo().Width) + track;
		}
	}
	return (w > 0.0f) ? (w - track) : 0.0f;
}

void TitleMenuUI::DrawTextVert(int fontId, float dLeft, float dTop, float pxStep,
	const char* str, const Math::Color& col)
{
	auto& fm = KdFontManager::Instance();
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	int i = 0;
	for (const char* p = str; *p; ++p, ++i)
	{
		char one[2] = { *p, '\0' };
		if (*p == ' ') { continue; }
		auto sprite = fm.CreateFontTexture(fontId, one, false);
		if (!sprite) { continue; }
		const Math::Vector2 pos = { MapX(dLeft), MapY(dTop + i * (pxStep / UIConst::Scale)) - pxStep };
		sp.DrawFont(sprite, pos, &col, 0);
	}
}

// 横書きを時計回り90°回転して縦帯に描く(vertical-rl)。各グリフのクアッドを回転させる。
void TitleMenuUI::DrawTextRotated(int fontId, float dCx, float dCy,
	const char* str, const Math::Color& col, float trackDesign)
{
	auto& fm = KdFontManager::Instance();
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	auto sprite = fm.CreateFontTexture(fontId, str, false);
	if (!sprite || sprite->GetTexList().empty()) { return; }

	const float track = trackDesign * UIConst::Scale;

	// まず縦方向の総長(=各グリフ幅+字間の合計)を測って中央揃えの開始Yを決める
	float total = 0.0f;
	for (auto& d : sprite->GetTexList())
	{
		if (!d || !d->FontTex || d->Code == '\n') { continue; }
		total += static_cast<float>(d->FontTex->GetInfo().Width) + track;
	}
	const float cx     = MapX(dCx);
	float       cursor = MapY(dCy) + total * 0.5f;   // 上端(高いy)から下へ進める

	for (auto& d : sprite->GetTexList())
	{
		if (!d || !d->FontTex || d->Code == '\n') { continue; }
		const float W = static_cast<float>(d->FontTex->GetInfo().Width);
		const float H = static_cast<float>(d->FontTex->GetInfo().Height);
		const float gcx = cx;
		const float gcy = cursor - W * 0.5f;   // このグリフの中心

		// 時計回り90°回転: 元の(x,y)→(y,-x)。幅Wが縦、高さHが横になる。
		const std::vector<KdSpriteShader::Vertex> verts = {
			{ { gcx - H * 0.5f, gcy + W * 0.5f, 0.0f }, { 0.0f, 1.0f } }, // BL
			{ { gcx + H * 0.5f, gcy + W * 0.5f, 0.0f }, { 0.0f, 0.0f } }, // TL
			{ { gcx - H * 0.5f, gcy - W * 0.5f, 0.0f }, { 1.0f, 1.0f } }, // BR
			{ { gcx + H * 0.5f, gcy - W * 0.5f, 0.0f }, { 1.0f, 0.0f } }, // TR
		};
		sp.DrawTexVertices(d->FontTex.get(), verts, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, &col);

		cursor -= (W + track);
	}
}

void TitleMenuUI::DrawRectTL(float dx, float dy, float w, float h, const Math::Color& col, bool fill)
{
	const int cx = static_cast<int>(MapX(dx + w * 0.5f));
	const int cy = static_cast<int>(MapY(dy + h * 0.5f));
	const int ex = static_cast<int>(w * UIConst::Scale * 0.5f);
	const int ey = static_cast<int>(h * UIConst::Scale * 0.5f);
	KdShaderManager::Instance().m_spriteShader.DrawBox(cx, cy, ex, ey, &col, fill);
}

// シーンRT(ゲーム画面)を、このボックスの画面位置に1:1で窓抜き描画する。
void TitleMenuUI::DrawSceneWindow(float dx, float dy, float w, float h)
{
	const auto& tex = KdShaderManager::Instance().m_postProcessShader.GetSceneRT();
	if (!tex) { return; }
	auto& sp = KdShaderManager::Instance().m_spriteShader;

	// RT実サイズに対する割合でsrcRectを作る(RT解像度に依存しない=常に1:1の窓)
	const float TW = static_cast<float>(tex->GetInfo().Width);
	const float TH = static_cast<float>(tex->GetInfo().Height);
	Math::Rectangle src(
		static_cast<long>(dx / UIConst::DesignW * TW),
		static_cast<long>(dy / UIConst::DesignH * TH),
		static_cast<long>(w  / UIConst::DesignW * TW),
		static_cast<long>(h  / UIConst::DesignH * TH));

	const int cx = static_cast<int>(MapX(dx + w * 0.5f));
	const int cy = static_cast<int>(MapY(dy + h * 0.5f));
	const int dw = static_cast<int>(w * UIConst::Scale);
	const int dh = static_cast<int>(h * UIConst::Scale);
	sp.DrawTex(tex.get(), cx, cy, dw, dh, &src, &UIConst::WHITE, { 0.5f, 0.5f });
}

// 太さpx(screen)の枠を4辺のベタ塗りで描く(デザインの2px罫線を再現)
void TitleMenuUI::DrawFrameTL(float dx, float dy, float w, float h, float px, const Math::Color& col)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	const float left = MapX(dx), top = MapY(dy);
	const float ws = w * UIConst::Scale, hs = h * UIConst::Scale;
	const float t = px * 0.5f;
	// 上辺・下辺
	sp.DrawBox(static_cast<int>(left + ws * 0.5f), static_cast<int>(top - t),            static_cast<int>(ws * 0.5f), static_cast<int>(t), &col, true);
	sp.DrawBox(static_cast<int>(left + ws * 0.5f), static_cast<int>(top - hs + t),        static_cast<int>(ws * 0.5f), static_cast<int>(t), &col, true);
	// 左辺・右辺
	sp.DrawBox(static_cast<int>(left + t),         static_cast<int>(top - hs * 0.5f),     static_cast<int>(t), static_cast<int>(hs * 0.5f), &col, true);
	sp.DrawBox(static_cast<int>(left + ws - t),    static_cast<int>(top - hs * 0.5f),     static_cast<int>(t), static_cast<int>(hs * 0.5f), &col, true);
}

void TitleMenuUI::DrawDotField(float dx, float dy, float w, float h, const Math::Color& col)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	const float left = MapX(dx), top = MapY(dy);
	const float ws = w * UIConst::Scale, hs = h * UIConst::Scale;
	for (float yy = top; yy >= top - hs; yy -= UIConst::DotCell)
	{
		for (float xx = left; xx <= left + ws; xx += UIConst::DotCell)
		{
			sp.DrawCircle(static_cast<int>(xx), static_cast<int>(yy), 2, &col, true);
		}
	}
}

void TitleMenuUI::DrawLineD(float x1, float y1, float x2, float y2, float px, const Math::Color& col)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	const int n = std::max(1, static_cast<int>(px));
	// 太さぶん平行にずらして重ね描き
	for (int i = 0; i < n; ++i)
	{
		const float o = static_cast<float>(i) - (n - 1) * 0.5f;
		sp.DrawLine(static_cast<int>(MapX(x1)), static_cast<int>(MapY(y1) + o),
			static_cast<int>(MapX(x2)), static_cast<int>(MapY(y2) + o), &col);
		sp.DrawLine(static_cast<int>(MapX(x1) + o), static_cast<int>(MapY(y1)),
			static_cast<int>(MapX(x2) + o), static_cast<int>(MapY(y2)), &col);
	}
}

void TitleMenuUI::DrawCross(float cx, float cy, float r, float px, const Math::Color& col)
{
	DrawLineD(cx - r, cy, cx + r, cy, px, col);
	DrawLineD(cx, cy - r, cx, cy + r, px, col);
}

void TitleMenuUI::DrawDashed(float x1, float y1, float x2, float y2, const Math::Color& col)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	const float sx = MapX(x1), sy = MapY(y1), ex = MapX(x2), ey = MapY(y2);
	const float len = std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));
	const float dash = 4.0f;
	const int   steps = std::max(1, static_cast<int>(len / (dash * 2.0f)));
	for (int i = 0; i < steps; ++i)
	{
		const float t0 = (float)(i * 2) / (steps * 2);
		const float t1 = (float)(i * 2 + 1) / (steps * 2);
		sp.DrawLine(static_cast<int>(sx + (ex - sx) * t0), static_cast<int>(sy + (ey - sy) * t0),
			static_cast<int>(sx + (ex - sx) * t1), static_cast<int>(sy + (ey - sy) * t1), &col);
	}
}

void TitleMenuUI::DrawRingD(float cx, float cy, float r, const Math::Color& col)
{
	KdShaderManager::Instance().m_spriteShader.DrawCircle(
		static_cast<int>(MapX(cx)), static_cast<int>(MapY(cy)), static_cast<int>(r * UIConst::Scale), &col, false);
}

void TitleMenuUI::DrawDotD(float cx, float cy, float r, const Math::Color& col)
{
	KdShaderManager::Instance().m_spriteShader.DrawCircle(
		static_cast<int>(MapX(cx)), static_cast<int>(MapY(cy)), std::max(1, static_cast<int>(r * UIConst::Scale)), &col, true);
}

// メニューアイコン(幾何形状でユニコード字形を代替)。cx,cyはscreen座標。
void TitleMenuUI::DrawMenuIcon(int index, float cx, float cy, const Math::Color& col)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	const int x = static_cast<int>(cx), y = static_cast<int>(cy);
	switch (index)
	{
	case 0: // PLAY ▶
		sp.DrawTriangle(x - 6, y - 7, x - 6, y + 7, x + 7, y, &col, true);
		break;
	case 1: // GARAGE ⌂ (家)
		sp.DrawTriangle(x - 8, y - 1, x + 8, y - 1, x, y - 9, &col, true);
		sp.DrawBox(x, y + 3, 6, 5, &col, false);
		break;
	case 2: // SETTINGS ⚙ (歯車=輪+中心)
		sp.DrawCircle(x, y, 8, &col, false);
		sp.DrawCircle(x, y, 3, &col, true);
		break;
	case 3: // STATISTICS ▥ (棒グラフ)
		sp.DrawBox(x - 6, y + 3, 1, 3, &col, true);
		sp.DrawBox(x,     y + 1, 1, 5, &col, true);
		sp.DrawBox(x + 6, y - 1, 1, 7, &col, true);
		break;
	case 4: // QUIT ⇥ (右矢印)
		sp.DrawLine(x - 8, y, x + 6, y, &col);
		sp.DrawTriangle(x + 3, y - 5, x + 3, y + 5, x + 9, y, &col, true);
		break;
	default: break;
	}
}

// ════════════════════════════ 本体 ════════════════════════════
void TitleMenuUI::DrawSprite()
{
	using namespace UIConst;
	auto& sp = KdShaderManager::Instance().m_spriteShader;

	// ── 背景(紙) ──
	sp.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	// ══ 画像処理(z2)：ボックス群にゲーム画面(シーンRT)を窓抜きマスク ══
	DrawRectTL(726.0f, 66.0f, 430.0f, 450.0f, GREY, true);     // 背面フレーム(下地)
	DrawSceneWindow(726.0f, 66.0f, 430.0f, 450.0f);           // 背面フレームにゲーム画面
	DrawRectTL(796.0f, 128.0f, 590.0f, 540.0f, GREY, true);    // メインフレーム(下地)
	DrawSceneWindow(796.0f, 128.0f, 590.0f, 540.0f);          // メインフレームにゲーム画面
	{   // アシッドのデュオトーン・スライス(右44%。ゲーム画面が透ける半透明)
		Math::Color slice = ACID; slice.w = 0.55f;
		DrawRectTL(1126.0f, 128.0f, 260.0f, 540.0f, slice, true);
	}
	DrawRectTL(796.0f, 650.0f, 160.0f, 32.0f, ACID, true);     // アクセントブロック

	// ══ トップ帯(z3) ══
	// 左：◍ + スローガン
	sp.DrawCircle(static_cast<int>(MapX(65.0f)), static_cast<int>(MapY(51.0f)), 9, &INK, false);
	sp.DrawCircle(static_cast<int>(MapX(65.0f)), static_cast<int>(MapY(51.0f)), 3, &INK, true);
	DrawText(FontStrip, 92.0f, 44.0f, 11.0f, "BUILT TO SLIDE.", INK);
	DrawText(FontStrip, 92.0f, 62.0f, 11.0f, "EST. 2024", INK);
	DrawLineD(92.0f, 84.0f, 235.0f, 84.0f, 2.0f, INK);
	// 右：バッジ DRIVER01 / LV.23 / ///
	DrawFrameTL(1262.0f, 40.0f, 118.0f, 34.0f, 2.0f, INK);
	DrawText(FontSmall, 1276.0f, 49.0f, 12.0f, "DRIVER01", INK);
	DrawRectTL(1380.0f, 40.0f, 70.0f, 34.0f, ACID, true);
	DrawFrameTL(1380.0f, 40.0f, 70.0f, 34.0f, 2.0f, INK);
	DrawText(FontSmall, 1394.0f, 49.0f, 12.0f, "LV.23", INK);
	{
		Math::Color slash = INK; slash.w = 0.85f;
		DrawText(FontSlash, 1462.0f, 40.0f, 17.0f, "///", slash);
	}

	// ══ 大タイトル(z3) ══ CSS: DRIFT 150/lh.72/-.045em, PROJECT 106/lh.8/-.02em
	DrawTextTracked(FontTitle, 52.0f, 92.0f,  125.0f, "DRIFT",   INK, -0.045f * 150.0f);
	DrawTextTracked(FontSub,   54.0f, 206.0f, 88.0f,  "PROJECT", ACID, -0.02f * 106.0f);

	// ══ タグライン(黒バー。inline-block=テキスト幅にフィット) ══
	{
		const float tagTop = 314.0f, padDx = 15.0f, tagH = 32.0f;
		const char* head = "CHASE CONTROL, NOT GLORY.  ";
		const float headW = MeasureText(FontSmall, head, 0.6f) / Scale;   // design px
		const float plusW = MeasureText(FontSmall, "+", 0.0f) / Scale;
		const float barW  = padDx * 2.0f + headW + plusW;
		DrawRectTL(54.0f, tagTop, barW, tagH, INK, true);
		DrawTextTracked(FontSmall, 54.0f + padDx, tagTop + 9.0f, 12.0f, head, WHITE, 0.6f);
		DrawText(FontSmall, 54.0f + padDx + headW, tagTop + 9.0f, 12.0f, "+", ACID);
	}

	// ══ メニュー(z3, 上下キーで選択) ══
	for (int i = 0; i < MenuCount; ++i)
	{
		const float rowDy = kMenuTopDy + i * kMenuRowDy;
		if (i == m_sel)
		{
			DrawRectTL(kMenuLeftDx, rowDy, kMenuWDx, kMenuRowDy, ACID, true);   // 選択=アシッド塗り
		}
		else
		{
			DrawLineD(kMenuLeftDx, rowDy + kMenuRowDy, kMenuLeftDx + kMenuWDx, rowDy + kMenuRowDy, 2.0f, INK30);
		}
		// アイコン(行の中心)
		const float iconCx = MapX(kMenuLeftDx + 30.0f);
		const float iconCy = MapY(rowDy + kMenuRowDy * 0.5f);
		DrawMenuIcon(i, iconCx, iconCy, INK);
		// ラベル
		DrawText(FontMenu, kMenuLeftDx + 56.0f, rowDy + 15.0f, 18.0f, kMenuLabels[i], INK);
	}

	// ══ フッター(z3) CSS: menu下 margin-top:38 ══
	DrawRectTL(54.0f, 673.0f, 12.0f, 12.0f, ACID, true);
	DrawText(FontTiny, 74.0f, 672.0f, 10.0f, "Ver. 0.1.0", INK);
	DrawText(FontTiny, 230.0f, 672.0f, 10.0f, "// WELCOME BACK, DRIVER.", SUBTXT);

	// ══ 縦レール(z3) 横書きを90°回転(vertical-rl)。右端・縦中央。letter-spacing .18em ══
	DrawTextRotated(FontFoot, 1514.0f, 432.0f, "FOCUS // ADAPT // OVERCOME", INK, 0.18f * 13.0f);

	// ══ NOW PLAYING(z4) ══
	DrawRectTL(1124.0f, 768.0f, 346.0f, 66.0f, NPBG, true);
	DrawFrameTL(1124.0f, 768.0f, 346.0f, 66.0f, 2.0f, INK);
	// 波形(小さなバーで代替)
	{
		const float bh[9] = { 6, 12, 18, 9, 22, 12, 6, 15, 9 };
		for (int i = 0; i < 9; ++i)
		{
			DrawRectTL(1140.0f + i * 6.0f, 800.0f - bh[i] * 0.5f, 3.0f, bh[i], INK, true);
		}
	}
	DrawText(FontNpTtl, 1210.0f, 776.0f, 13.0f, "MIDNIGHT", INK);
	DrawText(FontNpTtl, 1210.0f, 793.0f, 13.0f, "DRIVE", INK);
	DrawText(FontNpArt, 1210.0f, 812.0f, 10.0f, "KORDHELL", SUBTXT);
	DrawRectTL(1410.0f, 778.0f, 46.0f, 46.0f, INK, true);              // 再生ボタン
	{   // ▶
		const int bx = static_cast<int>(MapX(1433.0f)), by = static_cast<int>(MapY(801.0f));
		sp.DrawTriangle(bx - 5, by - 7, bx - 5, by + 7, bx + 7, by, &WHITE, true);
	}

	// ══ SVGデコ(z5, 最前面) ══
	// 十字(小さな+)
	DrawCross(648.0f, 80.0f, 8.0f, 2.5f, INK);
	DrawCross(684.0f, 92.0f, 8.0f, 2.5f, INK);
	DrawCross(968.0f, 44.0f, 8.0f, 2.5f, INK);
	// ドットフィールド群
	DrawDotField(700.0f, 150.0f, 70.0f, 42.0f, DOTS);
	DrawDotField(1170.0f, 120.0f, 150.0f, 110.0f, DOTS);
	DrawDotField(1330.0f, 120.0f, 90.0f, 110.0f, GREY9);
	DrawDotField(70.0f,  470.0f, 90.0f, 70.0f, DOTS);
	DrawDotField(840.0f, 720.0f, 180.0f, 46.0f, DOTS);
	DrawDotField(1150.0f, 712.0f, 150.0f, 14.0f, DOTS);
	// 破線のクロップマーク
	DrawDashed(720.0f, 200.0f, 790.0f, 200.0f, INK30);
	DrawDashed(720.0f, 200.0f, 720.0f, 250.0f, INK30);
	// 大きなX
	DrawLineD(712.0f, 448.0f, 744.0f, 480.0f, 6.0f, INK);
	DrawLineD(744.0f, 448.0f, 712.0f, 480.0f, 6.0f, INK);
	// ドリフト円(中心をずらした入れ子)
	{
		Math::Color ring = INK; ring.w = 0.38f;
		DrawRingD(700.0f, 620.0f, 120.0f, ring); DrawRingD(688.0f, 608.0f, 120.0f, ring);
		DrawRingD(676.0f, 596.0f, 120.0f, ring); DrawRingD(664.0f, 584.0f, 120.0f, ring);
		DrawRingD(652.0f, 572.0f, 120.0f, ring); DrawRingD(640.0f, 560.0f, 120.0f, ring);
	}
	// 箱入りのX(アシッド枠)
	DrawFrameTL(700.0f, 636.0f, 54.0f, 54.0f, 5.0f, ACID);
	DrawLineD(713.0f, 649.0f, 741.0f, 677.0f, 4.0f, INK);
	DrawLineD(741.0f, 649.0f, 713.0f, 677.0f, 4.0f, INK);
	// ドットの3連(左)
	{
		Math::Color d = INK; d.w = 0.5f;
		DrawDotD(470.0f, 600.0f, 2.2f, d); DrawDotD(486.0f, 600.0f, 2.2f, d); DrawDotD(502.0f, 600.0f, 2.2f, d);
		DrawDotD(470.0f, 616.0f, 2.2f, d); DrawDotD(486.0f, 616.0f, 2.2f, d); DrawDotD(470.0f, 632.0f, 2.2f, d);
	}
	// ドットの3連(下)
	{
		Math::Color d = INK; d.w = 0.55f;
		DrawDotD(720.0f, 712.0f, 2.4f, d); DrawDotD(734.0f, 712.0f, 2.4f, d); DrawDotD(748.0f, 712.0f, 2.4f, d);
	}
	// 下部の黒バー(右下)
	DrawRectTL(960.0f, 838.0f, 576.0f, 26.0f, INK, true);
}
