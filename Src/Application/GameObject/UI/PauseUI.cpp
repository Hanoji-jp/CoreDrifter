#include "PauseUI.h"
#include "../../Input/HjKeyInput.h"
#include "../../Audio/HjAudioSettings.h"
#include "../../Audio/HjAudioSpace.h"

using namespace UIConst;
using namespace PauseConst;
namespace U = HjUI;

namespace
{
	// 行き先があるものだけを並べる。押しても何も起きない項目は置かない
	// 行き先があるものだけを並べる。押しても何も起きない項目は置かない
	const char* kLabels[4] = { "RESUME", "SETTINGS", "RESTART RUN", "END RUN" };
	constexpr int kCount = 4;

	// SETTINGS だけはシーンへ返す操作ではなく、この画面の中で窓を開く。
	// None を入れておいて、選ばれたときに窓を開く側で拾う
	const PauseUI::Action kActions[4] =
	{
		PauseUI::Action::Resume,
		PauseUI::Action::None,
		PauseUI::Action::Restart,
		PauseUI::Action::EndRun,
	};

	// 設定ウィンドウの行
	const char* kVolRows[3] = { "SOUND EFFECTS", "MUSIC", "AMBIENCE" };
	constexpr int kVolCount = 3;

	// メニューの何番目が SETTINGS か。並びを変えたときに追いやすいよう名前を付ける
	constexpr int kSettingsIndex = 1;
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
	m_settingsOpen = false;   // 前回開いたまま残さない
	m_settingsRow  = 0;
	// 開いたキー(ESC)がそのまま「閉じる」として拾われないよう、
	// 今押されているものを消化しておく
	HjKeyInput::Instance().ConsumeAll();
}

void PauseUI::Update()
{
	if (!m_visible) { return; }

	U::BeginInput();

	// 窓が開いている間は、後ろのメニューを操作させない。
	// 両方が同じキーを見ると、選択が二重に動いて何が起きたか分からなくなる
	if (m_settingsOpen) { UpdateSettingsWindow(); return; }

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Up)) { m_sel = (m_sel + kCount - 1) % kCount; }
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Down)) { m_sel = (m_sel + 1) % kCount; }

	for (int i = 0; i < kCount; ++i)
	{
		const float y = MenuY + i * MenuH;
		if (U::Hover(MenuX, y, MenuW, MenuH))   { m_sel = i; }
		if (U::Clicked(MenuX, y, MenuW, MenuH))
		{
			m_sel = i;
			if (i == kSettingsIndex) { m_settingsOpen = true; m_settingsRow = 0; }
			else                     { m_action = kActions[i]; }
		}
	}

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide))
	{
		if (m_sel == kSettingsIndex) { m_settingsOpen = true; m_settingsRow = 0; }
		else                         { m_action = kActions[m_sel]; }
	}
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

	// 窓は最後に重ねる。後ろのメニューより手前に出す必要がある
	DrawSettingsWindow();
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

//==========================================================
// 設定ウィンドウ
//
// 走行中に音量だけ直したい、という用がほとんどなので、
// 画面ごと切り替えず小さな窓で出す。
// 画面を移ると走行の映像が消えて、どこで止めたのか分からなくなる。
//==========================================================

float PauseUI::VolumeOf(int row) const
{
	const auto& au = HjAudioSettings::Instance();
	switch (row)
	{
	case 0: return au.GetSfx();
	case 1: return au.GetMusic();
	case 2: return au.GetAmbient();
	default: return 0.0f;
	}
}

void PauseUI::SetVolumeOf(int row, float v)
{
	auto& au = HjAudioSettings::Instance();
	switch (row)
	{
	case 0:
		au.SetSfx(v);
		// 効果音と環境音はバスに掛かるので、変えたら渡し直す
		HjAudioSpace::Instance().SetSfxVolume(au.GetSfx());
		break;
	case 1:
		au.SetMusic(v);
		break;
	case 2:
		au.SetAmbient(v);
		HjAudioSpace::Instance().SetAmbientVolume(au.GetAmbient());
		break;
	default: break;
	}
}

void PauseUI::StepVolume(int row, int dir)
{
	SetVolumeOf(row, VolumeOf(row) + WinVolStep * static_cast<float>(dir));
}

