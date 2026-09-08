#include "GarageUI.h"

#include "../Car/HjCarChoice.h"
#include "../Car/CarBase.h"
#include "../../Input/HjKeyInput.h"
#include "HjCarPortrait.h"

namespace U  = HjUI;
namespace GC = GarageConst;

using namespace UIConst;

namespace
{
	//======================================================
	// 背景のぐにゃぐにゃ
	//
	// タイヤ痕を重ねたような、不規則な閉曲線を何枚も回して絡ませる。
	//
	// ■ 楕円の大小で代用しないこと
	// 同心円や相似形を重ねると「的」に見える。
	// 走った跡には見えない。歪んでいることそのものが図案なので、
	// 制御点は不揃いのまま持つ
	//======================================================

	//======================================================
	// 文字の幅(デザイン単位)
	//
	// HjUI::Measure が返すのは、そのフォントを焼いた寸法での画面px。
	// 素の値で右端合わせをすると、native と違う大きさで描いた文字だけ
	// ずれる。描く大きさとの比を掛けて揃える
	//======================================================
	float TextWidthD(int fontId, const char* str, float pxH)
	{
		const float native = UIConst::FontPx(fontId);
		if (native <= 0.0f) { return 0.0f; }

		return (U::Measure(fontId, str, 0.0f) / UIConst::Scale) * (pxH / native);
	}

	//======================================================
	// 背景のハーフトーン
	//
	// タイトルと同じ HjUI::DotFieldTwinkle で打つ。
	//
	// これは大きさ(デザイン単位)で受けるが、点の間隔は
	// UIConst::DotCell (画面px)で固定されている。
	// 幅をそのまま渡すと、解像度が変わった瞬間に点の数が変わって
	// 端が欠けたり一列はみ出したりする。
	//
	// 点の数から必要な幅を逆算して渡す
	//======================================================
	void DotPatch(const GarageConst::DotPatch& p)
	{
		const float step = UIConst::DotCell / UIConst::Scale;   // 1点ぶん(デザイン単位)

		Math::Color col = p.grey ? UIConst::GREY9 : UIConst::DOTS;
		col.w = p.alpha;

		// 最後の1点まで含めたいので、間隔は (個数-1) 本ぶん
		U::DotFieldTwinkle(p.x, p.y,
		                   step * (p.cols - 1),
		                   step * (p.rows - 1),
		                   col, p.phase);
	}

	//======================================================
	// 右端のトンボ
	//
	// 紙から切り出した体裁を作る飾り。
	// 押せないので、押せる枠(FrameTL)とは太さも色も変えてある
	//======================================================
	void CropTick(float y)
	{
		const Math::Color col = UIConst::GREY;

		const float h[] = {
			GC::TickX,                y,
			GC::TickX + GC::TickLen,  y,
		};
		U::PolylineD(h, 2, GC::TickPx, col);

		const float cx = GC::TickX + GC::TickLen * 0.5f;
		const float v[] = {
			cx, y - GC::TickHalf,
			cx, y + GC::TickHalf,
		};
		U::PolylineD(v, 2, GC::TickPx, col);
	}

	// 3本ぶんの制御点。4区間 x (始点・制御1・制御2・終点) x (x,y)
	// 原点まわりに閉じている
	const float WL1[] = {
		-210,-6,  -150,-84,  -50,-96,   44,-70,
		  44,-70,  150,-40,   236,-26,  202, 42,
		 202, 42,  172,100,    56, 82,  -54, 80,
		 -54, 80, -158, 78,  -244, 60, -210, -6,
	};
	const float WL2[] = {
		-176, 34, -214,-38,  -104,-90,   -6,-80,
		  -6,-80,  124,-66,   222,-30,  172, 30,
		 172, 30,  134, 82,    30, 60,  -66, 92,
		 -66, 92, -148,118,  -150, 92, -176, 34,
	};
	const float WL3[] = {
		-150,-34,  -74,-96,   66,-78,  152,-46,
		 152,-46,  214,-22,  196, 34,  146, 58,
		 146, 58,   74, 92,  -46, 66, -126, 58,
		-126, 58, -196, 50, -204, 18, -150,-34,
	};

