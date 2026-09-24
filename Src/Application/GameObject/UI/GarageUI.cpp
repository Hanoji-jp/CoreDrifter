#include "GarageUI.h"

#include "../Car/HjCarChoice.h"
#include "../Car/CarBase.h"
#include "../../Input/HjKeyInput.h"
#include "HjCarPortrait.h"

#include <algorithm>
#include <cmath>

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
	//======================================================
	// 暗い空間の上に置く色
	//
	// 画面全体が3Dになったので、墨のままでは読めない。
	// 明暗を入れ替えた色をここから配る
	//======================================================
	// 濃さを渡せるようにしてある。
	// 切り替えの途中で、行ごとに薄い所から入ってくる
	Math::Color Ink(float alpha = 1.0f)
	{
		return { GC::InkCol[0], GC::InkCol[1], GC::InkCol[2], alpha };
	}

	Math::Color Sub(float alpha = 1.0f)
	{
		return { GC::SubCol[0], GC::SubCol[1], GC::SubCol[2], alpha };
	}

	//======================================================
	// 終わり際をゆっくりにする
	//
	// 等速で動かすと、止まった瞬間が機械的に見える。
	// 入りを速く、締めを緩くすると、動いて止まったように見える
	//======================================================
	float EaseOut(float t)
	{
		const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
		return 1.0f - u * u * u;
	}

	//======================================================
	// 1つずつ遅らせて動かすときの進み
	//
	// 一斉に動かすと、棒が何本あっても1本の塊が動いたようにしか
	// 見えない。ずらすと本数が読める
	//======================================================
	float StaggerT(float animT, float lead)
	{
		const float span = std::max(GC::UiAnimTime - lead, 0.0001f);

		return std::clamp((animT * GC::UiAnimTime - lead) / span, 0.0f, 1.0f);
	}

	Math::Color Line(float alpha = 1.0f)
	{
		return { GC::LineCol[0], GC::LineCol[1], GC::LineCol[2], alpha };
	}

	float TextWidthD(int fontId, const char* str, float pxH)
	{
		const float native = UIConst::FontPx(fontId);
		if (native <= 0.0f) { return 0.0f; }

		return (U::Measure(fontId, str, 0.0f) / UIConst::Scale) * (pxH / native);
	}

	//======================================================
	// 右端のトンボ
	//
	// 紙から切り出した体裁を作る飾り。
	// 押せないので、押せる枠(FrameTL)とは太さも色も変えてある
	//======================================================
	void CropTick(float y)
	{
		const Math::Color col(GC::SubCol[0], GC::SubCol[1], GC::SubCol[2], 1.0f);

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
		e.maker = HjCarChoice::MakerOf(kind);
		e.model = HjCarChoice::ModelOf(kind);

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

		//===== 諸元表 =====
		// 車ごとに違う実寸だけを並べる
		{
			char buf[32] = {};

			e.spec.push_back({ "LAYOUT", "FR" });

			snprintf(buf, sizeof(buf), "%.0f km/h",
			         probe.GetMaxSpeedSpec() * CarConst::HudMsToKmh);
			e.spec.push_back({ "TOP SPEED", buf });

			// 前後と左右の半分で持っているので倍にする
			snprintf(buf, sizeof(buf), "%.2f m", probe.GetBaseSpec() * 2.0f);
			e.spec.push_back({ "WHEELBASE", buf });

			snprintf(buf, sizeof(buf), "%.2f m", probe.GetTrackSpec() * 2.0f);
			e.spec.push_back({ "TRACK", buf });
		}

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
	U::BeginInput();

	// 選択が変わったかは、前のフレームとの差で見る。
	//
	// 選び直す所が一覧・帯・キーと分かれていて、
	// 途中で抜ける道もある。変えた所ごとに書くと必ず漏れる
	if (m_sel != m_selPrev)
	{
		// 今出ている長さを控える。
		// 目標だけ差し替えると、動いている途中から別の値へ跳ぶ
		m_barFrom = m_barNow;

		m_animT   = 0.0f;
		m_selPrev = m_sel;
	}

	const float dt = KdFPSController::GetDt();

	m_animT = std::min(m_animT + dt / GC::UiAnimTime, 1.0f);

	UpdateAnim(dt);

	auto& key = HjKeyInput::Instance();
	const int n = static_cast<int>(m_entries.size());
	if (n <= 0) { return; }

	if (key.Pressed(VK_UP)   || key.Pressed('W')) { m_sel = (m_sel - 1 + n) % n; }
	if (key.Pressed(VK_DOWN) || key.Pressed('S')) { m_sel = (m_sel + 1) % n; }
	if (key.Pressed(VK_LEFT) || key.Pressed('A')) { m_sel = (m_sel - 1 + n) % n; }
	if (key.Pressed(VK_RIGHT)|| key.Pressed('D')) { m_sel = (m_sel + 1) % n; }

	//===== 車を手で回す =====
	// 勝手に回していると、見たい角度で止められない。
	// 絵の上を引きずるか、Q・E を押している間だけ回す
	{
		const float mx = U::MouseX();

		const bool down = key.Down(VK_LBUTTON);

		// 掴むのは絵の上だけ。一覧や帯を押したときに回らないように
		if (down && !m_dragging
		 && U::Hover(GC::SpinAreaX, GC::SpinAreaY,
		             GC::SpinAreaW, GC::SpinAreaH))
		{
			m_dragging = true;
		}

		if (!down) { m_dragging = false; }

		float yaw = 0.0f;

		if (m_dragging) { yaw += (mx - m_lastMouse) * GC::DragYawPerPx; }

		if (key.Down('Q')) { yaw -= GC::KeyYawSpeed * dt; }
		if (key.Down('E')) { yaw += GC::KeyYawSpeed * dt; }

		if (yaw != 0.0f)
		{
			if (auto p = m_wpPortrait.lock())
			{
				p->AddYaw(yaw * (3.14159265f / 180.0f));
			}
		}

		m_lastMouse = mx;
	}

	if (key.Pressed(VK_RETURN) || key.Pressed(VK_SPACE)) { m_decided = true; }
	if (key.Pressed(VK_ESCAPE))                          { m_back    = true; }

	//===== 押しても選べる =====
	// 回している間は拾わない。引きずり終わりで車が変わる
	if (m_dragging) { return; }

	// 左の一覧
	for (int i = 0; i < n; ++i)
	{
		const float y = GC::ListY + i * GC::ListStep;
		if (U::Clicked(GC::PadX - GC::ListPadX, y - 6.0f, 220.0f, 38.0f)) { m_sel = i; }
	}

	// 下の帯。矢印を枠で描いておいて押せないと、
	// 壊れているのか飾りなのか区別が付かない
	const float rightAx = GC::DesignRight - GC::ArrowW;

	if (U::Clicked(GC::PadX,  GC::StripY, GC::ArrowW, GC::StripH)) { m_sel = (m_sel - 1 + n) % n; }
	if (U::Clicked(rightAx, GC::StripY, GC::ArrowW, GC::StripH)) { m_sel = (m_sel + 1) % n; }

	{
		float x = 0.0f, cw = 0.0f;
		StripLayout(n, x, cw);

		for (int i = 0; i < n; ++i)
		{
			if (U::Clicked(x, GC::StripY, cw, GC::StripH)) { m_sel = i; }
			x += cw + GC::StripGap;
		}
	}
}

