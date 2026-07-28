#include "SettingsUI.h"

namespace
{
	const char* kTabs[6]  = { "GAMEPLAY", "CONTROLS", "GRAPHICS", "AUDIO", "UI", "OTHER" };
	const char* kRows[9]  = { "DIFFICULTY", "TRANSMISSION", "TRACTION CONTROL", "STABILITY ASSIST",
							   "ABS", "STEERING LINE", "DAMAGE", "REWIND", "UNITS" };
	// 行→トグルindex(トグル行のみ有効。-1=ステッパー)。TRACTION(row2)だけaccent(黒塗り)。
	const int kToggleOf[9] = { -1, -1, 0, 1, 2, 3, -1, 4, -1 };

	const char* kDiff[3]  = { "EASY", "NORMAL", "HARD" };
	const char* kTrans[2] = { "AUTOMATIC", "MANUAL" };
	const char* kDmg[3]   = { "LOW", "NORMAL", "HIGH" };
	const char* kUnits[2] = { "KM/H", "MPH" };

	// レイアウト(デザイン座標。参考drift_settings最新版に一致)
	const float kPADX = 64.0f, kPADY = 48.0f, kTitleY = kPADY - 12.0f;   // 見出しを上へ
	const float kPanelX = 408.0f, kPanelY = 150.0f, kPanelW = 880.0f, kRowH = 66.0f;   // パネル位置(右へ)・上へ
	const float kTabX = 64.0f, kTabY = kPanelY + 60.0f, kTabStep = 46.0f;   // タブはパネルより下げる
	const float kCtrlW = 158.0f;                                    // コントロール域の幅
	const float kCtrlX = kPanelX + kPanelW - 24.0f - kCtrlW;        // ステッパーの左端
	const float kToggleW = 124.0f;                                  // トグル幅(segW62×2)
	const float kToggleX = kPanelX + kPanelW - 24.0f - kToggleW;    // トグルは右端に揃える(右へ)
	const int   kRowCount = 9;
}

void SettingsUI::Init() {}

void SettingsUI::Update()
{
	const bool up = (GetAsyncKeyState(VK_UP)   & 0x8000) != 0 || (GetAsyncKeyState('W') & 0x8000) != 0;
	const bool dn = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 || (GetAsyncKeyState('S') & 0x8000) != 0;
	const bool lf = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 || (GetAsyncKeyState('A') & 0x8000) != 0;
	const bool rt = (GetAsyncKeyState(VK_RIGHT)& 0x8000) != 0 || (GetAsyncKeyState('D') & 0x8000) != 0;

	if (up && !m_prevUp) { m_row = (m_row + kRowCount - 1) % kRowCount; }
	if (dn && !m_prevDn) { m_row = (m_row + 1) % kRowCount; }

	auto stepRow = [&](int dir)
	{
		const int tg = kToggleOf[m_row];
		if (tg >= 0) { m_toggles[tg] = (dir > 0); }
		else switch (m_row)
		{
		case 0: m_diff   = (m_diff + dir + 3) % 3;   break;
		case 1: m_trans  = (m_trans + dir + 2) % 2;  break;
		case 6: m_damage = (m_damage + dir + 3) % 3; break;
		case 8: m_units  = (m_units + dir + 2) % 2;  break;
		default: break;
		}
	};
	if (rt && !m_prevR) { stepRow(1); }
	if (lf && !m_prevL) { stepRow(-1); }

	m_prevUp = up; m_prevDn = dn; m_prevL = lf; m_prevR = rt;

	// ── マウス ──
	HjUI::BeginInput();
	for (int i = 0; i < 6; ++i)   // タブ切替
	{
		if (HjUI::Clicked(kTabX - 14.0f, kTabY + i * kTabStep - 6.0f, 200.0f, 38.0f)) { m_tab = i; }
	}
	for (int i = 0; i < kRowCount; ++i)
	{
		const float ry = kPanelY + i * kRowH;
		if (HjUI::Hover(kPanelX, ry, kPanelW, kRowH)) { m_row = i; }
		const float cy = ry + kRowH * 0.5f - 17.0f;
		const int tg = kToggleOf[i];
		if (tg >= 0)
		{
			if (HjUI::Clicked(kToggleX, cy, 62.0f, 34.0f))         { m_toggles[tg] = true; }
			if (HjUI::Clicked(kToggleX + 62.0f, cy, 62.0f, 34.0f)) { m_toggles[tg] = false; }
		}
		else
		{
			int dir = 0;
			if (HjUI::Clicked(kToggleX, cy, 40.0f, 34.0f))                    { dir = -1; }
			if (HjUI::Clicked(kToggleX + kToggleW - 40.0f, cy, 40.0f, 34.0f)) { dir = 1; }
			if (dir != 0)
			{
				switch (i) { case 0: m_diff=(m_diff+dir+3)%3; break; case 1: m_trans=(m_trans+dir+2)%2; break;
							 case 6: m_damage=(m_damage+dir+3)%3; break; case 8: m_units=(m_units+dir+2)%2; break; }
			}
		}
	}
}

