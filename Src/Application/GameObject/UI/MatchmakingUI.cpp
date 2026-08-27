#include "MatchmakingUI.h"
#include "../../Input/HjKeyInput.h"

using namespace UIConst;
using namespace MultiConst;
namespace U = HjUI;

void MatchmakingUI::Update()
{
	U::BeginInput();

	const float dt = KdFPSController::GetDt();
	m_elapsed += dt;

	// 点滅は時間で回す。止まっていると探しているように見えない
	m_blink += dt * PipBlinkHz;
	while (m_blink >= 1.0f) { m_blink -= 1.0f; }

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel)) { m_cancel = true; }
}

void MatchmakingUI::DrawSprite()
{
	DrawFrame("MATCHMAKING", "SEARCHING");

	// 見出しの末尾に点を足して、待っていることを字面でも出す
	{
		const float w = U::Measure(FontHead, "SEARCHING", -0.03f * 60.0f) / Scale;
		U::TextAt(FontHead, PadX + w + 8.0f, TitleY, "...", ACID);
	}

	DrawGuides(470.0f, 240.0f, 320.0f);
	{
		Math::Color dot = INK; dot.w = DotAlpha;
		U::Deco::DotGrid(70.0f, 740.0f, 12, 4, DotGap, dot);
	}

	// 反転した帯。文の役割が説明であることを面で示す
	{
		const char* strap = "FINDING DRIVERS ON THE SAME NETWORK  +";
		const float w = U::Measure(FontRow, strap, 0.0f) / Scale + 32.0f;
		U::RectTL(PadX, 190.0f, w, 40.0f, INK);
		U::TextAt(FontRow, PadX + 16.0f,
		          190.0f + CenterInBox(40.0f, FontPx(FontRow)), strap, WHITE);
	}

	DrawPips();
	DrawReadout();
	DrawCells();

	U::Button(PadX, 636.0f, ButtonW, ButtonH, "CANCEL SEARCH",
	          U::BtnKind::Secondary, "ESC");
}

//----------------------------------------------------------
// 参加者の枠。
// 埋まればアシッド、次の1つが点滅、残りは薄い枠。
// 「次はここが埋まる」と見えていると、待ち時間が進行に見える。
//----------------------------------------------------------
void MatchmakingUI::DrawPips()
{
	for (int i = 0; i < PipMax; ++i)
	{
		const float x = PadX + i * (PipSize + PipGap);
		const bool filled = (i < m_found);
		const bool next   = (i == m_found);

		Math::Color edge = INK;
		if (filled || (next && m_blink < 0.5f))
		{
			U::RectTL(x, PipY, PipSize, PipSize, ACID);
		}
		else
		{
			edge.w = 0.40f;
		}
		U::FrameTL(x, PipY, PipSize, PipSize, 2.0f, edge);
	}
}

void MatchmakingUI::DrawReadout()
{
	char buf[64];

	snprintf(buf, sizeof(buf), "DRIVERS FOUND %d / %d", m_found, PipMax);
	U::TextAt(FontRow, PadX, ReadoutY, buf, INK);

	const int sec = static_cast<int>(m_elapsed);
	snprintf(buf, sizeof(buf), "ELAPSED %02d:%02d", sec / 60, sec % 60);
	U::TextAt(FontRow, PadX + 300.0f, ReadoutY, buf, INK);

	U::TextAt(FontRow, PadX + 520.0f, ReadoutY, "NETWORK  LOCAL", INK);
}

//----------------------------------------------------------
// 下の3つ組。最後の1つだけ塗って、今の種目を強く出す。
//----------------------------------------------------------
void MatchmakingUI::DrawCells()
{
	const char* lbl[3] = { "PLAYERS", "CONNECTION", "MODE" };
	char cap[16];
	snprintf(cap, sizeof(cap), "%d", PipMax);
	const char* val[3] = { cap, "DIRECT IP", "DRIFT" };

	U::FrameTL(PadX, CellY, CellW * 3.0f, CellH, 2.0f, INK);

	for (int i = 0; i < 3; ++i)
	{
		const float x = PadX + i * CellW;
		if (i == 2) { U::RectTL(x, CellY, CellW, CellH, ACID); }
		if (i < 2)  { U::LineD(x + CellW, CellY, x + CellW, CellY + CellH, 2.0f, INK); }

		U::TextAt(FontFoot, x + 20.0f, CellY + CellLabelDy, lbl[i],
		        (i == 2) ? INK : SUBTXT);
		U::TextAt(FontCard, x + 20.0f, CellY + CellValueDy, val[i], INK);
	}
}