//----------------------------------------------------------
// 切り替えの動きを進める
//
// 描く側は const なので、動く値はここで作って持たせる。
// 描画の中で時間を進めると、1フレームに2回描いた時に倍進む
//----------------------------------------------------------
void GarageUI::UpdateAnim(float dt)
{
	if (m_entries.empty()) { return; }

	const Entry& e = m_entries[
		std::clamp(m_sel, 0, static_cast<int>(m_entries.size()) - 1)];

	//===== 性能の棒 =====
	const size_t n = e.stats.size();

	m_barNow.resize(n, 0.0f);
	m_barFrom.resize(n, 0.0f);

	for (size_t i = 0; i < n; ++i)
	{
		const float t = StaggerT(m_animT, GC::BarStagger * i);

		m_barNow[i] = m_barFrom[i]
			        + (e.stats[i].value - m_barFrom[i]) * EaseOut(t);
	}

	//===== 選択の地 =====
	// 行を飛び越すのではなく、遅れて追いつく
	const float target = GC::ListY + m_sel * GC::ListStep;

	if (!m_selInit)
	{
		// 開いた1フレーム目。ここで寄せ始めると、
		// 画面の外から地が飛んでくる
		m_selY    = target;
		m_selInit = true;
		return;
	}

	// フレーム時間に依らない寄せ方。
	// 単純に差の何割かを足すと、フレームが落ちたときだけ遅くなる
	const float k = 1.0f - std::exp(-GC::SelFollow * dt);

	m_selY += (target - m_selY) * k;
}