void PauseUI::UpdateSettingsWindow()
{
	auto& key = HjKeyInput::Instance();

	if (key.Pressed(HjKeyInput::Key::Up))   { m_settingsRow = (m_settingsRow + kVolCount - 1) % kVolCount; }
	if (key.Pressed(HjKeyInput::Key::Down)) { m_settingsRow = (m_settingsRow + 1) % kVolCount; }
	if (key.Pressed(HjKeyInput::Key::Right)) { StepVolume(m_settingsRow,  1); }
	if (key.Pressed(HjKeyInput::Key::Left))  { StepVolume(m_settingsRow, -1); }

	// 決定でもESCでも閉じる。
	// 窓の中に「決める」対象が無いので、どちらも「戻る」でよい
	if (key.Pressed(HjKeyInput::Key::Cancel) || key.Pressed(HjKeyInput::Key::Decide))
	{
		m_settingsOpen = false;
		return;
	}

	// マウス。棒をクリックした位置で音量を決める
	const float barX = WinX + WinW - WinRowPadX - WinBarW;
	for (int i = 0; i < kVolCount; ++i)
	{
		const float ry = WinY + WinHeadH + i * WinRowH;
		if (U::Hover(WinX, ry, WinW, WinRowH)) { m_settingsRow = i; }

		if (U::Clicked(barX, ry, WinBarW, WinRowH))
		{
			const float v = std::clamp((U::MouseX() - barX) / std::max(WinBarW, 1.0f), 0.0f, 1.0f);
			SetVolumeOf(i, v);
		}
	}
}

void PauseUI::DrawSettingsWindow()
{
	if (!m_settingsOpen) { return; }

	// 後ろをさらに落とす。
	// 落とさないと、どちらの層を操作しているのか分からない
	{
		Math::Color dim = INK; dim.w = WinDimAlpha;
		U::RectTL(0.0f, 0.0f, CanvasW, CanvasH, dim);
	}

	// 窓。紙色の板に墨の枠。画面の他の要素と同じ組み方
	U::RectTL(WinX, WinY, WinW, WinH, PAPER, true);
	U::FrameTL(WinX, WinY, WinW, WinH, 2.0f, INK);

	// 見出しの帯
	U::RectTL(WinX, WinY, WinW, WinHeadH, INK, true);
	U::TextAt(FontTab, WinX + WinRowPadX,
	          WinY + CenterInBox(WinHeadH, FontPx(FontTab)), "AUDIO", PAPER);

	const float barX = WinX + WinW - WinRowPadX - WinBarW;

	for (int i = 0; i < kVolCount; ++i)
	{
		const float ry = WinY + WinHeadH + i * WinRowH;

		if (i == m_settingsRow)
		{
			// 選択中：薄いアシッドを敷いて、左に太いバー。設定画面と同じ見せ方
			Math::Color wash = ACID; wash.w = 0.22f;
			U::RectTL(WinX + 2.0f, ry, WinW - 4.0f, WinRowH, wash, true);
			U::RectTL(WinX + 2.0f, ry, 8.0f, WinRowH, ACID, true);
		}
		if (i > 0) { U::LineD(WinX, ry, WinX + WinW, ry, 1.5f, INK30); }

		U::TextAt(FontRow, WinX + WinRowPadX + 14.0f,
		          ry + CenterInBox(WinRowH, FontPx(FontRow)), kVolRows[i], INK);

		// 棒。枠を描いて中を値のぶんだけ塗る
		const float v    = VolumeOf(i);
		const float barY = ry + (WinRowH - WinBarH) * 0.5f;

		U::FrameTL(barX, barY, WinBarW, WinBarH, 2.0f, INK);
		U::RectTL(barX + 2.0f, barY + 2.0f,
		          (WinBarW - 4.0f) * std::clamp(v, 0.0f, 1.0f), WinBarH - 4.0f, ACID, true);

		// 数字。%のほうが「半分」などが分かりやすい
		char buf[16];
		snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(v * 100.0f + 0.5f));
		U::TextAt(FontFoot, barX - 46.0f,
		          ry + CenterInBox(WinRowH, FontPx(FontFoot)), buf, INK);
	}

	// 操作説明。窓の下に置く
	const float fy = WinY + WinH + WinFootDy;
	const float used = U::Keycap(WinX, fy, "<>", "ADJUST");
	U::Keycap(WinX + used + 24.0f, fy, "ESC", "CLOSE");
}
