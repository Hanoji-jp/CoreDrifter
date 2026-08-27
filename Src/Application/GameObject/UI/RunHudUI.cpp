#include "RunHudUI.h"
#include "../Car/CarBase.h"
#include "../Score/DriftScore.h"
#include "../../Const/DriftScoreConst.h"
#include "HjUiVisibility.h"

using namespace UIConst;
using namespace RunHudConst;
namespace U = HjUI;

namespace
{
	// 映像の上に置くので、メニュー画面と色の役割が逆になる。
	// 紙色を文字に使い、背景は映像そのもの。
	Math::Color Paper() { return WHITE; }
	Math::Color Dim()   { Math::Color c = WHITE; c.w = DimAlpha;   return c; }
	Math::Color Faint() { Math::Color c = WHITE; c.w = FaintAlpha; return c; }

	//----------------------------------------------------------
	// ここから下は仮の値。
	// 何本目・コース名・区間・採点の内訳・通過点は、
	// どれも対応する仕組みがまだゲーム側に無い。
	// 置き場所と見え方を先に決めておき、値が用意できたら差し替える。
	//----------------------------------------------------------
	constexpr int   kRun = 1, kRunTotal = 2;
	const char* kCourse = "NEXUS TOUGE";
	constexpr int   kSection = 2, kSectionTotal = 4;
	const char* kSectionNote = "UPHILL";

	constexpr int kClipHit = 2;

	// 走行ライン。コースの形そのものなので、汎用の曲線では代用できない
	const float kLineFull[] = { 70.0f, 830.0f, 124.0f, 826.0f, 138.0f, 780.0f,
	                            182.0f, 768.0f, 232.0f, 754.0f, 254.0f, 796.0f,
	                            304.0f, 784.0f };
	const float kLineDone[] = { 70.0f, 830.0f, 124.0f, 826.0f, 138.0f, 780.0f,
	                            182.0f, 768.0f };
}


void RunHudUI::DrawSprite()
{
	if (!HjUiVisibility::Instance().showHud)
	{
		// スコアの炎は文字流体化が別経路で描いているので、明示的に止める
		KdShaderManager::Instance().m_postProcessShader.HideFluidScore();
		return;
	}

	auto car = m_wpCar.lock();
	if (!car) { return; }

	DrawFrame();
	DrawRunInfo();
	DrawDriftAngle(car->GetDriftAngleDegSigned());
	DrawCourseLine();
	DrawTach(car->GetRpmRatio());
	DrawSpeed(car->GetSpeedKmh(), car->GetGear());

	if (auto score = m_wpScore.lock())
	{
		DrawScore(score->GetLiveScore(), score->GetCombo(), score->GetChainHold());
	}
	else
	{
		KdShaderManager::Instance().m_postProcessShader.HideFluidScore();
	}
}

//----------------------------------------------------------
// 画面の枠。
// 四隅のかぎ括弧だけで示す。囲ってしまうと、その面積ぶん
// 走っている絵が見えなくなる。
//----------------------------------------------------------
void RunHudUI::DrawFrame()
{
	U::Deco::CornerBrackets(BracketInset, BracketLen, CanvasW, CanvasH,
	                        BracketPx, Paper());

	// 画面の中心を示す短い縦線。上下に置くと、車の向きが読み取りやすい
	U::LineD(CenterX, CenterTickTop0, CenterX, CenterTickTop1, 1.0f, Faint());
	U::LineD(CenterX, CenterTickBot0, CenterX, CenterTickBot1, 1.0f, Faint());
}

