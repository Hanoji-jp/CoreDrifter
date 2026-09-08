#include "HjUI.h"
#include "../../main.h"   // Application::GetWindowHandle()(マウス座標変換用)

namespace
{
	// スプライトシェーダーの短縮取得
	KdSpriteShader& SP() { return KdShaderManager::Instance().m_spriteShader; }

	// マウス状態(デザイン座標＋左クリックのエッジ)
	float s_mx = 0.0f, s_my = 0.0f;
	bool  s_prevDown = false, s_clicked = false;

	// アニメ用の共有クロック(秒)
	float s_time = 0.0f;
}

namespace HjUI
{
	void BeginInput()
	{
		HWND hwnd = Application::Instance().GetWindowHandle();
		POINT pt{};
		if (hwnd && GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
		{
			RECT rc{};
			GetClientRect(hwnd, &rc);
			const float cw = static_cast<float>(rc.right - rc.left);
			const float ch = static_cast<float>(rc.bottom - rc.top);
			// クライアント画素→デザイン座標(1536x864)。DPI/ウィンドウ拡大に依存しない。
			if (cw > 0.0f && ch > 0.0f)
			{
				s_mx = static_cast<float>(pt.x) * UIConst::DesignW / cw;
				s_my = static_cast<float>(pt.y) * UIConst::DesignH / ch;
			}
		}
		const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		s_clicked = down && !s_prevDown;   // 押した瞬間だけ
		s_prevDown = down;
	}

	float MouseX() { return s_mx; }
	float MouseY() { return s_my; }
	bool  Hover(float dx, float dy, float w, float h) { return s_mx >= dx && s_mx < dx + w && s_my >= dy && s_my < dy + h; }
	bool  Clicked(float dx, float dy, float w, float h) { return s_clicked && Hover(dx, dy, w, h); }

	// ══════════════ テキスト ══════════════
	float Text(int fontId, float dx, float dy, float pxH, const char* str, const Math::Color& col)
	{
		auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
		if (!sprite || sprite->GetTexList().empty()) { return MapX(dx); }
		// DrawFontはセル高ぶん上へ描く。em高(上寄り)とセル高(下寄り)の中点で下げると上端が概ねdyに揃う。
		float gh = pxH;
		if (sprite->GetTexList()[0]->FontTex)
		{
			const float cell = static_cast<float>(sprite->GetTexList()[0]->FontTex->GetInfo().Height);
			gh = (pxH + cell) * 0.5f;
		}
		const Math::Vector2 pos = { MapX(dx), MapY(dy) - gh };
		SP().DrawFont(sprite, pos, &col, 0);
		// 右端X = 左 + 総幅
		float w = 0.0f;
		for (auto& d : sprite->GetTexList()) { if (d && d->FontTex) { w += static_cast<float>(d->FontTex->GetInfo().Width); } }
		return MapX(dx) + w;
	}

	float TextTracked(int fontId, float dx, float dy, float pxH, const char* str, const Math::Color& col, float trackDesign)
	{
		auto& fm = KdFontManager::Instance();
		float x = MapX(dx);
		const float track = trackDesign * UIConst::Scale;
		float yb = MapY(dy) - pxH;
		{
			auto probe = fm.CreateFontTexture(fontId, "M", 3);
			if (probe && !probe->GetTexList().empty() && probe->GetTexList()[0]->FontTex)
			{
				const float cell = static_cast<float>(probe->GetTexList()[0]->FontTex->GetInfo().Height);
				yb = MapY(dy) - (pxH + cell) * 0.5f;
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
				SP().DrawFont(s, pos, &col, 0);
				w = static_cast<float>(s->GetTexList()[0]->FontTex->GetInfo().Width);
			}
			x += w + track;
		}
		return x - track;
	}

	void TextC(int fontId, float dCx, float dy, float pxH, const char* str, const Math::Color& col)
	{
		const float w = Measure(fontId, str, 0.0f) / UIConst::Scale;   // デザインpx幅
		Text(fontId, dCx - w * 0.5f, dy, pxH, str, col);
	}

	//----------------------------------------------------------
	// 中心基準・スケール付きの縁取り文字。
	// 3D画面の上に文字を置くと、背景の明暗が毎フレーム変わって読めなくなる。
	// 縁色を8方向へずらして描いてから本体を重ねると、背景に関係なく形が立つ。
	// ずらし量はスケールに比例させること。固定にすると、
	// 大きく出したときに縁が細くなって効かなくなる。
	//----------------------------------------------------------
	void TextCenteredScaledOutline(int fontId, float dCx, float dCy, float scale, const char* str,
		const Math::Color& edge, const Math::Color& fill)
	{
		const float o = UIConst::OutlineOffset * scale;
		const float off[8][2] = { {-o,0},{o,0},{0,-o},{0,o},{-o,-o},{o,-o},{-o,o},{o,o} };
		for (int i = 0; i < 8; ++i)
		{
			TextCenteredScaled(fontId, dCx + off[i][0], dCy + off[i][1], scale, str, edge);
		}
		TextCenteredScaled(fontId, dCx, dCy, scale, str, fill);
	}

	void TextOutline(int fontId, float dx, float dy, float pxH, const char* str,
		const Math::Color& edge, const Math::Color& fill)
	{
		// 縁色を8方向にずらして描き、その上に塗り色で本体を描く＝縁だけ残る(中空)。
		const float o = 2.5f;   // デザインpxのずらし量
		const float off[8][2] = { {-o,0},{o,0},{0,-o},{0,o},{-o,-o},{o,-o},{-o,o},{o,o} };
		for (int i = 0; i < 8; ++i) { Text(fontId, dx + off[i][0], dy + off[i][1], pxH, str, edge); }
		Text(fontId, dx, dy, pxH, str, fill);
	}

	//----------------------------------------------------------
	// 文字テクスチャの実際の高さ。
	// 指定サイズ(FontPx)とは違い、上下の張り出しを含んだ実寸になる。
	// 焼いた画像の縦横比を求めるのに要る。
	//----------------------------------------------------------
	float MeasureHeight(int fontId, const char* str)
	{
		auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
		if (!sprite) { return 0.0f; }

		float h = 0.0f;
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex) { continue; }
			h = std::max(h, static_cast<float>(d->FontTex->GetInfo().Height));
		}
		return h;
	}

	float Measure(int fontId, const char* str, float trackDesign)
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

	void TextCenteredScaled(int fontId, float dCx, float dCy, float scale, const char* str, const Math::Color& col)
	{
		auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
		if (!sprite || sprite->GetTexList().empty()) { return; }
		// 総幅(素の画素)を測る
		float total = 0.0f;
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex || d->Code == '\n') { continue; }
			total += static_cast<float>(d->FontTex->GetInfo().Width);
		}
		const float cx = MapX(dCx), cy = MapY(dCy);
		float x = cx - total * scale * 0.5f;   // 左端(screen)
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex || d->Code == '\n') { continue; }
			const float W = static_cast<float>(d->FontTex->GetInfo().Width);
			const float H = static_cast<float>(d->FontTex->GetInfo().Height);
			const int gw = static_cast<int>(W * scale), gh = static_cast<int>(H * scale);
			const int gxc = static_cast<int>(x + W * scale * 0.5f);
			SP().DrawTex(d->FontTex.get(), gxc, static_cast<int>(cy), gw, gh, nullptr, &col, { 0.5f, 0.5f });
			x += W * scale;
		}
	}

	void TextRotated(int fontId, float dCx, float dCy, const char* str, const Math::Color& col, float trackDesign)
	{
		auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
		if (!sprite || sprite->GetTexList().empty()) { return; }
		const float track = trackDesign * UIConst::Scale;
		float total = 0.0f;
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex || d->Code == '\n') { continue; }
			total += static_cast<float>(d->FontTex->GetInfo().Width) + track;
		}
		const float cx = MapX(dCx);
		float cursor = MapY(dCy) + total * 0.5f;
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex || d->Code == '\n') { continue; }
			const float W = static_cast<float>(d->FontTex->GetInfo().Width);
			const float H = static_cast<float>(d->FontTex->GetInfo().Height);
			const float gcy = cursor - W * 0.5f;
			const std::vector<KdSpriteShader::Vertex> verts = {
				{ { cx - H * 0.5f, gcy + W * 0.5f, 0.0f }, { 0.0f, 1.0f } },
				{ { cx + H * 0.5f, gcy + W * 0.5f, 0.0f }, { 0.0f, 0.0f } },
				{ { cx - H * 0.5f, gcy - W * 0.5f, 0.0f }, { 1.0f, 1.0f } },
				{ { cx + H * 0.5f, gcy - W * 0.5f, 0.0f }, { 1.0f, 0.0f } },
			};
			SP().DrawTexVertices(d->FontTex.get(), verts, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, &col);
			cursor -= (W + track);
		}
	}

	// ══════════════ 図形 ══════════════
	//----------------------------------------------------------
	// 矩形。DrawBoxは「中心と半分の大きさ」を整数で取る。
	//
	// 半分の大きさを切り捨てると、細い物ほど割合として大きく痩せる。
	// 幅5(デザイン)は 5×0.8333÷2 = 2.08 → 2 になり、実際は4px＝2割細い。
	// 2pxの罫線に至っては 0.83 → 0 で消えてしまう。
	// 四捨五入したうえで、最低1pxは残す。
	//----------------------------------------------------------
	void TexRectTL(const KdTexture* tex, float dx, float dy, float w, float h, const Math::Color& col)
	{
		if (!tex) { return; }

		const int ws = std::max(1, static_cast<int>(w * UIConst::Scale + 0.5f));
		const int hs = std::max(1, static_cast<int>(h * UIConst::Scale + 0.5f));

		SP().DrawTex(tex,
		             static_cast<int>(MapX(dx + w * 0.5f) + 0.5f),
		             static_cast<int>(MapY(dy + h * 0.5f) + 0.5f),
		             ws, hs, nullptr, &col);
	}

	void RectTL(float dx, float dy, float w, float h, const Math::Color& col, bool fill)
	{
		const int hw = std::max(1, static_cast<int>(w * UIConst::Scale * 0.5f + 0.5f));
		const int hh = std::max(1, static_cast<int>(h * UIConst::Scale * 0.5f + 0.5f));
		SP().DrawBox(static_cast<int>(MapX(dx + w * 0.5f) + 0.5f),
			static_cast<int>(MapY(dy + h * 0.5f) + 0.5f), hw, hh, &col, fill);
	}

	void FrameTL(float dx, float dy, float w, float h, float px, const Math::Color& col)
	{
		const float left = MapX(dx), top = MapY(dy);
		const float ws = w * UIConst::Scale, hs = h * UIConst::Scale, t = px * 0.5f;
		// 枠の太さも切り捨てると消える。最低1pxは残す
		const int ti = std::max(1, static_cast<int>(t + 0.5f));
		const int hw = std::max(1, static_cast<int>(ws * 0.5f + 0.5f));
		const int hh = std::max(1, static_cast<int>(hs * 0.5f + 0.5f));
		SP().DrawBox(static_cast<int>(left + ws * 0.5f), static_cast<int>(top - t),      hw, ti, &col, true);
		SP().DrawBox(static_cast<int>(left + ws * 0.5f), static_cast<int>(top - hs + t), hw, ti, &col, true);
		SP().DrawBox(static_cast<int>(left + t),         static_cast<int>(top - hs * 0.5f), ti, hh, &col, true);
		SP().DrawBox(static_cast<int>(left + ws - t),    static_cast<int>(top - hs * 0.5f), ti, hh, &col, true);
	}

	void LineD(float x1, float y1, float x2, float y2, float px, const Math::Color& col)
	{
		const int n = std::max(1, static_cast<int>(px));
		for (int i = 0; i < n; ++i)
		{
			const float o = static_cast<float>(i) - (n - 1) * 0.5f;
			SP().DrawLine(static_cast<int>(MapX(x1)), static_cast<int>(MapY(y1) + o), static_cast<int>(MapX(x2)), static_cast<int>(MapY(y2) + o), &col);
			SP().DrawLine(static_cast<int>(MapX(x1) + o), static_cast<int>(MapY(y1)), static_cast<int>(MapX(x2) + o), static_cast<int>(MapY(y2)), &col);
		}
	}

	void Vignette(float strength)
	{
		if (strength <= 0.001f) { return; }
		const int   N = 14;
		const float band = 130.0f;                 // 各エッジのグラデ幅(デザインpx)
		const float th   = band / N + 1.0f;        // 1層の厚み
		Math::Color c = UIConst::INK;
		for (int i = 0; i < N; ++i)
		{
			const float t = 1.0f - static_cast<float>(i) / N;   // 端で1・内側で0
			c.w = strength * t * 0.13f;
			const float o = static_cast<float>(i) / N * band;   // 内側へのオフセット
			RectTL(0.0f, o, UIConst::DesignW, th, c, true);                                  // 上
			RectTL(0.0f, UIConst::DesignH - o - th, UIConst::DesignW, th, c, true);          // 下
			RectTL(o, 0.0f, th, UIConst::DesignH, c, true);                                  // 左
			RectTL(UIConst::DesignW - o - th, 0.0f, th, UIConst::DesignH, c, true);          // 右
		}
	}

	void DotField(float dx, float dy, float w, float h, const Math::Color& col)
	{
		const float left = MapX(dx), top = MapY(dy);
		const float ws = w * UIConst::Scale, hs = h * UIConst::Scale;
		for (float yy = top; yy >= top - hs; yy -= UIConst::DotCell)
		{
			for (float xx = left; xx <= left + ws; xx += UIConst::DotCell)
			{
				SP().DrawCircle(static_cast<int>(xx), static_cast<int>(yy), 2, &col, true);
			}
		}
	}

	void RingD(float cx, float cy, float r, const Math::Color& col)
	{
		SP().DrawCircle(static_cast<int>(MapX(cx)), static_cast<int>(MapY(cy)), static_cast<int>(r * UIConst::Scale), &col, false);
	}

	void DotFieldTwinkle(float dx, float dy, float w, float h, const Math::Color& base, float phase)
	{
		// 位置で位相をずらして「波」をフィールド内に伝播させる(斜めに流れる)。
		// 各ドットごとに大きさ・濃さが時間差でピークになる＝波打って見える。
		const float left = MapX(dx), top = MapY(dy);
		const float ws = w * UIConst::Scale, fh = h * UIConst::Scale;
		const float kx = 0.020f, ky = 0.020f;   // 波数(小さい=波長が長い=大きなうねり)
		// float寸法の四角クアッドで描く＝サイズを連続的に(滑らかに)変えられる。
		const KdTexture* white = KdDirect3D::Instance().GetWhiteTex().get();
		for (float yy = top; yy >= top - fh; yy -= UIConst::DotCell)
		{
			for (float xx = left; xx <= left + ws; xx += UIConst::DotCell)
			{
				const float t = 0.5f + 0.5f * std::sin(s_time * 1.1f - xx * kx - yy * ky + phase);
				Math::Color c = base;
				c.w = base.w * (0.12f + 0.88f * t);
				const float hs = 0.7f + t * 0.8f;   // 半分サイズ 0.7〜1.5(小さめ・滑らか)
				if (white)
				{
					const std::vector<KdSpriteShader::Vertex> v = {
						{ { xx - hs, yy - hs, 0.0f }, { 0.0f, 1.0f } },
						{ { xx - hs, yy + hs, 0.0f }, { 0.0f, 0.0f } },
						{ { xx + hs, yy - hs, 0.0f }, { 1.0f, 1.0f } },
						{ { xx + hs, yy + hs, 0.0f }, { 1.0f, 0.0f } },
					};
					SP().DrawTexVertices(white, v, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, &c);
				}
			}
		}
	}

	void DotDissolve(float dx, float dy, float w, float h, const Math::Color& dot, float phase)
	{
		// 密な角丸ボックス網。各ドットのサイズを波で0↔最大に(谷で消える=溶ける)。斜めに流れる。
		const float cell = 42.0f;                // ドット間隔(大きめ=数を減らして1個をでかく)
		const float left = MapX(dx), top = MapY(dy);
		const float ws = w * UIConst::Scale, fh = h * UIConst::Scale;
		const float kx = 0.0065f, ky = 0.0065f;  // 小さい=波長が長い(スパン=緑もフェードも長く)
		for (float yy = top; yy >= top - fh; yy -= cell)
		{
			for (float xx = left; xx <= left + ws; xx += cell)
			{
				// 位相はセル中心で評価(左上基準だと成長が偏ってガタつく)
				const float ccx = xx + cell * 0.5f, ccy = yy - cell * 0.5f;
				const float raw = 0.5f + 0.5f * std::sin(s_time * 0.95f - ccx * kx - ccy * ky + phase);
				// sqrtで低い所を持ち上げる＝緑(山)を長く。連続カーブなのでサイズのフェードは残る。
				const float t = std::sqrt(raw);
				// 最大時に半サイズ=セル/2(+少し)＝上下左右が接する。角丸なので角だけ開く。
				const float hs = t * (cell * 0.5f + 1.0f);
				if (hs < 1.0f) { continue; }
				// 中心を整数グリッドに固定し、そこからサイズだけ左右対称に広げる。
				// (毎フレーム左上を丸め直すと偶奇跨ぎで中心が0.5pxジッター=ガタつく)
				const int icx = static_cast<int>(ccx), icy = static_cast<int>(ccy);
				const int isz = static_cast<int>(hs + 0.5f);
				const int bx = icx - isz / 2;
				const int by = icy - isz / 2;
				Math::Color c = dot; c.w = dot.w * t;
				SP().DrawRoundedBox(bx, by, isz, isz, hs * 0.45f, &c, 6);
			}
		}
	}

	// 下で定義する拡大描画を先に使うための宣言
	void  TextScaled (int fontId, float dx, float dy, float targetPx, const char* str, const Math::Color& col);
	void  TextScaledR(int fontId, float dRightX, float dy, float targetPx, const char* str, const Math::Color& col);
	void  TextScaledC(int fontId, float dCx, float dy, float targetPx, const char* str, const Math::Color& col);

	//----------------------------------------------------------
	// 大きさを指定しない入口。
	// pxH はフォントIDから引く。呼ぶ側が数字を書かないので食い違わない。
	//----------------------------------------------------------
	// ※Text() は縦位置を「指定サイズと文字テクスチャの高さの中点」で決めるので、
	//   フォントによって上端の揃い方が変わる。大きいフォントほどずれが大きい。
	//   そこで拡大文字と同じ経路(グリフ単位の描画)へ通し、
	//   どのフォントでも上端が dy に来るようにする。
	float TextAt(int fontId, float dx, float dy, const char* str, const Math::Color& col)
	{
		TextScaled(fontId, dx, dy, UIConst::FontPx(fontId), str, col);
		return MapX(dx) + Measure(fontId, str, 0.0f);
	}

	void TextAtC(int fontId, float dCx, float dy, const char* str, const Math::Color& col)
	{
		TextScaledC(fontId, dCx, dy, UIConst::FontPx(fontId), str, col);
	}

	void TextAtR(int fontId, float dRightX, float dy, const char* str, const Math::Color& col)
	{
		TextScaledR(fontId, dRightX, dy, UIConst::FontPx(fontId), str, col);
	}

	float TextAtTracked(int fontId, float dx, float dy, const char* str,
		const Math::Color& col, float trackDesign)
	{
		return TextTracked(fontId, dx, dy, UIConst::FontPx(fontId), str, col, trackDesign);
	}

	//----------------------------------------------------------
	// フォントに無い大きさで出す。左上を(dx,dy)に置いて拡大縮小する。
	// 大きな数字のためだけにフォントを増やすと、その分だけ
	// テクスチャを持つことになる。1枚を伸ばす方が軽い。
	//----------------------------------------------------------
	void TextScaled(int fontId, float dx, float dy, float targetPx,
		const char* str, const Math::Color& col)
	{
		auto sprite = KdFontManager::Instance().CreateFontTexture(fontId, str, 3);
		if (!sprite || sprite->GetTexList().empty()) { return; }

		const float scale = targetPx / std::max(UIConst::FontPx(fontId), 1.0f);

		float x = MapX(dx);
		const float top = MapY(dy);

		// 文字テクスチャは指定サイズより縦に余白ぶん大きい。
		// テクスチャの上端を dy に合わせると、実際の文字は余白のぶん下へずれ、
		// 大きい文字ほどずれ幅が大きくなる。
		// そこで「指定サイズの箱」の上端が dy に来るように置く。
		const float boxH = targetPx * UIConst::Scale;   // 画面px
		for (auto& d : sprite->GetTexList())
		{
			if (!d || !d->FontTex || d->Code == '\n') { continue; }

			const float W = static_cast<float>(d->FontTex->GetInfo().Width);
			const float H = static_cast<float>(d->FontTex->GetInfo().Height);
			const int gw = static_cast<int>(W * scale);
			const int gh = static_cast<int>(H * scale);

			SP().DrawTex(d->FontTex.get(),
				static_cast<int>(x + W * scale * 0.5f),
				static_cast<int>(top - boxH * 0.5f),
				gw, gh, nullptr, &col, { 0.5f, 0.5f });
			x += W * scale;
		}
	}

	//----------------------------------------------------------
	// 拡大した文字を右端・中央へ合わせる。
	// 伸ばした後の幅は「元の幅 × 倍率」。呼ぶ側で毎回計算すると
	// 倍率の掛け忘れが起きるので、ここにまとめる。
	//----------------------------------------------------------
	static float ScaledWidth(int fontId, const char* str, float targetPx)
	{
		const float scale = targetPx / std::max(UIConst::FontPx(fontId), 1.0f);
		return Measure(fontId, str, 0.0f) / UIConst::Scale * scale;
	}

	void TextScaledR(int fontId, float dRightX, float dy, float targetPx,
		const char* str, const Math::Color& col)
	{
		TextScaled(fontId, dRightX - ScaledWidth(fontId, str, targetPx), dy, targetPx, str, col);
	}

	void TextScaledC(int fontId, float dCx, float dy, float targetPx,
		const char* str, const Math::Color& col)
	{
		TextScaled(fontId, dCx - ScaledWidth(fontId, str, targetPx) * 0.5f, dy, targetPx, str, col);
	}

	//----------------------------------------------------------
	// 右端を合わせて描く。
	// 表の数値のように、頭ではなく右が揃う方が読みやすい場面で使う。
	//----------------------------------------------------------
	void TextR(int fontId, float dRightX, float dy, float pxH, const char* str, const Math::Color& col)
	{
		const float w = Measure(fontId, str, 0.0f) / UIConst::Scale;   // デザインpx幅
		Text(fontId, dRightX - w, dy, pxH, str, col);
	}

	// ══════════════ 図形(追加分) ══════════════
	void DiscD(float cx, float cy, float r, const Math::Color& col)
	{
		SP().DrawCircle(static_cast<int>(MapX(cx)), static_cast<int>(MapY(cy)),
			static_cast<int>(r * UIConst::Scale), &col, true);
	}

	//----------------------------------------------------------
	// 円弧。短い直線をつないで描く。
	// 分割数は半径と角度から決める。半径が大きいほど細かくしないと角張る。
	// ※デザイン座標はY下向きなので、画面上で時計回りに進むようYを反転する。
	//----------------------------------------------------------
	void ArcD(float cx, float cy, float r, float a0, float a1, float px, const Math::Color& col)
	{
		const float sweep = fabsf(a1 - a0);
		const int   seg   = std::clamp(static_cast<int>(r * sweep * 0.25f), 4, 128);

		float px0 = cx + cosf(a0) * r;
		float py0 = cy + sinf(a0) * r;
		for (int i = 1; i <= seg; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(seg);
			const float a = a0 + (a1 - a0) * t;
			const float x = cx + cosf(a) * r;
			const float y = cy + sinf(a) * r;
			LineD(px0, py0, x, y, px, col);
			px0 = x; py0 = y;
		}
	}

	//----------------------------------------------------------
	// 多角形の塗りつぶし(走査線)。
	//
	// 使える図形が矩形・線・円しかないので、形を横1本ずつの線に
	// 分けて塗る。1行ごとに輪郭との交点を求め、対になった区間を線で結ぶ。
	//
	// 矩形で塗らないのは、DrawBoxが「中心と半分の大きさ」を整数で取るため。
	// 高さ1の帯を作ろうとすると半分が0になって消える。
	//----------------------------------------------------------
	void PolyFillD(const float* xy, int count, const Math::Color& col)
	{
		if (!xy || count < 3) { return; }

		// 画面座標へ直しておく。1行ずつ塗るので、行は画面の1ピクセル
		std::vector<float> px(count), py(count);
		float top = 0.0f, bottom = 0.0f;
		for (int i = 0; i < count; ++i)
		{
			px[i] = MapX(xy[i * 2 + 0]);
			py[i] = MapY(xy[i * 2 + 1]);
			if (i == 0) { top = bottom = py[i]; }
			top    = std::max(top,    py[i]);
			bottom = std::min(bottom, py[i]);
		}

		// 画面座標はY上向きなので、上から下へ向かって減らしていく
		std::vector<float> cross;
		for (int y = static_cast<int>(top); y >= static_cast<int>(bottom); --y)
		{
			const float fy = static_cast<float>(y) + 0.5f;   // 行の中心で見る

			cross.clear();
			for (int i = 0; i < count; ++i)
			{
				const int j = (i + 1) % count;
				const float y0 = py[i], y1 = py[j];

				// この行をまたぐ辺だけが交点を持つ。
				// 片側だけを含める(>= と <)ことで、頂点をちょうど通るときに
				// 交点が二重に数えられるのを防ぐ
				if ((y0 <= fy && y1 > fy) || (y1 <= fy && y0 > fy))
				{
					const float t = (fy - y0) / (y1 - y0);
					cross.push_back(px[i] + (px[j] - px[i]) * t);
				}
			}

			if (cross.size() < 2) { continue; }
			std::sort(cross.begin(), cross.end());

			// 交点を2つずつ組にして、その間が図形の内側
			for (size_t k = 0; k + 1 < cross.size(); k += 2)
			{
				SP().DrawLine(static_cast<int>(cross[k]),     y,
				              static_cast<int>(cross[k + 1]), y, &col);
			}
		}
	}

	void PolylineD(const float* xy, int count, float px, const Math::Color& col)
	{
		if (!xy || count < 2) { return; }
		for (int i = 1; i < count; ++i)
		{
			LineD(xy[(i - 1) * 2], xy[(i - 1) * 2 + 1], xy[i * 2], xy[i * 2 + 1], px, col);
		}
	}

	//----------------------------------------------------------
	// 状態チップ。
	// 点いていればアシッド塗り、消えていれば細い枠だけ。
	// 面と線の差で見せるので、色を増やさずに状態を表せる。
	//----------------------------------------------------------
	float Chip(float dx, float dy, const char* label, bool hot, const Math::Color& ink)
	{
		const float w = Measure(UIConst::FontTiny, label, 0.0f) / UIConst::Scale
		              + UIConst::ChipPadX * 2.0f;

		if (hot) { RectTL(dx, dy, w, UIConst::ChipH, UIConst::ACID); }
		else     { FrameTL(dx, dy, w, UIConst::ChipH, 2.0f, ink); }

		TextAt(UIConst::FontTiny, dx + UIConst::ChipPadX,
			dy + UIConst::CenterInBox(UIConst::ChipH, UIConst::FontPx(UIConst::FontTiny)),
			label, hot ? UIConst::INK : ink);
		return w;
	}

	//----------------------------------------------------------
	// 表の見出し帯。
	// 列の左端は個別に指定する。等分割にすると、
	// 短い列(PINGなど)に無駄な幅が付いて全体が間延びする。
	//----------------------------------------------------------
	void TableHead(float dx, float dy, float w, const char* const* cols,
		const float* colX, int count)
	{
		if (!cols || !colX || count <= 0) { return; }

		RectTL(dx, dy, w, UIConst::TableHeadH, UIConst::INK);
		for (int i = 0; i < count; ++i)
		{
			TextAt(UIConst::FontFoot, colX[i],
				dy + UIConst::CenterInBox(UIConst::TableHeadH, UIConst::FontPx(UIConst::FontFoot)),
				cols[i], UIConst::WHITE);
		}
	}

	// ══════════════ クリップ ══════════════
	void PushClip(float dx, float dy, float w, float h)
	{
		// ラスターpx = デザイン×Scale(デザインも画面も左上原点なのでそのまま)
		Math::Rectangle rc;
		rc.x = static_cast<long>(dx * UIConst::Scale);
		rc.y = static_cast<long>(dy * UIConst::Scale);
		rc.width  = static_cast<long>(w * UIConst::Scale);
		rc.height = static_cast<long>(h * UIConst::Scale);
		SP().SetScissorRect(rc);
	}
	void PopClip()
	{
		// SetScissorRectはCULL_BACK＋scissor有効に切替える。通常状態(CullNone・scissor無効)へ戻す。
		// (戻さないと後続のDrawBox等がカリングやクリップの影響を受ける)
		ID3D11RasterizerState* rs = KdDirect3D::Instance().CreateRasterizerState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);
		KdDirect3D::Instance().WorkDevContext()->RSSetState(rs);
		rs->Release();
	}

	namespace
	{
		// 4頂点の塗り四角(両巻きで描いてカリングを回避)
		void FillQuad(float ax, float ay, float bx, float by, float cx, float cy, float dx2, float dy2, const Math::Color& col)
		{
			const int a1 = static_cast<int>(ax), a2 = static_cast<int>(ay);
			const int b1 = static_cast<int>(bx), b2 = static_cast<int>(by);
			const int c1 = static_cast<int>(cx), c2 = static_cast<int>(cy);
			const int d1 = static_cast<int>(dx2), d2 = static_cast<int>(dy2);
			SP().DrawTriangle(a1, a2, b1, b2, c1, c2, &col, true);
			SP().DrawTriangle(a1, a2, c1, c2, d1, d2, &col, true);
			SP().DrawTriangle(a1, a2, c1, c2, b1, b2, &col, true);   // 逆巻き
			SP().DrawTriangle(a1, a2, d1, d2, c1, c2, &col, true);
		}
	}

	// ① 流れる斜めハザードストライプ
	void FxStripes(float dx, float dy, float w, float h, const Math::Color& col, float t)
	{
		PushClip(dx, dy, w, h);
		const float ccx = MapX(dx + w * 0.5f), ccy = MapY(dy + h * 0.5f);
		const float ws = w * UIConst::Scale, hs = h * UIConst::Scale;
		const float span = ws + hs;                       // 斜めに覆う長さ
		const float ux = 0.7071f, uy = -0.7071f;          // 帯の伸びる向き(右下)
		const float nx = 0.7071f, ny = 0.7071f;           // 帯の並ぶ向き(法線)
		const float period = 46.0f, thick = 24.0f;        // 間隔と太さ(=残り22pxが黒地)
		const float flow = std::fmod(t * 34.0f, period);
		const int   N = static_cast<int>(span / period) + 3;
		for (int k = -N; k <= N; ++k)
		{
			const float off = k * period + flow;
			const float px = ccx + nx * off, py = ccy + ny * off;
			const float hl = span, ht = thick * 0.5f;
			FillQuad(
				px + ux * hl + nx * ht, py + uy * hl + ny * ht,
				px - ux * hl + nx * ht, py - uy * hl + ny * ht,
				px - ux * hl - nx * ht, py - uy * hl - ny * ht,
				px + ux * hl - nx * ht, py + uy * hl - ny * ht, col);
		}
		PopClip();
	}

	// ② 奥→手前に流れるシェブロン矢印(">"の行を横スクロール)
	void FxChevrons(float dx, float dy, float w, float h, const Math::Color& col, float t)
	{
		PushClip(dx, dy, w, h);
		const char* row = ">   >   >   >   >   >";
		const float span = 150.0f;                        // 1周期の横幅(デザインpx)
		const float sx = dx - std::fmod(t * 80.0f, span);
		const float rowH = 46.0f;
		for (float ry = dy - 6.0f; ry <= dy + h; ry += rowH)
		{
			// 行ごとに位相をずらして奥行き感
			const float ox = std::fmod((ry - dy) * 0.6f, span);
			Text(UIConst::FontCard, sx + ox, ry, 30.0f, row, col);
		}
		PopClip();
	}

	// ③ 上下に走査する光の帯＋薄い走査線
	void FxScanBar(float dx, float dy, float w, float h, const Math::Color& col, float t)
	{
		PushClip(dx, dy, w, h);
		// 薄い走査線(横罫を等間隔)
		Math::Color faint = col; faint.w = 0.10f;
		for (float y = dy; y <= dy + h; y += 8.0f) { RectTL(dx, y, w, 1.0f, faint, true); }
		// 走査バー(上→下へ。手前に太い芯＋うしろに薄いにじみ)
		const float by = dy + std::fmod(t * 120.0f, h);
		Math::Color glow = col; glow.w = 0.20f;
		RectTL(dx, by - 14.0f, w, 28.0f, glow, true);
		RectTL(dx, by - 2.0f,  w, 4.0f,  col,  true);
		PopClip();
	}

	// ④ 背景に脈動する巨大タイポ(番号)
	void FxGiantType(float dx, float dy, float w, float h, const Math::Color& col, const char* label, float t)
	{
		PushClip(dx, dy, w, h);
		const float pulse = 0.5f + 0.5f * std::sin(t * 2.0f);
		Math::Color c = col; c.w = 0.12f + 0.16f * pulse;
		const float drift = std::sin(t * 0.8f) * 10.0f;
		// はみ出すほど大きく描いてクリップで切り取る
		Text(UIConst::FontHead, dx - 10.0f + drift, dy + h - 30.0f, h * 1.4f, label, c);
		PopClip();
	}

	// ── アニメーション ──
	void  Tick(float dt) { s_time += dt; }
	float Time() { return s_time; }

	void WaveLine(float dx, float dy, float len, float amp, float waves, float speed, float px, const Math::Color& col)
	{
		const int   N  = 48;
		const float k  = waves * 6.2831853f / len;   // 波数(ラジアン/デザインpx)
		const float ph = s_time * speed;             // 位相(時間で流れる)
		const int   n  = std::max(1, static_cast<int>(px));
		float px0 = MapX(dx);
		float py0 = MapY(dy + amp * std::sin(ph));
		for (int i = 1; i <= N; ++i)
		{
			const float t  = len * i / N;
			const float yy = dy + amp * std::sin(k * t + ph);
			const float px1 = MapX(dx + t);
			const float py1 = MapY(yy);
			for (int o = 0; o < n; ++o)   // 太さぶん重ね描き
			{
				const float off = static_cast<float>(o) - (n - 1) * 0.5f;
				SP().DrawLine(static_cast<int>(px0), static_cast<int>(py0 + off), static_cast<int>(px1), static_cast<int>(py1 + off), &col);
			}
			px0 = px1; py0 = py1;
		}
	}

	void WaveLineV(float dx, float dy, float len, float amp, float waves, float speed, float px, const Math::Color& col)
	{
		const int   N  = 48;
		const float k  = waves * 6.2831853f / len;
		const float ph = s_time * speed;
		const int   n  = std::max(1, static_cast<int>(px));
		float px0 = MapX(dx + amp * std::sin(ph));
		float py0 = MapY(dy);
		for (int i = 1; i <= N; ++i)
		{
			const float t  = len * i / N;
			const float xx = dx + amp * std::sin(k * t + ph);
			const float px1 = MapX(xx);
			const float py1 = MapY(dy + t);
			for (int o = 0; o < n; ++o)
			{
				const float off = static_cast<float>(o) - (n - 1) * 0.5f;
				SP().DrawLine(static_cast<int>(px0 + off), static_cast<int>(py0), static_cast<int>(px1 + off), static_cast<int>(py1), &col);
			}
			px0 = px1; py0 = py1;
		}
	}

	// ══════════════ ウィジェット ══════════════
	// 文字列を矩形の中央(縦)＋左パディングで置くための下端Y(デザイン)を返す簡便関数
	namespace { float CenterTextDy(float dy, float h, float pxH) { return dy + (h - pxH) * 0.5f; } }

	void Button(float dx, float dy, float w, float h, const char* label, BtnKind kind, const char* trailing)
	{
		using namespace UIConst;
		const float ty = CenterTextDy(dy, h, 15.0f);
		switch (kind)
		{
		case BtnKind::Primary:
			RectTL(dx, dy, w, h, ACID, true);
			Text(FontBtn, dx + 16.0f, ty, 15.0f, label, INK);
			break;
		case BtnKind::Secondary:
			FrameTL(dx, dy, w, h, 2.0f, INK);
			Text(FontBtn, dx + 16.0f, ty, 15.0f, label, INK);
			break;
		case BtnKind::Ghost:
			Text(FontBtn, dx + 16.0f, ty, 15.0f, label, INK);
			break;
		case BtnKind::Disabled:
		{
			Math::Color c = INK; c.w = 0.4f;
			FrameTL(dx, dy, w, h, 2.0f, c);
			Text(FontBtn, dx + 16.0f, ty, 15.0f, label, c);
			break;
		}
		}
		// 右端の添え文字(記号)を右寄せ
		if (trailing && trailing[0])
		{
			const float tw = Measure(FontBtn, trailing, 0.0f) / UIConst::Scale;
			Text(FontBtn, dx + w - 16.0f - tw, ty, 15.0f, trailing, INK);
		}
	}

	// 幾何アイコン(glyph: 0=▶ 1=＋ 2=✕ 3=⚙ 4=→)を正方の中心へ
	static void DrawGlyph(float scx, float scy, int glyph, const Math::Color& col)
	{
		auto& sp = SP();
		const int x = static_cast<int>(scx), y = static_cast<int>(scy);
		switch (glyph)
		{
		case 0: sp.DrawTriangle(x - 6, y - 8, x - 6, y + 8, x + 8, y, &col, true); break;               // ▶
		case 1: sp.DrawBox(x, y, 9, 2, &col, true); sp.DrawBox(x, y, 2, 9, &col, true); break;          // ＋
		case 2: HjUI::LineD(0, 0, 0, 0, 0, col);                                                        // (未使用)
			sp.DrawLine(x - 7, y - 7, x + 7, y + 7, &col); sp.DrawLine(x + 7, y - 7, x - 7, y + 7, &col); break; // ✕
		case 3: sp.DrawCircle(x, y, 8, &col, false); sp.DrawCircle(x, y, 3, &col, true); break;         // ⚙
		case 4: sp.DrawLine(x - 8, y, x + 6, y, &col); sp.DrawTriangle(x + 3, y - 5, x + 3, y + 5, x + 9, y, &col, true); break; // →
		default: break;
		}
	}

	void IconButton(float dx, float dy, float s, int glyph, int style)
	{
		using namespace UIConst;
		const float scx = MapX(dx + s * 0.5f), scy = MapY(dy + s * 0.5f);
		if (style == 0) { RectTL(dx, dy, s, s, INK, true);  DrawGlyph(scx, scy, glyph, WHITE); }
		else if (style == 2) { RectTL(dx, dy, s, s, ACID, true); DrawGlyph(scx, scy, glyph, INK); }
		else { FrameTL(dx, dy, s, s, 2.0f, INK); DrawGlyph(scx, scy, glyph, INK); }
	}

	void Toggle(float dx, float dy, bool on, const Math::Color& onFill)
	{
		using namespace UIConst;
		const float segW = 62.0f, h = 34.0f;
		FrameTL(dx, dy, segW * 2.0f, h, 2.0f, INK);
		const float ty = CenterTextDy(dy, h, 14.0f);
		const bool onIsInk = (onFill.x + onFill.y + onFill.z < 0.5f);   // 塗りが黒っぽいか
		// ON セグメント(有効時のみ塗り)
		if (on) { RectTL(dx + 2.0f, dy + 2.0f, segW - 2.0f, h - 4.0f, onFill, true); }
		TextC(FontBtn, dx + segW * 0.5f, ty, 14.0f, "ON", on ? (onIsInk ? WHITE : INK) : MUTE);
		// OFF セグメント(無効時のみ灰塗り)
		if (!on) { RectTL(dx + segW, dy + 2.0f, segW - 2.0f, h - 4.0f, GREY, true); }
		TextC(FontBtn, dx + segW * 1.5f, ty, 14.0f, "OFF", !on ? INK : MUTE);
	}

	void Tab(float dx, float dy, const char* label, bool active)
	{
		using namespace UIConst;
		const float w = Measure(FontTab, label, 0.0f) / UIConst::Scale + 28.0f;
		const float h = 34.0f;
		if (active) { RectTL(dx, dy, w, h, ACID, true); }
		Math::Color c = INK; if (!active) { c.w = 0.6f; }
		Text(FontTab, dx + 14.0f, CenterTextDy(dy, h, 17.0f), 17.0f, label, c);
	}

	void Badge2(float dx, float dy, const char* a, const char* b, const Math::Color& bBg)
	{
		using namespace UIConst;
		const float h = 30.0f;
		const float aw = Measure(FontSmall, a, 0.0f) / UIConst::Scale + 24.0f;
		const float bw = Measure(FontSmall, b, 0.0f) / UIConst::Scale + 24.0f;
		FrameTL(dx, dy, aw, h, 2.0f, INK);
		Text(FontSmall, dx + 12.0f, CenterTextDy(dy, h, 12.0f), 12.0f, a, INK);
		RectTL(dx + aw, dy, bw, h, bBg, true);
		FrameTL(dx + aw, dy, bw, h, 2.0f, INK);
		Text(FontSmall, dx + aw + 12.0f, CenterTextDy(dy, h, 12.0f), 12.0f, b, INK);
	}

	void StatBar(float dx, float dy, float w, const char* label, float value)
	{
		using namespace UIConst;
		Text(FontRow, dx, dy, 14.0f, label, INK);
		const float barDy = dy + 22.0f, barH = 16.0f;
		RectTL(dx, barDy, w, barH, INK, true);                              // 黒地
		RectTL(dx, barDy, w * std::clamp(value, 0.0f, 1.0f), barH, ACID, true); // アシッド充填
	}

	float Keycap(float dx, float dy, const char* key, const char* label, const Math::Color& col)
	{
		using namespace UIConst;
		const float pad = 10.0f;
		const float kw = Measure(FontRow, key, 0.0f) / UIConst::Scale + pad * 2.0f;
		const float kh = 30.0f;
		FrameTL(dx, dy, kw, kh, 2.0f, col);
		Text(FontRow, dx + pad, CenterTextDy(dy, kh, 14.0f), 14.0f, key, col);
		const float cx = dx + kw + 9.0f;
		Text(FontRow, cx, CenterTextDy(dy, kh, 14.0f), 14.0f, label, col);
		return (cx + Measure(FontRow, label, 0.0f) / UIConst::Scale) - dx;   // 消費幅
	}

	float Keycap(float dx, float dy, const char* key, const char* label)
	{
		return Keycap(dx, dy, key, label, UIConst::INK);
	}

	void Stepper(float dx, float dy, float w, const char* value)
	{
		using namespace UIConst;
		const float h = 34.0f;
		const float ty = CenterTextDy(dy, h, 16.0f);
		if (w <= 0.0f) { w = 158.0f; }
		Text(FontRow, dx, ty, 16.0f, "<", INK);                       // 左
		TextC(FontRow, dx + w * 0.5f, ty, 16.0f, value, INK);         // 値は中央寄せ
		const float rw = Measure(FontRow, ">", 0.0f) / UIConst::Scale;
		Text(FontRow, dx + w - rw, ty, 16.0f, ">", INK);              // 右
	}

	void WindowHeader(float dx, float dy, float w, const char* title, const Math::Color& bg, const Math::Color& fg)
	{
		using namespace UIConst;
		const float h = 30.0f;
		RectTL(dx, dy, w, h, bg, true);
		FrameTL(dx, dy, w, h, 2.0f, INK);
		const float ty = CenterTextDy(dy, h, 12.0f);
		Text(FontSmall, dx + 12.0f, ty, 12.0f, title, fg);
		// ✕
		auto& sp = SP();
		const int x = static_cast<int>(MapX(dx + w - 16.0f)), y = static_cast<int>(MapY(dy + h * 0.5f));
		sp.DrawLine(x - 5, y - 5, x + 5, y + 5, &fg); sp.DrawLine(x + 5, y - 5, x - 5, y + 5, &fg);
	}

	void Swatch(float dx, float dy, const char* label, const Math::Color& col)
	{
		using namespace UIConst;
		RectTL(dx, dy, 40.0f, 22.0f, col, true);
		FrameTL(dx, dy, 40.0f, 22.0f, 1.0f, INK);
		Text(FontFoot, dx + 52.0f, dy + 5.0f, 11.0f, label, INK);
	}

	void SectionLabel(float dx, float dy, const char* label)
	{
		using namespace UIConst;
		Text(FontSmall, dx, dy, 12.0f, label, INK);
		LineD(dx, dy + 20.0f, dx + 240.0f, dy + 20.0f, 2.0f, INK);   // 下線
	}
}