//----------------------------------------------------------
// 見出し
//----------------------------------------------------------
void GarageUI::DrawHeader() const
{
	// 元絵は 56px/900。FontTitle(125/900)を縮めて描く。
	//
	// HjUI::Text では大きさが変わらない(第4引数は縦位置)。
	// 焼いた 125px のまま出てしまうので、TextScaled を通す
	float x = GC::PadX;

	const char* part[] = { "GARAGE ", "/ ", "CAR SELECT" };
	const Math::Color col[] = { Ink(), Sub(), ACID };

	for (int i = 0; i < 3; ++i)
	{
		U::TextScaled(FontTitle, x, GC::PadY, GC::HeadPx, part[i], col[i]);
		x += TextWidthD(FontTitle, part[i], GC::HeadPx);
	}

	//===== 右上 =====
	// 元案はここに所持金があったが、この作品に通貨は無い。
	// 選べる台数を出す
	{
		const char* label = "CARS";
		char value[8] = {};
		sprintf_s(value, "%02d", static_cast<int>(m_entries.size()));

		const float lw = TextWidthD(FontFoot, label, GC::CountLabelPx);
		const float vw = TextWidthD(FontTab,  value, GC::CountValuePx);

		U::TextScaled(FontFoot, GC::DesignRight - lw, GC::CountLabelY,
		              GC::CountLabelPx, label, Sub());
		U::TextScaled(FontTab, GC::DesignRight - vw, GC::CountValueY,
		              GC::CountValuePx, value, Ink());
	}
}

//----------------------------------------------------------
// 車の一覧(左の列)
//
// 作り話のブランドは持たない。選べる車をそのまま並べる
//----------------------------------------------------------
void GarageUI::DrawList() const
{
	const int n = static_cast<int>(m_entries.size());
	if (n <= 0) { return; }

	const int sel = std::clamp(m_sel, 0, n - 1);

	//===== 選ばれている行の地 =====
	// 行の位置ではなく、追いかけている位置に出す。
	// 地が遅れて付いてくると、一覧が動いた形になる
	{
		const float w =
			TextWidthD(FontTab, m_entries[sel].name.c_str(), GC::ListPx)
			+ GC::ListIconW + GC::ListPadX * 2.0f;

		U::RectTL(GC::PadX - GC::ListPadX, m_selY - GC::ListPadY,
			  w, GC::ListRowH, ACID, true);
	}

	// 地が今どの行に一番近いか。
	//
	// 地は遅れて動くので、選んだ行で墨に戻すと、
	// 地の来ていない行が先に墨になって背景に溶ける。
	// 幅で見ると、行と行の間で1つも当たらない瞬間ができて、
	// そこだけ地の上に薄い字が乗る。必ず1行に決まる形にする
	const int lit = std::clamp(
		static_cast<int>(std::lround((m_selY - GC::ListY) / GC::ListStep)),
		0, n - 1);

	for (int i = 0; i < n; ++i)
	{
		const float y = GC::ListY + i * GC::ListStep;

		const bool on = (i == lit);

		const Math::Color ink = on ? INK : Sub();

		U::TextScaled(FontTab, GC::PadX, y, GC::ListPx, "///", ink);
		U::TextScaled(FontTab, GC::PadX + GC::ListIconW, y, GC::ListPx,
			      m_entries[i].name.c_str(), ink);
	}
}

