#include "PauseUI.h"
#include "../../Input/HjKeyInput.h"

using namespace UIConst;
using namespace PauseConst;
namespace U = HjUI;

namespace
{
	// 行き先があるものだけを並べる。押しても何も起きない項目は置かない
	const char* kLabels[3] = { "RESUME", "RESTART RUN", "END RUN" };
	constexpr int kCount = 3;

	const PauseUI::Action kActions[3] =
	{
		PauseUI::Action::Resume,
		PauseUI::Action::Restart,
		PauseUI::Action::EndRun,
	};
}


//----------------------------------------------------------
// 開閉。
//
// 開いた瞬間、押されているキーは「今押した」とみなさない。
// ポーズを開くのに使ったESCがそのまま「閉じる」として拾われて、
// 1フレームで開いて閉じることになるため。
//----------------------------------------------------------
void PauseUI::SetVisible(bool visible)
{
	if (visible == m_visible) { return; }

	m_visible = visible;
	if (!visible) { return; }

	m_sel = 0;
	m_action = Action::None;
	// 開いたキー(ESC)がそのまま「閉じる」として拾われないよう、
	// 今押されているものを消化しておく
	HjKeyInput::Instance().ConsumeAll();
}

void PauseUI::Update()
{
	if (!m_visible) { return; }

	U::BeginInput();

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Up)) { m_sel = (m_sel + kCount - 1) % kCount; }
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Down)) { m_sel = (m_sel + 1) % kCount; }

	for (int i = 0; i < kCount; ++i)
	{
		const float y = MenuY + i * MenuH;
		if (U::Hover(MenuX, y, MenuW, MenuH))   { m_sel = i; }
		if (U::Clicked(MenuX, y, MenuW, MenuH)) { m_sel = i; m_action = kActions[i]; }
	}

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide)) { m_action = kActions[m_sel]; }
	// ESCは「戻る」。開いたキーで閉じられる方が迷わない
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel)) { m_action = Action::Resume; }
}

void PauseUI::DrawSpriteOverlay()
{
	if (!m_visible) { return; }

	// 止めた映像を暗く落とす。
	// 落とさないと背景の情報量にメニューが負けて、どこが選ばれているか分からない
	{
		Math::Color dim = INK; dim.w = DimAlpha;
		U::RectTL(0.0f, 0.0f, CanvasW, CanvasH, dim);
	}

	U::TextAt(FontFoot, PadX, KickerY, "RUN PAUSED", ACID);
	// 伸ばす倍率が大きいほど元のドットが見える。近い大きさのフォントから伸ばす
	U::TextScaled(FontSub, PadX, TitleY, TitlePx, "PAUSE", WHITE);

	DrawMenu();
	DrawScorePanel();

	// 映像を暗く落とした上に置くので、既定の墨色だと沈んで読めない
	U::Keycap(PadX, KeycapY, "ESC", "RESUME", WHITE);
	U::Keycap(PadX + 160.0f, KeycapY, "ENTER", "SELECT", WHITE);
}

//----------------------------------------------------------
// メニュー。選んでいる行だけ塗り、他は下に罫線。
//----------------------------------------------------------
void PauseUI::DrawMenu()
{
	for (int i = 0; i < kCount; ++i)
	{
		const float y = MenuY + i * MenuH;
		const bool on = (i == m_sel);

		if (on)
		{
			U::RectTL(MenuX, y, MenuW, MenuH, ACID);
		}
		else
		{
			Math::Color rule = WHITE; rule.w = 0.35f;
			U::LineD(MenuX, y + MenuH, MenuX + MenuW, y + MenuH, 2.0f, rule);
		}
		U::TextAt(FontMenu, MenuX + MenuLabelDx,
		          y + CenterInBox(MenuH, FontPx(FontMenu)),
		          kLabels[i], on ? INK : WHITE);
	}
}

//----------------------------------------------------------
// 右の成績。今どこまで稼いだかを止まった状態で確かめられる。
//----------------------------------------------------------
void PauseUI::DrawScorePanel()
{
	const float x = CanvasW - PadX - PanelW;

	// 見出し。何の数字なのかを大きく出す。
	// 数字だけ並んでいても、合計なのか今回ぶんなのかが分からない。
	U::TextScaled(FontSub, x, PanelTitleY, PanelTitlePx, "TOTAL", ACID);

	Math::Color edge = WHITE;
	U::FrameTL(x, PanelY, PanelW, PanelRowH * 2.0f, 2.0f, edge);
	U::LineD(x, PanelY + PanelRowH, x + PanelW, PanelY + PanelRowH, 2.0f, edge);

	Math::Color dim = WHITE; dim.w = 0.75f;

	// 2行のパネル。行ごとに、フォントの高さで中央へ揃える。
	// 共通の固定値で下げると、大きいフォントほど下へはみ出す。
	const float labelY = PanelY + CenterInBox(PanelRowH, FontPx(FontFoot));
	const float valueY = PanelY + CenterInBox(PanelRowH, FontPx(FontTab));

	U::TextAt(FontFoot, x + 16.0f, labelY, "SCORE", dim);
	char sc[32];
	snprintf(sc, sizeof(sc), "%d", static_cast<int>(m_total));
	U::TextAtR(FontTab, x + PanelW - 16.0f, valueY, sc, WHITE);

	U::TextAt(FontFoot, x + 16.0f, labelY + PanelRowH, "CHAIN", dim);
	char cb[16];
	snprintf(cb, sizeof(cb), "x%d", m_combo);
	U::TextAtR(FontTab, x + PanelW - 16.0f, valueY + PanelRowH, cb, ACID);
}