//----------------------------------------------------------
// 左上：何本目・コース名・区間。
// ※中身は仮。対応する仕組みはまだ無い。
//
// 本数と総数で大きさを変える。同じ大きさで「1 / 2」と書くと、
// どちらが今なのかが読み取りにくい。
//----------------------------------------------------------
void RunHudUI::DrawRunInfo()
{
	char buf[64];

	U::TextAt(FontSmall, PadX, RunLabelY, "RUN", Paper());

	snprintf(buf, sizeof(buf), "%d", kRun);
	U::TextScaled(FontHead, PadX + RunNumX, RunNumY, RunNumPx, buf, Paper());
	const float w = U::Measure(FontHead, buf, 0.0f) / Scale
	              * (RunNumPx / UIConst::FontPx(FontHead));

	snprintf(buf, sizeof(buf), "/%d", kRunTotal);
	U::TextAt(FontTab, PadX + RunNumX + w, RunTotalY, buf, Dim());

	U::RectTL(PadX, RunRuleY, RunRuleW, RunRuleH, Paper());
	U::TextAt(FontBtn, PadX, CourseY, kCourse, Paper());

	snprintf(buf, sizeof(buf), "SECTION %d / %d  %s", kSection, kSectionTotal, kSectionNote);
	U::TextAt(FontSmall, PadX, SectionY, buf, Dim());
}

//----------------------------------------------------------
// 上中央：ドリフト角と目盛り。
//
// 目盛りの中に1本だけ高いものを立てる。
// 数字だけだと今の角度は分かっても、それが深いのか浅いのかが分からない。
// 狙う位置が見えていれば、そこへ合わせる操作になる。
//----------------------------------------------------------
void RunHudUI::DrawDriftAngle(float angleDeg)
{
	// 数字は深さだけを出す。向きは目盛りが示すので、符号は要らない
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", static_cast<int>(fabsf(angleDeg)));

	U::TextAtC(FontFoot, CenterX, AngleLabelY, "DRIFT ANGLE", Dim());
	U::TextScaledC(FontHead, CenterX, AngleValueY, AngleValuePx, buf, Paper());

	const float step = AngleTickW + AngleTickGap;
	const int   side = (angleDeg >= 0.0f) ? 1 : -1;   // 正=右へ滑っている
	const float depth = fabsf(angleDeg);

	// 目盛りは全部同じ高さ・同じ色。
	// 埋まっているかどうかだけで伸び具合を読ませる。
	for (int sgn = -1; sgn <= 1; sgn += 2)
	{
		for (int i = 1; i <= AngleTicksPerSide; ++i)
		{
			// この目盛りが表す角度
			const float tickDeg = AngleFullDeg * static_cast<float>(i)
			                    / static_cast<float>(AngleTicksPerSide);

			// 滑っている側だけ埋まる。反対側は常に空
			const bool reached = (sgn == side) && (depth >= tickDeg);

			// 外側へ行くほど赤くする。深い角度は戻せなくなる領域なので、
			// 同じ色のまま伸びるより、端へ近づくほど警告の色になった方が
			// 「行き過ぎ」が体で分かる。
			Math::Color c = Faint();
			if (reached)
			{
				const float t = std::clamp(
					(tickDeg / AngleFullDeg - AngleHotStart) / (1.0f - AngleHotStart),
					0.0f, 1.0f);
				const float k = powf(t, AngleHotCurve);

				c = Paper();
				c.x += (AngleHot.x - c.x) * k;
				c.y += (AngleHot.y - c.y) * k;
				c.z += (AngleHot.z - c.z) * k;
			}

			const float x = CenterX + static_cast<float>(sgn * i) * step
			              - AngleTickW * 0.5f;
			U::RectTL(x, AngleTickY, AngleTickW, AngleTickH, c);
		}
	}
}