	// 3次ベジェを点の列にする
	void SampleCubic(float x0, float y0, float x1, float y1,
	                 float x2, float y2, float x3, float y3,
	                 int steps, float* out, int& n)
	{
		for (int i = 0; i <= steps; ++i)
		{
			const float t = static_cast<float>(i) / steps;
			const float u = 1.0f - t;

			const float b0 = u * u * u;
			const float b1 = 3.0f * u * u * t;
			const float b2 = 3.0f * u * t * t;
			const float b3 = t * t * t;

			out[n++] = b0 * x0 + b1 * x1 + b2 * x2 + b3 * x3;
			out[n++] = b0 * y0 + b1 * y1 + b2 * y2 + b3 * y3;
		}
	}

	// 1本ぶんを、回して・伸ばして・置いて描く
	void DrawLoop(const float* segs, float cx, float cy,
	              float rot, float scale, const Math::Color& col)
	{
		// 4区間 x (steps+1)点 x 2成分
		float pts[4 * (GC::LoopSteps + 1) * 2] = {};
		int   n = 0;

		for (int s = 0; s < 4; ++s)
		{
			const float* p = segs + s * 8;
			SampleCubic(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
			            GC::LoopSteps, pts, n);
		}

		const float ca = cosf(rot), sa = sinf(rot);
		for (int i = 0; i < n; i += 2)
		{
			const float x = pts[i]     * scale;
			const float y = pts[i + 1] * scale;

			pts[i]     = cx + x * ca - y * sa;
			pts[i + 1] = cy + x * sa + y * ca;
		}

		U::PolylineD(pts, n / 2, 1.0f, col);
	}

	// 12枚を回して絡ませる
	void DriftLoops(float cx, float cy, float spin, const Math::Color& col)
	{
		struct Copy { const float* segs; float rotDeg, scale; };

		static const Copy kCopies[] = {
			{ WL1,   0.0f, 1.00f }, { WL2,  28.0f, 1.00f },
			{ WL3,  54.0f, 1.00f }, { WL1,  80.0f, 1.16f },
			{ WL2, 108.0f, 0.88f }, { WL3, 134.0f, 1.10f },
			{ WL1, 162.0f, 0.94f }, { WL2, 196.0f, 1.22f },
			{ WL3, 228.0f, 0.98f }, { WL1, 262.0f, 1.08f },
			{ WL2, 300.0f, 0.92f }, { WL3, 332.0f, 1.14f },
		};

		constexpr float Deg = 3.14159265f / 180.0f;

		for (const Copy& k : kCopies)
		{
			DrawLoop(k.segs, cx, cy, spin + k.rotDeg * Deg, k.scale, col);
		}
	}

	// 決めた範囲に対する位置(0〜1)。
	// 台数が少ないので、車どうしで正規化すると
	// 常に片方が満タンになって差が読めない
	float Norm(float v, float lo, float hi)
	{
		return std::clamp((v - lo) / std::max(hi - lo, 0.001f), 0.0f, 1.0f);
	}
}

