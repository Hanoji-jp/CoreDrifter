#include "CountdownUI.h"

using namespace UIConst;
using namespace CountdownConst;
namespace U = HjUI;

void CountdownUI::Update()
{
	if (m_done) { return; }

	m_timer += KdFPSController::GetDt();
	if (m_timer >= StartCount * StepTime + GoTime) { m_done = true; }
}

void CountdownUI::DrawSprite()
{
	if (m_done) { return; }

	Math::Color paper = WHITE;
	Math::Color faint = WHITE; faint.w = 0.30f;

	U::Deco::CornerBrackets(BracketInset, BracketLen, CanvasW, CanvasH, 2.0f, paper);

	U::TextAtC(FontRow, CenterX, CourseY, "TOUGE  //  FREE RUN", paper);

	// 数字。1秒ごとに膨らませて縮める。
	// 大きさが変わる瞬間があると、数が変わったことが目に留まる。
	const int step = static_cast<int>(m_timer / StepTime);
	const float frac = m_timer / StepTime - static_cast<float>(step);

	char label[8];
	if (step < StartCount) { snprintf(label, sizeof(label), "%d", StartCount - step); }
	else                   { snprintf(label, sizeof(label), "GO"); }

	// 元の文字は文字テクスチャなので、伸ばした倍率ぶん粗さが出る。
	// 47pxから200pxへ伸ばすと4倍以上になり、元のドットがそのまま見える。
	// 一番大きいフォント(125px)から伸ばせば倍率が1.6倍で済む。
	const float pulse = 1.0f + PulseAmount * sinf(frac * 3.14159265f);
	const float scale = pulse * (NumberPx / UIConst::FontPx(FontTitle));
	U::TextCenteredScaledOutline(FontTitle, CenterX, NumberY, scale, label, INK, ACID);

	// 合図の帯。文字幅にぴったり合わせる
	{
		const char* strap = (step < StartCount) ? "GET READY" : "SEND IT";
		const float w = U::Measure(FontRow, strap, 0.0f) / Scale + StrapPadX * 2.0f;
		U::RectTL(CenterX - w * 0.5f, StrapY, w, StrapH, paper);
		U::TextAtC(FontRow, CenterX,
		           StrapY + CenterInBox(StrapH, FontPx(FontRow)), strap, INK);
	}

	U::LineD(CenterX, 120.0f, CenterX, 300.0f, 1.0f, faint);
	U::LineD(CenterX, 640.0f, CenterX, 760.0f, 1.0f, faint);
}