//==========================================================
// HjUI::Deco  装飾
//==========================================================
namespace HjUI::Deco
{
	using namespace UIConst;

	//----------------------------------------------------------
	// 同じ大きさの円を左上へずらしながら重ねる。
	// 大きさを変えずに位置だけずらすのが要点で、
	// 縮小コピーにすると遠近に見えてしまい、motifとして別物になる。
	//----------------------------------------------------------
	void DriftCircles(float startX, float startY, float r, int count,
		float stepX, float stepY, const Math::Color& col)
	{
		for (int i = 0; i < count; ++i)
		{
			RingD(startX + stepX * i, startY + stepY * i, r, col);
		}
	}

	void BoxedX(float dx, float dy, float s)
	{
		FrameTL(dx, dy, s, s, 5.0f, ACID);
		const float p = s * 0.24f;
		LineD(dx + p,     dy + p,     dx + s - p, dy + s - p, 4.0f, INK);
		LineD(dx + s - p, dy + p,     dx + p,     dy + s - p, 4.0f, INK);
	}

	void DotGrid(float dx, float dy, int cols, int rows, float gap, const Math::Color& col)
	{
		for (int y = 0; y < rows; ++y)
		{
			for (int x = 0; x < cols; ++x)
			{
				DiscD(dx + x * gap, dy + y * gap, DotGridRadius, col);
			}
		}
	}