//----------------------------------------------------------
// 車の設定を読んで一覧を作る
//
// 表に手で書くと、車を触るたびに画面の数字と食い違う。
// 実際の設定(Silvia::Setup / Nsx::Setup)を当てて読む
//----------------------------------------------------------
void GarageUI::BuildEntries()
{
	m_entries.clear();

	for (int i = 0; i < static_cast<int>(CarChoiceConst::Kind::Count); ++i)
	{
		const auto kind = static_cast<CarChoiceConst::Kind>(i);

		// 設定だけ当てた車を作って値を読む。
		// Init は呼ばないので、モデルの読み込みは走らない
		CarBase probe;
		HjCarChoice::ApplySpec(probe, kind);

		Entry e;
		e.kind  = kind;
		e.name  = HjCarChoice::NameOf(kind);
		e.model = probe.GetOwnBodyName();

		// 煙の色をそのまま札の色にする。
		// 走っているときに出る色と揃うので、見分けが付く
		const Math::Vector3& sc = probe.GetSmokeColorA();
		e.swatch = Math::Color(sc.x, sc.y, sc.z, 1.0f);

		// 等級。最高速と出力から機械的に決める。
		// 手で振ると、車を触ったときに合わなくなる
		const float grade = Norm(probe.GetMaxSpeedSpec(), GC::SpeedMin, GC::SpeedMax) * 0.5f
		                  + Norm(probe.GetEnginePowerSpec(), GC::PowerMin, GC::PowerMax) * 0.5f;

		e.tier = (grade > 0.75f) ? "S" : (grade > 0.55f) ? "A"
		       : (grade > 0.35f) ? "B" : "C";

		e.stats = {
			{ "SPEED",        Norm(probe.GetMaxSpeedSpec(),     GC::SpeedMin, GC::SpeedMax) },
			{ "ACCELERATION", Norm(probe.GetEnginePowerSpec(),  GC::PowerMin, GC::PowerMax) },
			{ "HANDLING",     Norm(probe.GetMuFrontSpec(),      GC::GripMin,  GC::GripMax)  },
			// 後ろが前より滑るほど出る。比が小さいほどドリフト向き
			{ "DRIFT",        1.0f - Norm(probe.GetMuRearSpec() / std::max(probe.GetMuFrontSpec(), 0.01f),
			                              GC::DriftMin, GC::DriftMax) },
			{ "BRAKING",      Norm(probe.GetBrakePowerSpec(),   GC::BrakeMin, GC::BrakeMax) },
		};

		m_entries.push_back(std::move(e));
	}
}

//----------------------------------------------------------
void GarageUI::Init()
{
	BuildEntries();

	// いま選ばれている車に合わせる。
	// 毎回先頭へ戻ると、選び直したのか分からない
	const auto now = HjCarChoice::Instance().Get();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i].kind == now) { m_sel = static_cast<int>(i); break; }
	}
}

CarChoiceConst::Kind GarageUI::Selected() const
{
	if (m_entries.empty()) { return CarChoiceConst::Fallback; }

	const int i = std::clamp(m_sel, 0, static_cast<int>(m_entries.size()) - 1);
	return m_entries[i].kind;
}

//----------------------------------------------------------
void GarageUI::Update()
{
	m_spin += KdFPSController::GetDt() * GC::LoopSpinDeg * (3.14159265f / 180.0f);

	U::BeginInput();

	auto& key = HjKeyInput::Instance();
	const int n = static_cast<int>(m_entries.size());
	if (n <= 0) { return; }

	if (key.Pressed(VK_UP)   || key.Pressed('W')) { m_sel = (m_sel - 1 + n) % n; }
	if (key.Pressed(VK_DOWN) || key.Pressed('S')) { m_sel = (m_sel + 1) % n; }
	if (key.Pressed(VK_LEFT) || key.Pressed('A')) { m_sel = (m_sel - 1 + n) % n; }
	if (key.Pressed(VK_RIGHT)|| key.Pressed('D')) { m_sel = (m_sel + 1) % n; }

	if (key.Pressed(VK_RETURN) || key.Pressed(VK_SPACE)) { m_decided = true; }
	if (key.Pressed(VK_ESCAPE))                          { m_back    = true; }

	//===== 押しても選べる =====
	// 左の一覧
	for (int i = 0; i < n; ++i)
	{
		const float y = GC::ListY + i * GC::ListStep;
		if (U::Clicked(GC::PadX - GC::ListPadX, y - 6.0f, 220.0f, 38.0f)) { m_sel = i; }
	}

	// 下の帯。矢印を枠で描いておいて押せないと、
	// 壊れているのか飾りなのか区別が付かない
	const float rightAx = UIConst::DesignW - GC::PadX - GC::ArrowW;

	if (U::Clicked(GC::PadX,  GC::StripY, GC::ArrowW, GC::StripH)) { m_sel = (m_sel - 1 + n) % n; }
	if (U::Clicked(rightAx, GC::StripY, GC::ArrowW, GC::StripH)) { m_sel = (m_sel + 1) % n; }

	{
		const float band = rightAx - (GC::PadX + GC::ArrowW) - GC::StripGap * 2.0f;
		const float cw   = (band - GC::StripGap * (n - 1)) / n;

		float x = GC::PadX + GC::ArrowW + GC::StripGap;
		for (int i = 0; i < n; ++i)
		{
			if (U::Clicked(x, GC::StripY, cw, GC::StripH)) { m_sel = i; }
			x += cw + GC::StripGap;
		}
	}
}