void SettingsUI::DrawSprite()
{
	using namespace UIConst;
	namespace U = HjUI;

	auto& sp = KdShaderManager::Instance().m_spriteShader;

	// 背景(紙)
	sp.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	// ── 装飾(参考drift_settings最新版) ──
	// 薄い縦の構築ガイド＋クロップティック(+)
	const Math::Color guide = { 0.078f, 0.078f, 0.078f, 0.07f };
	const Math::Color tick  = { 0.078f, 0.078f, 0.078f, 0.22f };
	U::LineD(360.0f,  0.0f, 360.0f,  864.0f, 1.0f, guide);
	U::LineD(1300.0f, 0.0f, 1300.0f, 864.0f, 1.0f, guide);
	U::LineD(1400.0f, 0.0f, 1400.0f, 864.0f, 1.0f, guide);
	auto cropTick = [&](float x, float y) { U::LineD(x - 5, y, x + 5, y, 1.5f, tick); U::LineD(x, y - 5, x, y + 5, 1.5f, tick); };
	cropTick(360.0f, 70.0f); cropTick(1300.0f, 70.0f); cropTick(360.0f, 800.0f);

	// 右レール：縦書き(90°回転)の"FOCUS // ADAPT // OVERCOME"
	U::TextRotated(FontFoot, 1510.0f, 320.0f, "FOCUS // ADAPT // OVERCOME", INK, 0.18f * 13.0f);
	// アシッドのコーナーマーク＋斜線
	U::RectTL(1476.0f, 60.0f, 40.0f, 80.0f, ACID, true);
	U::LineD(1476.0f, 140.0f, 1516.0f, 60.0f, 1.5f, INK);
	U::LineD(1476.0f, 116.0f, 1500.0f, 60.0f, 1.5f, INK);
	// 右レール下の +/◆ 列
	auto diamond = [&](float dcx, float dcy) {
		const int cx = static_cast<int>(U::MapX(dcx)), cy = static_cast<int>(U::MapY(dcy)), r = 4;
		sp.DrawTriangle(cx, cy - r, cx - r, cy, cx, cy + r, &INK, true);
		sp.DrawTriangle(cx, cy - r, cx + r, cy, cx, cy + r, &INK, true);
	};
	// +/◆ 列は上下フロート
	const float T = U::Time();
	const float f1 = std::sin(T * 1.5f) * 4.0f, f2 = std::sin(T * 1.5f + 1.2f) * 4.0f;
	U::TextC(FontTab, 1496.0f, 620.0f + f1, 18.0f, "+", INK);
	U::TextC(FontTab, 1496.0f, 656.0f + f2, 18.0f, "+", INK);
	diamond(1496.0f, 700.0f + f1); diamond(1496.0f, 728.0f + f2); diamond(1496.0f, 756.0f + f1);

	// 明滅ドット(四隅)
	U::DotFieldTwinkle(1180.0f, 780.0f, 120.0f, 40.0f, DOTS, 0.0f);
	U::DotFieldTwinkle(60.0f,   40.0f, 110.0f, 34.0f, DOTS, 1.7f);

	// 見出し SETTINGS ＋ アシッド下線
	const float endX = U::Text(FontHead, kPADX, kTitleY, 47.0f, "SETTINGS", INK);
	const float uw = (endX - U::MapX(kPADX)) / Scale;
	U::RectTL(kPADX, kTitleY + 72.0f, uw, 5.0f, ACID, true);

	// 右上デコ + +
	U::Text(FontTab, DesignW - 124.0f, kPADY - 4.0f, 24.0f, "+ +", INK);

	// 左タブ
	for (int i = 0; i < 6; ++i)
	{
		const float rowY = kTabY + i * kTabStep;
		const bool active = (i == m_tab);
		if (active)
		{
			const float w = U::Measure(FontTab, kTabs[i], 0.0f) / Scale + 28.0f;
			U::RectTL(kTabX - 14.0f, rowY - 6.0f, w, 38.0f, ACID, true);
		}
		U::Text(FontTab, kTabX, rowY, 20.0f, kTabs[i], active ? INK : MUTE);
	}

	// 右パネル(枠＋行罫線)
	U::FrameTL(kPanelX, kPanelY, kPanelW, kRowH * kRowCount, 2.0f, INK);
	for (int i = 0; i < kRowCount; ++i)
	{
		const float ry = kPanelY + i * kRowH;
		if (i == m_row)
		{
			// 選択中：行全体に薄いアシッドのハイライト＋左に太めのアシッドバー
			Math::Color wash = ACID; wash.w = 0.22f;
			U::RectTL(kPanelX, ry, kPanelW, kRowH, wash, true);
			U::RectTL(kPanelX, ry, 10.0f, kRowH, ACID, true);
		}
		if (i > 0) { U::LineD(kPanelX, ry, kPanelX + kPanelW, ry, 2.0f, INK); }

		U::Text(FontRow, kPanelX + 24.0f, ry + kRowH * 0.5f - 8.0f, 16.0f, kRows[i], INK);

		const float cy = ry + kRowH * 0.5f - 17.0f;
		const int tg = kToggleOf[i];
		if (tg >= 0)
		{
			U::Toggle(kToggleX, cy, m_toggles[tg], (i == 2) ? INK : ACID);   // 右寄せ・TRACTIONだけ黒塗り
		}
		else
		{
			const char* val = "";
			switch (i) { case 0: val = kDiff[m_diff]; break; case 1: val = kTrans[m_trans]; break;
						 case 6: val = kDmg[m_damage]; break; case 8: val = kUnits[m_units]; break; }
			U::Stepper(kToggleX, cy, kToggleW, val);   // トグルと同じ枠に揃える
		}
	}

	// 下部キーキャップ
	const float fy = kPanelY + kRowH * kRowCount + 60.0f;
	const float used = U::Keycap(kPADX, fy, "R", "RESET TO DEFAULT");
	U::Keycap(kPADX + used + 32.0f, fy, "ESC", "BACK");
}
