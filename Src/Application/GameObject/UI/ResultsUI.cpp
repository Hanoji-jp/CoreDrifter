#include "ResultsUI.h"
#include "../../Input/HjKeyInput.h"

using namespace UIConst;
using namespace MultiConst;
namespace U = HjUI;

namespace
{
	const float kColX[4] = { PadX + 20.0f, PadX + 120.0f, PadX + 560.0f, PadX + 1290.0f };
	const char* const kCols[4] = { "RANK", "DRIVER", "SCORE", "CHAIN" };
	constexpr int kColCount = 4;
}

void ResultsUI::Update()
{
	U::BeginInput();
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel) || HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide)) { m_back = true; }
}

void ResultsUI::DrawSprite()
{
	DrawFrame("DRIFT RUN // COMPLETE", "RESULTS");

	DrawGuides(420.0f, 180.0f, 250.0f);
	{
		Math::Color dot = INK; dot.w = DotAlpha;
		U::Deco::DotGrid(1180.0f, 70.0f, 12, 8, DotGap, dot);
	}
	// 右下の黒帯。画面の底を締める
	U::RectTL(960.0f, 840.0f, CanvasW - 960.0f, 24.0f, INK);

	DrawTable();
}

//----------------------------------------------------------
// 成績の表。
// 点数は棒でも出す。数字だけだと差の大きさが読み取れない。
// 長さは1位を基準にするので、棒の差がそのまま点差になる。
//----------------------------------------------------------
void ResultsUI::DrawTable()
{
	const int rows = std::max(static_cast<int>(m_rows.size()), 1);
	const float h = HeadH + RowH * rows;

	U::FrameTL(PadX, TableY, ContentW, h, 2.0f, INK);
	U::TableHead(PadX, TableY, ContentW, kCols, kColX, kColCount);

	if (m_rows.empty())
	{
		U::TextAtC(FontCard, PadX + ContentW * 0.5f, TableY + HeadH + RowH * 0.7f, "NO RUN RECORDED", MUTE);
		DrawFooter(TableY + h);
		return;
	}

	// 先頭が最高点。棒の長さの基準にする
	double best = m_rows[0].score;
	for (const Row& r : m_rows) { best = std::max(best, r.score); }
	best = std::max(best, 1.0);

	for (int i = 0; i < static_cast<int>(m_rows.size()); ++i)
	{
		const Row& r = m_rows[i];
		const float ry = TableY + HeadH + i * RowH;
		const bool first = (i == 0);

		if (first) { U::RectTL(PadX + 2.0f, ry, ContentW - 4.0f, RowH, ACID); }
		if (i > 0) { U::LineD(PadX, ry, PadX + ContentW, ry, 2.0f, INK); }

		// 行の中では、フォントごとに中央へ揃える。
		// 全部を同じ数値で下げると、大きいフォントほど下へはみ出す。
		auto rowY = [ry](int fontId) { return ry + CenterInBox(RowH, FontPx(fontId)); };

		char rank[8];
		snprintf(rank, sizeof(rank), "%02d", i + 1);
		U::TextAt(FontTab, kColX[0], rowY(FontTab), rank, INK);

		U::TextAt(FontCard, kColX[1], rowY(FontCard), r.driver, INK);

		// 点数の棒。1位の行はアシッド塗りの上なので、棒を墨へ反転させる
		const float by = ry + (RowH - BarH) * 0.5f;
		U::RectTL(kColX[2], by, BarW, BarH, INK);
		U::RectTL(kColX[2], by, BarW * static_cast<float>(r.score / best), BarH,
		          first ? INK : ACID);

		char sc[32];
		snprintf(sc, sizeof(sc), "%d", static_cast<int>(r.score));
		U::TextAt(FontRow, kColX[2] + BarW + BarGap, rowY(FontRow), sc, INK);

		char cb[16];
		snprintf(cb, sizeof(cb), "x%d", r.combo);
		U::TextAt(FontCard, kColX[3], rowY(FontCard), cb, INK);
	}

	DrawFooter(TableY + h);
}

void ResultsUI::DrawFooter(float tableBottom)
{
	const float by = tableBottom + FooterGap;
	U::Button(PadX, by, ButtonW, ButtonH, "BACK", U::BtnKind::Primary, "ESC");
}