//----------------------------------------------------------
// 見出し
//----------------------------------------------------------
void GarageUI::DrawHeader() const
{
	const float w1 = U::Text(FontHead, GC::PadX, GC::PadY, 47.0f, "GARAGE ", INK);
	const float w2 = U::Text(FontHead, w1, GC::PadY, 47.0f, "/ ", GREY);
	U::Text(FontHead, w2, GC::PadY, 47.0f, "CAR SELECT", ACID);

	//===== 右上 =====
	// 元案はここに所持金があったが、この作品に通貨は無い。
	// 選べる台数を出す
	{
		const char* label = "CARS";
		char value[8] = {};
		sprintf_s(value, "%02d", static_cast<int>(m_entries.size()));

		const float lw = TextWidthD(FontRow, label, GC::CountLabelPx);
		const float vw = TextWidthD(FontTab, value, GC::CountValuePx);

		const float right = UIConst::DesignW - GC::PadX;

		U::Text(FontRow, right - lw, GC::CountLabelY, GC::CountLabelPx, label, GREY);
		U::Text(FontTab, right - vw, GC::CountValueY, GC::CountValuePx, value, INK);
	}
}

//----------------------------------------------------------
// 車の一覧(左の列)
//
// 作り話のブランドは持たない。選べる車をそのまま並べる
//----------------------------------------------------------
void GarageUI::DrawList() const
{
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		const float y = GC::ListY + i * GC::ListStep;
		const bool  active = (static_cast<int>(i) == m_sel);

		if (active)
		{
			const float w = U::Measure(FontTab, m_entries[i].name.c_str(), 0.0f)
			              / UIConst::Scale + 60.0f;

			U::RectTL(GC::PadX - GC::ListPadX, y - 6.0f, w, 38.0f, ACID, true);
		}

		const Math::Color ink = active ? INK : SUBTXT;

		U::Text(FontTab, GC::PadX, y, 17.0f, "///", ink);
		U::Text(FontTab, GC::PadX + 34.0f, y, 17.0f, m_entries[i].name.c_str(), ink);
	}
}

//----------------------------------------------------------
// 車の台(中央)
//----------------------------------------------------------
void GarageUI::DrawStage(const Entry& e) const
{
	U::Text(FontTab,  GC::StageX, GC::StageY, 17.0f, e.name.c_str(), INK);
	U::Text(FontHead, GC::StageX, GC::StageY + 26.0f, 47.0f, e.model.c_str(), INK);

	//===== 等級の札 =====
	{
		const float lw = U::Measure(FontRow, "TIER", 0.0f) / UIConst::Scale + 24.0f;
		const float rw = U::Measure(FontRow, e.tier.c_str(), 0.0f) / UIConst::Scale + 32.0f;

		U::FrameTL(GC::StageX, GC::TierY, lw, GC::TierH, 2.0f, INK);
		U::Text(FontRow, GC::StageX + 12.0f,
		        GC::TierY + CenterInBox(GC::TierH, 14.0f), 14.0f, "TIER", INK);

		U::FrameTL(GC::StageX + lw, GC::TierY, rw, GC::TierH, 2.0f, INK);
		U::Text(FontRow, GC::StageX + lw + 16.0f,
		        GC::TierY + CenterInBox(GC::TierH, 14.0f), 14.0f, e.tier.c_str(), INK);
	}

	//===== アシッドの台 =====
	// 斜めに切る。矩形で代用すると、画面の他の要素と同じ形になって
	// 台に見えない
	{
		const float sh = GC::SlabW * GC::SlabShear;

		const float quad[] = {
			GC::SlabX + sh,           GC::SlabY,
			GC::SlabX + GC::SlabW,    GC::SlabY,
			GC::SlabX + GC::SlabW - sh, GC::SlabY + GC::SlabH,
			GC::SlabX,                GC::SlabY + GC::SlabH,
		};
		U::PolyFillD(quad, 4, ACID);
	}

	//===== 車 =====
	// 台の"後"に貼る。順番を逆にすると台が車を塗り潰す
	if (auto p = m_wpPortrait.lock())
	{
		U::TexRectTL(p->GetTexture(), GC::CarX, GC::CarY, GC::CarW, GC::CarH);
	}
}

