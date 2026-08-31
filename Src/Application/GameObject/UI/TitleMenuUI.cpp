#include "TitleMenuUI.h"
#include "../Score/HjPlayerProfile.h"
#include "../../Audio/HjBgm.h"
#include "../../Input/HjKeyInput.h"
#include "HjUI.h"   // マウス入力(BeginInput/Hover/Clicked)

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
	// 名前の編集中はキー入力を独占する。
	// そうしないと、名前に打った文字でメニューが動いてしまう。
	if (m_nameEditing)
	{
		UpdateNameEdit();
		HjUI::BeginInput();   // マウス位置は更新しておく(バッジのホバー判定に要る)
		return;
	}

	// 上下キーで選択移動(押した瞬間だけ)
	auto& key = HjKeyInput::Instance();
	if (key.Pressed(HjKeyInput::Key::Up))   { m_sel = (m_sel + UIConst::MenuCount - 1) % UIConst::MenuCount; }
	if (key.Pressed(HjKeyInput::Key::Down)) { m_sel = (m_sel + 1) % UIConst::MenuCount; }

	// マウス：行にホバーで選択、クリックで決定
	HjUI::BeginInput();
	for (int i = 0; i < UIConst::MenuCount; ++i)
	{
		const float rowDy = kMenuTopDy + i * kMenuRowDy;
		if (HjUI::Hover(kMenuLeftDx, rowDy, kMenuWDx, kMenuRowDy)) { m_sel = i; }
		if (HjUI::Clicked(kMenuLeftDx, rowDy, kMenuWDx, kMenuRowDy)) { m_sel = i; m_activated = true; }
	}

	// NOW PLAYING の再生/停止ボタン。
	// 曲を止めたいときに設定画面まで行かせない
	if (HjUI::Clicked(1410.0f, 778.0f, 46.0f, 46.0f))
	{
		HjBgm::Instance().ToggleMuted();
	}
}

// ════════════ 描画ヘルパー(すべてデザイン座標 1536x864 基準) ════════════