//----------------------------------------------------------
// 車の台(中央)
//----------------------------------------------------------
void GarageUI::DrawStage(const Entry& e) const
{
	// 車名は横から入れる。
	//
	// 名前が瞬時に差し替わると、絵が切り替わっただけに見える。
	// 車の入れ替えに合わせて動かすと、同じ台の上で乗り換わって見える
	const float k  = EaseOut(m_animT);
	const float dx = (1.0f - k) * GC::NameSlide;

	// 元絵は 作り手20px / 型番96px。どちらも weight 900。
	//
	// 型番にモデルのファイル名を出していたときは "silvia_body" と
	// 並んで、96pxでは台の下まではみ出して読めなかった
	U::TextScaled(FontHead, GC::StageX + dx, GC::StageY, GC::NamePx,
		      e.maker.c_str(), Ink(k));

	// 型名は 96px の大見出し。
	// FontHead(47px)では倍に引き伸ばすことになるので、
	// もっと大きく焼いてある FontTitle(125px)を縮めて使う。
	//
	// 大きい字ほど大きく動かす。同じ量だと、下の小さい字だけが目立つ
	U::TextScaled(FontTitle, GC::StageX + dx * 1.6f, GC::ModelY, GC::ModelPx,
		       e.model.c_str(), Ink(k));

	//===== 等級の札 =====
	// 札は動かさない。枠まで動くと、画面の骨組みごと揺れて見える
	{
		const float lw = TextWidthD(FontRow, "TIER", GC::TierPx) + GC::TierPadL * 2.0f;
		const float rw = TextWidthD(FontRow, e.tier.c_str(), GC::TierPx) + GC::TierPadR * 2.0f;

		U::FrameTL(GC::StageX, GC::TierY, lw, GC::TierH, 2.0f, Ink());
		U::TextScaled(FontRow, GC::StageX + GC::TierPadL,
			      GC::TierY + CenterInBox(GC::TierH, GC::TierPx),
			      GC::TierPx, "TIER", Ink());

		// 等級そのものは車で変わるので、こちらは濃さだけ合わせる
		U::FrameTL(GC::StageX + lw, GC::TierY, rw, GC::TierH, 2.0f, Ink());
		U::TextScaled(FontRow, GC::StageX + lw + GC::TierPadR,
			      GC::TierY + CenterInBox(GC::TierH, GC::TierPx),
			      GC::TierPx, e.tier.c_str(), Ink(k));
	}
}

//----------------------------------------------------------
// 性能の棒(右)
//----------------------------------------------------------
void GarageUI::DrawStats(const Entry& e) const
{
	const float x = GC::DesignRight - GC::StatW;

	for (size_t i = 0; i < e.stats.size(); ++i)
	{
		const float y = GC::StatY + i * GC::StatStep;

		U::TextScaled(FontRow, x, y, GC::StatPx, e.stats[i].label, Ink());

		const float by = y + GC::StatBarDy;

		// 地は薄く。暗い上では墨の地が背景と見分けが付かない
		U::RectTL(x, by, GC::StatW, GC::StatBarH, Line(GC::BarBackAlpha), true);

		// 長さは車の値ではなく、追いかけている長さ。
		// 直に描くと、選び直した瞬間に別の長さへ跳ぶ
		const float v = (i < m_barNow.size()) ? m_barNow[i] : e.stats[i].value;

		U::RectTL(x, by, GC::StatW * v, GC::StatBarH, ACID, true);
	}
}

//----------------------------------------------------------
// 諸元表(左の列の下)
//
// 右の棒は「他と比べてどうか」しか言わない。
// 実寸を並べて補うと、選ぶ手がかりが増えるうえ、
// 台数が少ないときに空く左下も埋まる
//----------------------------------------------------------
void GarageUI::DrawSpec(const Entry& e) const
{
	U::TextScaled(FontFoot, GC::PadX, GC::SpecY, GC::SpecHeadPx, "SPEC", Sub());

	float y = GC::SpecY + GC::SpecHeadGap;

	int row = 0;

	for (const auto& r : e.spec)
	{
		// 上の行から順に入ってくる。
		// 一斉に出すと、表が丸ごと差し替わったように見える
		const float k = EaseOut(StaggerT(m_animT, GC::SpecStagger * row));

		// 横から入れる。薄いまま出すだけだと、何も動いていないように見える
		const float dx = (1.0f - k) * GC::SpecSlide;

		U::TextScaled(FontRow, GC::PadX + dx, y, GC::SpecPx, r.label, Sub(k));

		// 値は右端で揃える。桁が変わっても列が崩れない
		const float vw = TextWidthD(FontRow, r.value.c_str(), GC::SpecPx);

		U::TextScaled(FontRow, GC::PadX + GC::ListW - vw + dx, y,
			      GC::SpecPx, r.value.c_str(), Ink(k));

		// 行の下に細い罫線。並びが表に見える
		U::RectTL(GC::PadX, y + GC::SpecLineY, GC::ListW, 1.0f,
			  Line(0.30f * k), true);

		y += GC::SpecStep;
		++row;
	}
}