//----------------------------------------------------------
// 性能の棒(右)
//----------------------------------------------------------
void GarageUI::DrawStats(const Entry& e) const
{
	const float x = UIConst::DesignW - GC::PadX - GC::StatW;

	for (size_t i = 0; i < e.stats.size(); ++i)
	{
		const float y = GC::StatY + i * GC::StatStep;

		U::Text(FontRow, x, y, 14.0f, e.stats[i].label, INK);

		U::RectTL(x, y + 24.0f, GC::StatW, GC::StatBarH, INK, true);
		U::RectTL(x, y + 24.0f, GC::StatW * e.stats[i].value, GC::StatBarH, ACID, true);
	}
}

//----------------------------------------------------------
// 下の帯
//----------------------------------------------------------
void GarageUI::DrawStrip() const
{
	const float rightAx = UIConst::DesignW - GC::PadX - GC::ArrowW;

	U::FrameTL(GC::PadX, GC::StripY, GC::ArrowW, GC::StripH, 2.0f, INK);
	U::TextC(FontTab, GC::PadX + GC::ArrowW * 0.5f,
	         GC::StripY + CenterInBox(GC::StripH, 17.0f), 17.0f, "<", INK);

	U::FrameTL(rightAx, GC::StripY, GC::ArrowW, GC::StripH, 2.0f, INK);
	U::TextC(FontTab, rightAx + GC::ArrowW * 0.5f,
	         GC::StripY + CenterInBox(GC::StripH, 17.0f), 17.0f, ">", INK);

	const int n = static_cast<int>(m_entries.size());
	if (n <= 0) { return; }

	const float band = rightAx - (GC::PadX + GC::ArrowW) - GC::StripGap * 2.0f;
	const float cw   = (band - GC::StripGap * (n - 1)) / n;

	float x = GC::PadX + GC::ArrowW + GC::StripGap;
	for (int i = 0; i < n; ++i)
	{
		U::RectTL(x, GC::StripY, cw, GC::StripH, m_entries[i].swatch, true);

		// 選んでいることは枠の太さで見せる。
		// 光らせたり影を落としたりしない
		U::FrameTL(x, GC::StripY, cw, GC::StripH,
		           (i == m_sel) ? 3.0f : 2.0f, INK);

		U::TextC(FontRow, x + cw * 0.5f,
		         GC::StripY + GC::StripH - 26.0f, 14.0f,
		         m_entries[i].model.c_str(), INK);

		x += cw + GC::StripGap;
	}
}

//----------------------------------------------------------
void GarageUI::DrawSprite()
{
	U::RectTL(0.0f, 0.0f, UIConst::DesignW, UIConst::DesignH, PAPER, true);

	if (m_entries.empty()) { return; }

	const Entry& e = m_entries[std::clamp(m_sel, 0, static_cast<int>(m_entries.size()) - 1)];

	// 背景。一番下に敷く
	{
		Math::Color loop = INK; loop.w = 0.22f;
		DriftLoops(GC::LoopCx, GC::LoopCy, m_spin, loop);
	}
	for (int i = 0; i < GC::DotCount; ++i) { DotPatch(GC::Dots[i]); }
	for (int i = 0; i < GC::TickCount; ++i) { CropTick(GC::TickYs[i]); }

	DrawHeader();
	DrawList();
	DrawStage(e);
	DrawStats(e);
	DrawStrip();

	//===== 下端のキー案内 =====
	{
		const float w1 = U::Keycap(GC::PadX, GC::KeyY, "ENTER", "SELECT");
		U::Keycap(GC::PadX + w1 + 30.0f, GC::KeyY, "ESC", "BACK");
	}
}