//----------------------------------------------------------
// 左下：走行ラインと通過点。
// ※中身は仮。コースの形も通過判定もまだ無い。
//
// 通った部分を太くアシッドで、これからの部分を細く薄く描く。
// 同じ太さで色だけ変えると、どちらが済んだ方か分からない。
//----------------------------------------------------------
void RunHudUI::DrawCourseLine()
{
	U::TextAt(FontFoot, PadX, ClipLabelY, "CLIPPING POINTS", Paper());

	U::PolylineD(kLineFull, 7, 2.0f, Faint());
	U::PolylineD(kLineDone, 4, 4.0f, ACID);

	// 通過済みは塗り、まだの点は輪郭だけ
	U::DiscD(124.0f, 826.0f, 6.0f, ACID);
	U::DiscD(182.0f, 768.0f, 6.0f, ACID);
	U::RingD(254.0f, 796.0f, 6.0f, Paper());
	U::RingD(304.0f, 784.0f, 6.0f, Faint());

	for (int i = 0; i < ClipCount; ++i)
	{
		char cp[8];
		snprintf(cp, sizeof(cp), "CP%d", i + 1);
		U::Chip(PadX + i * ClipChipGap, ClipChipY, cp, i < kClipHit, Paper());
	}
}

//----------------------------------------------------------
// 中下：伸びているスコアとチェーン倍率。
//----------------------------------------------------------
void RunHudUI::DrawScore(double live, int combo, float hold)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", static_cast<int>(live));

	U::TextAtC(FontFoot, CenterX, ScoreLabelY, "RUN SCORE", Dim());

	// 点が伸びるほど、判定文字と同じ文字流体化を強く掛ける。
	// 数字が増えるだけだと「どれだけ凄いのか」が伝わらない。
	// 段は採点のしきい値をそのまま使う(見た目が変わる点と判定が出る点を揃える)。
	auto& pp = KdShaderManager::Instance().m_postProcessShader;

	if (live >= DriftScoreConst::GreatScore)
	{
		const bool high = (live >= DriftScoreConst::PerfectScore);
		const int  style = high ? ScoreFxStyleHigh : ScoreFxStyleLow;

		// 下の段から上の段へ向けて強くする
		const float lo = high ? DriftScoreConst::PerfectScore : DriftScoreConst::GreatScore;
		const float hi = high ? DriftScoreConst::PerfectScore * 3.0f
		                      : DriftScoreConst::PerfectScore;
		const float t = std::clamp((static_cast<float>(live) - lo) / std::max(hi - lo, 1.0f),
		                           0.0f, 1.0f);

		const Math::Vector4 core  = { 1.0f, 0.96f, 0.72f, 1.0f };
		const Math::Vector4 fluid = high ? Math::Vector4{ 0.85f, 0.95f, 0.30f, 1.0f }
		                                 : Math::Vector4{ 1.0f, 0.42f, 0.08f, 1.0f };
		pp.SetFluidScore(buf, style, core, fluid, ScoreFxDilate);

		// 枠は焼いた画像の実寸から作る。
		// 文字流体化は枠いっぱいに引き伸ばすので、縦横比が合っていないと
		// そのまま横に潰れる/伸びる。桁数で幅が変わるため固定値では合わない。
		//
		// 焼いた画像には上下に余白が入るので、文字が普通の表示と同じ高さに
		// 見えるよう、余白のぶんまで見込んで枠を大きく取る。
		const float texW = U::Measure(FontTitle, buf, 0.0f);       // 画面px
		const float texH = U::MeasureHeight(FontTitle, buf);       // 画面px(実寸)
		const float imgH = texH * ScoreFxPadRatio;                 // 余白込み

		const float h = ScoreValuePx * ScoreFxSizeMul;            // 枠の高さ(デザインpx)
		const float w = h * (texW / std::max(imgH, 1.0f));

		// 中心を普通の表示と揃える。上端を固定にすると、
		// 大きさを変えたときに下だけ伸びて輪へ被る。
		const float cy = ScoreValueY + ScoreValuePx * 0.5f + ScoreFxDropY;
		pp.UpdateFluidScore(ScoreFxMinAlpha + (1.0f - ScoreFxMinAlpha) * t,
		                    CenterX / CanvasW, cy / CanvasH,
		                    w / CanvasW, h / CanvasH);
	}
	else
	{
		// しきい値未満は普通の文字。
		// 47pxのフォントを58pxへ伸ばすと元のドットが粗さになるので、
		// 125pxのフォントから縮める方向にする。
		pp.HideFluidScore();
		U::TextScaledC(FontTitle, CenterX, ScoreValueY, ScoreValuePx, buf, Paper());
	}

	// 倍率は掛かっているときだけ出す。x1 は情報として意味がない
	if (combo <= 1) { return; }

	// 輪。全周が「残り時間いっぱい」で、減るほど欠けていく。
	// 「CHAIN」と書かなくても、減っていく形そのものが説明になる。
	// 真上から時計回りに減らすのは、時計と同じ向きで直感に合うため。
	U::ArcD(CenterX, ChainRingCy, ChainRingR, 0.0f, 6.2831853f, ChainRingPx, Faint());

	const float remain = std::clamp(hold, 0.0f, 1.0f);
	if (remain > 0.001f)
	{
		const float start = -1.5707963f;                  // 真上
		U::ArcD(CenterX, ChainRingCy, ChainRingR,
		        start, start + 6.2831853f * remain, ChainRingPx, ACID);
	}

	char cb[16];
	snprintf(cb, sizeof(cb), "x%d", combo);
	U::TextScaledC(FontHead, CenterX, ChainRingCy - ChainTextPx * 0.5f,
	               ChainTextPx, cb, Paper());
}