	//----------------------------------------------------------
	// バーコード状の縦線。
	// 幅を規則的にばらつかせる。完全に等間隔だと機械的な縞になり、
	// 印刷物から取ってきた記号には見えない。
	//----------------------------------------------------------
	void Barcode(float dx, float dy, float h, int count)
	{
		float x = dx;
		for (int i = 0; i < count; ++i)
		{
			// 3種の幅を巡回させて、規則はあるが読めない並びにする
			const float w = BarcodeWidths[i % BarcodeWidthCount];
			RectTL(x, dy, w, h, INK);
			x += w + BarcodeGap;
		}
	}

	void Guide(float dx, float dy0, float dy1, const Math::Color& col)
	{
		LineD(dx, dy0, dx, dy1, 1.0f, col);
	}

	//----------------------------------------------------------
	// ガイド線の切れ目に置く印。
	// 線を途中で切っただけだと描き忘れに見えるので、
	// 切れ目であることを示す短い横線を足す。
	//----------------------------------------------------------
	void CropTick(float dx, float dy, const Math::Color& col)
	{
		LineD(dx - CropTickLen, dy, dx + CropTickLen, dy, 2.0f, col);
	}

	//----------------------------------------------------------
	// 画面四隅のかぎ括弧。
	// 中身を囲わないので、映像の上に置いても画面を塞がない。
	//----------------------------------------------------------
	void CornerBrackets(float inset, float len, float w, float h,
		float px, const Math::Color& col)
	{
		const float l = inset, r = w - inset;
		const float t = inset, b = h - inset;

		LineD(l, t, l + len, t, px, col);   LineD(l, t, l, t + len, px, col);   // 左上
		LineD(r - len, t, r, t, px, col);   LineD(r, t, r, t + len, px, col);   // 右上
		LineD(l, b, l + len, b, px, col);   LineD(l, b - len, l, b, px, col);   // 左下
		LineD(r - len, b, r, b, px, col);   LineD(r, b - len, r, b, px, col);   // 右下
	}
}