void TitleMenuUI::DrawText(int fontId, float dLeft, float dTop, float pxHeight,
	const char* str, const Math::Color& col)
{
	auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
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
		auto probe = fm.CreateFontTexture(fontId, "M", 3);
		if (probe && !probe->GetTexList().empty() && probe->GetTexList()[0]->FontTex)
		{
			const float cell = static_cast<float>(probe->GetTexList()[0]->FontTex->GetInfo().Height);
			yb = MapY(dTop) - (pxHeight + cell) * 0.5f;
		}
	}
	for (const char* p = str; *p; ++p)
	{
		char one[2] = { *p, '\0' };
		auto s = fm.CreateFontTexture(fontId, one, 3);
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
		auto s = fm.CreateFontTexture(fontId, one, 3);
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
		auto sprite = fm.CreateFontTexture(fontId, one, 3);
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
	auto sprite = fm.CreateFontTexture(fontId, str, 3);
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
// tintはテクスチャに乗算される(緑を渡せば mix-blend:multiply の二階調になる)。
void TitleMenuUI::DrawSceneWindow(float dx, float dy, float w, float h, const Math::Color& tint)
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
	sp.DrawTex(tex.get(), cx, cy, dw, dh, &src, &tint, { 0.5f, 0.5f });
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

	// ── 中断された縦の構築線＋切れ目のクロップティック(参考menu最新版。背面) ──
	{
		const Math::Color gl = { 0.078f, 0.078f, 0.078f, 0.07f };   // ~0.07
		const Math::Color tk = { 0.078f, 0.078f, 0.078f, 0.22f };   // ~0.22
		DrawLineD(470.0f,    0.0f, 470.0f,  180.0f, 1.0f, gl);  DrawLineD(470.0f,  240.0f, 470.0f,  560.0f, 1.0f, gl);
		DrawLineD(770.0f,  120.0f, 770.0f,  430.0f, 1.0f, gl);  DrawLineD(770.0f,  500.0f, 770.0f,  864.0f, 1.0f, gl);
		DrawLineD(1120.0f,   0.0f, 1120.0f, 120.0f, 1.0f, gl);  DrawLineD(1120.0f, 300.0f, 1120.0f, 700.0f, 1.0f, gl);
		auto tick = [&](float x, float y) { DrawLineD(x - 5.0f, y, x + 5.0f, y, 1.5f, tk); };
		tick(470.0f, 180.0f); tick(470.0f, 240.0f); tick(770.0f, 430.0f); tick(770.0f, 500.0f); tick(1120.0f, 120.0f); tick(1120.0f, 300.0f);
	}

	// ══ 画像処理(z2)：ボックス群にゲーム画面(シーンRT)を窓抜きマスク ══
	DrawRectTL(726.0f, 66.0f, 430.0f, 450.0f, GREY, true);     // 背面フレーム(下地)
	DrawSceneWindow(726.0f, 66.0f, 430.0f, 450.0f);           // 背面フレームにゲーム画面
	DrawRectTL(796.0f, 128.0f, 590.0f, 540.0f, GREY, true);    // メインフレーム(下地)
	DrawSceneWindow(796.0f, 128.0f, 590.0f, 540.0f);          // メインフレームにゲーム画面
	// 一番右のスライス(右44%)：ゲーム画面のマスク＋緑を乗算(mix-blend:multiply の二階調)
	DrawSceneWindow(1126.0f, 128.0f, 260.0f, 540.0f, ACID);
	DrawRectTL(796.0f, 650.0f, 160.0f, 32.0f, ACID, true);     // 細い緑のアクセントboxはそのまま

	// ══ トップ帯(z3) ══
	// 左：◍ + スローガン
	sp.DrawCircle(static_cast<int>(MapX(65.0f)), static_cast<int>(MapY(51.0f)), 9, &INK, false);
	sp.DrawCircle(static_cast<int>(MapX(65.0f)), static_cast<int>(MapY(51.0f)), 3, &INK, true);
	DrawPlate();
	DrawBadge();
	// ///(そのまま。上下フロート)
	{
		Math::Color slash = INK; slash.w = 0.85f;
		const float fy = std::sin(HjUI::Time() * 1.6f) * 5.0f;   // 上下フロート
		DrawText(FontSlash, 1462.0f, 40.0f + fy, 17.0f, "///", slash);
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
	// ※バージョンは左上へ集約した。2箇所に出すと更新漏れで食い違う。
	DrawText(FontTiny, 74.0f, 672.0f, 10.0f, "// WELCOME BACK, DRIVER.", SUBTXT);

	// ══ 縦レール(z3) 横書きを90°回転(vertical-rl)。右端・縦中央。letter-spacing .18em ══
	DrawTextRotated(FontFoot, 1514.0f, 432.0f, "FOCUS // ADAPT // OVERCOME", INK, 0.18f * 13.0f);

	// ══ NOW PLAYING(z4) ══
	// 実際に鳴っている曲と連動させる。
	// 飾りとして固定の曲名を出すと、鳴っている音と食い違って
	// 「作り込んでいない所」として目に付く。
	{
		auto& bgm = HjBgm::Instance();
		const bool sounding = bgm.IsSounding();

		DrawRectTL(1124.0f, 768.0f, 346.0f, 66.0f, NPBG, true);
		DrawFrameTL(1124.0f, 768.0f, 346.0f, 66.0f, 2.0f, INK);

		// 波形。鳴っている間だけ動かす。
		// 止めているのに揺れていると、止まったことが伝わらない
		{
			const float base[9] = { 6, 12, 18, 9, 22, 12, 6, 15, 9 };
			for (int i = 0; i < 9; ++i)
			{
				float h = base[i];
				if (sounding)
				{
					// 棒ごとに位相と速さをずらす。揃っていると機械的に見える
					const float t = HjUI::Time() * (5.0f + i * 0.7f) + i * 1.3f;
					h = 4.0f + (base[i] - 2.0f) * (0.55f + 0.45f * std::fabs(std::sin(t)));
				}
				else
				{
					// 止めている間は低く平らに寝かせる
					h = 3.0f;
				}
				DrawRectTL(1140.0f + i * 6.0f, 800.0f - h * 0.5f, 3.0f, h,
				           sounding ? INK : SUBTXT, true);
			}
		}

		// 曲名。長いので枠に収まるところまで
		DrawText(FontNpTtl, 1210.0f, 776.0f, 13.0f, bgm.GetTitle(), INK);
		DrawText(FontNpArt, 1210.0f, 812.0f, 10.0f, bgm.GetArtist(), SUBTXT);

		// 再生/停止ボタン。押せることが分かるよう、指すと色が変わる
		const bool overBtn = HjUI::Hover(1410.0f, 778.0f, 46.0f, 46.0f);
		DrawRectTL(1410.0f, 778.0f, 46.0f, 46.0f, overBtn ? ACID : INK, true);
		{
			const int bx = static_cast<int>(MapX(1433.0f));
			const int by = static_cast<int>(MapY(801.0f));
			const Math::Color mark = overBtn ? INK : WHITE;

			if (sounding)
			{
				// 停止(一時停止)の印。縦棒2本
				sp.DrawBox(bx - 4, by, 2, 7, &mark, true);
				sp.DrawBox(bx + 4, by, 2, 7, &mark, true);
			}
			else
			{
				sp.DrawTriangle(bx - 5, by - 7, bx - 5, by + 7, bx + 7, by, &mark, true);
			}
		}
	}

	// ══ SVGデコ(z5, 最前面) ══
	// 十字(小さな+)：点滅
	auto blinkCol = [&](float phase) { Math::Color c = INK; c.w = 0.35f + 0.65f * std::fabs(std::sin(HjUI::Time() * 3.0f + phase)); return c; };
	DrawCross(648.0f, 80.0f, 8.0f, 2.5f, blinkCol(0.0f));
	DrawCross(684.0f, 92.0f, 8.0f, 2.5f, blinkCol(1.0f));
	DrawCross(968.0f, 44.0f, 8.0f, 2.5f, blinkCol(2.0f));
	// ドットフィールド群(明滅アニメ)
	HjUI::DotFieldTwinkle(700.0f, 150.0f, 70.0f, 42.0f, DOTS, 0.0f);
	HjUI::DotFieldTwinkle(1170.0f, 120.0f, 150.0f, 110.0f, DOTS, 0.7f);
	HjUI::DotFieldTwinkle(1330.0f, 120.0f, 90.0f, 110.0f, GREY9, 1.4f);
	HjUI::DotFieldTwinkle(70.0f,  470.0f, 90.0f, 70.0f, DOTS, 2.1f);
	HjUI::DotFieldTwinkle(840.0f, 720.0f, 180.0f, 46.0f, DOTS, 2.8f);
	HjUI::DotFieldTwinkle(1150.0f, 712.0f, 150.0f, 14.0f, DOTS, 3.5f);
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

	// 名前入力のダイアログ。最後に描く＝画面のどれよりも手前に出す
	DrawNameDialog();
}

//----------------------------------------------------------
// 左上の銘板。
//
// 走行記録が増えるほど内容が変わる。固定の飾りのままだと、
// 遊んでも画面が何も変わらず「積み上がっている感じ」が出ない。
//   走る前 … EST. 2024(創業表記のまま)
//   走った後 … これまでの走行回数
//----------------------------------------------------------
void TitleMenuUI::DrawPlate()
{
	using namespace UIConst;

	DrawText(FontStrip, 92.0f, 44.0f, 11.0f, "BUILT TO SLIDE.", INK);

	// バージョン表記。走行記録は右上のバッジへ集約したので、ここは固定。
	// 走行前後で内容が入れ替わると、この2行の役割が定まらない。
	DrawText(FontStrip, 92.0f, 62.0f, 11.0f, GameVersion, INK);

	DrawLineD(92.0f, 84.0f, 235.0f, 84.0f, 2.0f, INK);
}

//----------------------------------------------------------
// 右上のバッジ(名前 / レベル)。
//
// 数字は累計スコアから求めた実データ。
// 下辺に次のレベルまでの進み具合を細い帯で敷く。
// 数字だけだと「あとどれくらいで上がるのか」が分からない。
//----------------------------------------------------------
void TitleMenuUI::DrawBadge()
{
	using namespace UIConst;
	const auto& prof = HjPlayerProfile::Instance();

	// 押し分ける。名前側は「名前を変える」、レベル側は「記録を見る」。
	// バッジ全体で1つの操作にすると、どちらが起きるのか予想できない。
	const bool nameHover = HjUI::Hover(TitleBadgeNameX, TitleBadgeY, TitleBadgeNameW, TitleBadgeH);
	const bool lvHover   = HjUI::Hover(TitleBadgeLvX, TitleBadgeY, TitleBadgeLvW, TitleBadgeH);

	if (!m_nameEditing && HjUI::Clicked(TitleBadgeNameX, TitleBadgeY, TitleBadgeNameW, TitleBadgeH))
	{
		// 編集は今の名前から始める。空欄から打ち直させると、
		// 少し直したいだけのときに全部打つことになる。
		m_nameBuf     = prof.GetName();
		m_nameEditing = true;
		// 開いたクリックが直後の入力として拾われないようにする
		HjKeyInput::Instance().ConsumeAll();
	}
	if (HjUI::Clicked(TitleBadgeLvX, TitleBadgeY, TitleBadgeLvW, TitleBadgeH))
	{
		m_statsOpen    = !m_statsOpen;
		m_badgeClicked = true;
	}

	// 名前側。編集中は枠を強調し、末尾にカーソルを出す
	DrawRectTL(TitleBadgeNameX, TitleBadgeY, TitleBadgeNameW, TitleBadgeH,
	           (m_nameEditing || nameHover) ? ACID : PAPER, true);
	DrawFrameTL(TitleBadgeNameX, TitleBadgeY, TitleBadgeNameW, TitleBadgeH, 2.0f, INK);

	// 編集中の文字は中央のダイアログへ出す。ここは今の名前のまま。
	// 名前は日本語・中国語が入りうるので、CJKのグリフを持つ書体で描く
	DrawText(FontCJKSmall, TitleBadgeNameX + 14.0f, TitleBadgeY + 9.0f, 12.0f,
	         prof.GetName().c_str(), INK);

	// レベル側は常にアシッド塗り(この画面で一番の見せ場なので固定で強く出す)
	DrawRectTL(TitleBadgeLvX, TitleBadgeY, TitleBadgeLvW, TitleBadgeH, ACID, true);
	DrawFrameTL(TitleBadgeLvX, TitleBadgeY, TitleBadgeLvW, TitleBadgeH,
	            lvHover ? 3.0f : 2.0f, INK);

	char lv[16];
	snprintf(lv, sizeof(lv), "LV.%d", prof.GetLevel());
	DrawText(FontSmall, TitleBadgeLvX + 14.0f, TitleBadgeY + 9.0f, 12.0f, lv, INK);

	// 次のレベルまでの進み具合。バッジの下辺に敷く
	{
		const float gy = TitleBadgeY + TitleBadgeH - TitleBadgeGaugeH;
		Math::Color track = INK; track.w = 0.20f;
		DrawRectTL(TitleBadgeAllX, gy, TitleBadgeAllW, TitleBadgeGaugeH, track, true);
		DrawRectTL(TitleBadgeAllX, gy, TitleBadgeAllW * prof.GetLevelProgress(), TitleBadgeGaugeH, INK, true);
	}

	if (!m_statsOpen) { return; }

	// 押したときだけ出す記録の内訳。
	// 常時出すと上部が数字だらけになるので、見たいときだけ開く。
	const float px = TitleBadgeAllX - 60.0f, py = TitleBadgeY + TitleBadgeH + 10.0f;
	const float pw = TitleBadgeAllW + 60.0f, ph = 96.0f;
	DrawRectTL(px, py, pw, ph, PAPER, true);
	DrawFrameTL(px, py, pw, ph, 2.0f, INK);

	char buf[64];
	snprintf(buf, sizeof(buf), "TOTAL  %d", static_cast<int>(prof.GetTotalScore()));
	DrawText(FontSmall, px + 14.0f, py + 14.0f, 12.0f, buf, INK);
	snprintf(buf, sizeof(buf), "BEST   %d", static_cast<int>(prof.GetBestScore()));
	DrawText(FontSmall, px + 14.0f, py + 38.0f, 12.0f, buf, INK);
	snprintf(buf, sizeof(buf), "RUNS   %d", prof.GetRunCount());
	DrawText(FontSmall, px + 14.0f, py + 62.0f, 12.0f, buf, INK);
}

//----------------------------------------------------------
// 名前の編集中の文字入力。
//
// 英数字と一部の記号だけを受け付ける。
// 保存は「key value」の簡易テキストなので、空白が入ると読み込みで崩れる。
// 弾く場所を入力側に置けば、保存側で気にしなくて済む。
//----------------------------------------------------------
void TitleMenuUI::UpdateNameEdit()
{
	auto& key = HjKeyInput::Instance();

	// ESC=取り消し。打った内容は捨てて元の名前のまま
	if (key.Pressed(VK_ESCAPE))
	{
		m_nameEditing = false;
		return;
	}

	// ENTER=確定
	if (key.Pressed(VK_RETURN))
	{
		HjPlayerProfile::Instance().SetName(m_nameBuf);
		HjPlayerProfile::Instance().Save();
		m_nameEditing = false;
		return;
	}

	if (key.Pressed(VK_BACK) && !m_nameBuf.empty())
	{
		// UTF-8 は1文字が複数バイトなので、後ろの継続バイトごと削る。
		// 1バイトだけ消すと文字が壊れて表示できなくなる。
		while (!m_nameBuf.empty())
		{
			const unsigned char c = static_cast<unsigned char>(m_nameBuf.back());
			m_nameBuf.pop_back();
			// 継続バイト(10xxxxxx)以外まで消したら1文字ぶん
			if ((c & 0xC0) != 0x80) { break; }
		}
		return;
	}

	// IMEで確定した文字をそのまま受け取る。
	// 仮想キーコードから組み立てる方式だと、日本語や中国語を拾えない。
	const std::string& typed = key.TypedText();
	if (typed.empty()) { return; }

	for (char c : typed)
	{
		// 空白は入れない。保存が「key value」の簡易テキストなので、
		// 空白が入ると読み込みで名前が途中で切れる。
		if (c == ' ' || c == '	') { continue; }

		if (CountUtf8Chars(m_nameBuf) >= PlayerConst::MaxNameLen) { break; }
		m_nameBuf.push_back(c);
	}
}

//----------------------------------------------------------
// 名前入力のダイアログ(画面中央)。
//
// バッジの中で直接打たせると枠が狭く、今どこまで打ったか見えない。
// 背景を暗く落として中央に大きく出し、入力だけに集中できるようにする。
// 真っ暗にはしない。元の画面が見えないと、どこから来たのか分からなくなる。
//----------------------------------------------------------
void TitleMenuUI::DrawNameDialog()
{
	using namespace UIConst;
	if (!m_nameEditing) { return; }

	// 背景を落とす
	{
		Math::Color dim = INK; dim.w = NameDlgDim;
		DrawRectTL(0.0f, 0.0f, DesignW, DesignH, dim, true);
	}

	DrawRectTL(NameDlgX, NameDlgY, NameDlgW, NameDlgH, PAPER, true);
	DrawFrameTL(NameDlgX, NameDlgY, NameDlgW, NameDlgH, 2.0f, INK);

	DrawText(FontSmall, NameDlgX + NameDlgPadX, NameDlgY + NameDlgCapDy, 12.0f,
	         "DRIVER NAME", SUBTXT);

	// 入力中の文字＋点滅カーソル。
	// カーソルが無いと、打てる状態なのかどうかが分からない
	const bool caretOn = std::fmodf(HjUI::Time() * 2.0f, 1.0f) < 0.5f;
	const std::string shown = m_nameBuf + (caretOn ? "_" : " ");
	// 日本語・中国語が入りうるのでCJK書体を使う。
	// 登録サイズ(47px)と出したい大きさを揃えてあるので、拡大は掛からない
	HjUI::TextScaled(FontCJK, NameDlgX + NameDlgPadX, NameDlgY + NameDlgValDy,
	                 NameDlgValPx, shown.c_str(), INK);

	// 打ち込む場所だと分かるよう、値の下に線を引く
	DrawLineD(NameDlgX + NameDlgPadX, NameDlgY + NameDlgRuleDy,
	          NameDlgX + NameDlgW - NameDlgPadX, NameDlgY + NameDlgRuleDy, 2.0f, INK);

	// ENTERとESCの役割が分からないと確定できない
	char hint[64];
	snprintf(hint, sizeof(hint), "ENTER OK  /  ESC CANCEL  /  MAX %d", PlayerConst::MaxNameLen);
	DrawText(FontTiny, NameDlgX + NameDlgPadX, NameDlgY + NameDlgHintDy, 10.0f, hint, SUBTXT);
}

//----------------------------------------------------------
// UTF-8 の文字数を数える。
// バイト数で上限を見ると、日本語は1文字3バイトなので
// 数文字しか打てなくなる。継続バイト(10xxxxxx)を除いて数える。
//----------------------------------------------------------
int TitleMenuUI::CountUtf8Chars(const std::string& s)
{
	int n = 0;
	for (char c : s)
	{
		if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) { ++n; }
	}
	return n;
}