//----------------------------------------------------------
// 右下：回転計。
//
// 目盛りは右へ行くほど高くする。回転が上がるほど視線が引っ張られるので、
// 針を追わなくても、上限が近いことが視界の端で分かる。
// 上限域だけ色を変え、まだ届いていないぶんは薄く落とす。
//----------------------------------------------------------
void RunHudUI::DrawTach(float rpmRatio)
{
	const float total = TachTicks * TachTickW + (TachTicks - 1) * TachTickGap;
	const float x0 = CanvasW - PadX - total;
	const int   lit = static_cast<int>(TachTicks * rpmRatio + 0.5f);

	for (int i = 0; i < TachTicks; ++i)
	{
		const float h = TachTickBaseH + i * TachTickRiseH;

		Math::Color c = (i >= TachTicks - TachRedTicks) ? ACID : Paper();
		if (i > lit) { c.w = TachDimAlpha; }   // まだ回っていないぶん

		// 下端を揃えて上へ伸ばす
		U::RectTL(x0 + i * (TachTickW + TachTickGap),
		          TachY + (TachMaxH - h), TachTickW, h, c);
	}
}

//----------------------------------------------------------
// 右下：速度とギア。
// 速度は画面で一番大きい数字にする。走行中に一番よく読む値なので、
// 視線を送らなくても端で読める大きさが要る。
//----------------------------------------------------------
void RunHudUI::DrawSpeed(float kmh, int gear)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", static_cast<int>(kmh));

	// 伸ばす倍率が大きいほど元のドットが見える。
	// 126pxなら、一番大きいフォント(125px)からほぼ等倍で出せる。
	U::TextScaledR(FontTitle, CanvasW - PadX - SpeedRightPad, SpeedY, SpeedPx, buf, Paper());

	const float gx = CanvasW - PadX - UnitX;
	U::TextAt(FontSmall, gx, UnitY, "KM/H", Dim());
	U::TextAt(FontFoot,  gx, GearLabelY, "GEAR", Dim());

	// リバースは数字にならないので R と出す
	char gb[8];
	if (gear <= 0) { snprintf(gb, sizeof(gb), "R"); }
	else           { snprintf(gb, sizeof(gb), "%d", gear); }
	U::TextScaled(FontHead, gx, GearValueY, GearValuePx, gb, ACID);
}

//----------------------------------------------------------
// 調整パネル。
// 定数のままだと値を変えるたびにビルドし直すことになる。