//----------------------------------------------------------
// 下の帯の札の位置
//
// ■ 幅に上限を置く
// 元絵は5台前提で、帯を等分して並べていた。
// 台数が少ないまま等分すると1枚が画面幅いっぱいまで伸びて、
// 車の札ではなく色の帯に見える。
// 5台のときの幅を上限にして、余ったぶんは中央へ寄せる
//----------------------------------------------------------
void GarageUI::StripLayout(int n, float& outX, float& outW) const
{
	const float rightAx = GC::DesignRight - GC::ArrowW;

	const float band = rightAx - (GC::PadX + GC::ArrowW) - GC::ArrowGap * 2.0f;

	outW = std::min((band - GC::StripGap * (n - 1)) / n, GC::StripCellMax);

	const float used = outW * n + GC::StripGap * (n - 1);

	outX = GC::PadX + GC::ArrowW + GC::ArrowGap + (band - used) * 0.5f;
}

//----------------------------------------------------------
// 下の帯
//----------------------------------------------------------
void GarageUI::DrawStrip() const
{
	const float rightAx = GC::DesignRight - GC::ArrowW;

	U::FrameTL(GC::PadX, GC::StripY, GC::ArrowW, GC::StripH, 2.0f, Ink());
	U::TextScaledC(FontTab, GC::PadX + GC::ArrowW * 0.5f,
	               GC::StripY + CenterInBox(GC::StripH, GC::ArrowPx),
	               GC::ArrowPx, "<", Ink());

	U::FrameTL(rightAx, GC::StripY, GC::ArrowW, GC::StripH, 2.0f, Ink());
	U::TextScaledC(FontTab, rightAx + GC::ArrowW * 0.5f,
	               GC::StripY + CenterInBox(GC::StripH, GC::ArrowPx),
	               GC::ArrowPx, ">", Ink());

	const int n = static_cast<int>(m_entries.size());
	if (n <= 0) { return; }

	float x = 0.0f, cw = 0.0f;
	StripLayout(n, x, cw);
	for (int i = 0; i < n; ++i)
	{
		U::RectTL(x, GC::StripY, cw, GC::StripH, m_entries[i].swatch, true);

		// 選んでいることは枠の太さで見せる。
		// 光らせたり影を落としたりしない
		U::FrameTL(x, GC::StripY, cw, GC::StripH,
		           (i == m_sel) ? 3.0f : 2.0f, Ink());

		U::TextScaledC(FontRow, x + cw * 0.5f,
		               GC::StripY + GC::StripH - GC::StripTextUp, GC::StatPx,
		               m_entries[i].model.c_str(), INK);

		x += cw + GC::StripGap;
	}
}

//----------------------------------------------------------
void GarageUI::DrawSprite()
{
	// 地は塗らない。後ろは3Dの空間がそのまま見えている。
	//
	// 網点とぐにゃぐにゃの線もやめた。紙の上の図案だったもので、
	// 空間の手前に浮くと窓に貼った紙くずに見える

	if (m_entries.empty()) { return; }

	const Entry& e = m_entries[std::clamp(m_sel, 0, static_cast<int>(m_entries.size()) - 1)];

	// 裁ち切りのトンボだけは残す。画面の端の飾りで、空間には掛からない
	for (int i = 0; i < GC::TickCount; ++i) { CropTick(GC::TickYs[i]); }

	DrawHeader();
	DrawList();

	DrawStage(e);
	DrawStats(e);
	DrawSpec(e);
	DrawStrip();

	//===== 下端のキー案内 =====
	{
		float x = GC::PadX;

		x += U::Keycap(x, GC::KeyY, "ENTER", "SELECT") + GC::KeyGap;
		x += U::Keycap(x, GC::KeyY, "Q / E", "TURN") + GC::KeyGap;
		U::Keycap(x, GC::KeyY, "ESC", "BACK");
	}
}
