#pragma once

// ポーズ画面。デザイン座標(1536x864)。
namespace PauseConst
{
	constexpr float CanvasW = 1536.0f, CanvasH = 864.0f;
	constexpr float PadX = 64.0f;

	// 止めた映像を暗く落とす。落とさないと、
	// 背景の情報量にメニューが負けて読めない
	constexpr float DimAlpha = 0.72f;

	//===== 見出し =====
	// 小見出しのすぐ下に大見出しを置く。
	// 文字は上端が基準なので、間を空けすぎると大見出しだけが下へ落ちて見える。
	constexpr float KickerY = 110.0f;
	constexpr float TitleY  = 132.0f, TitlePx = 96.0f;

	//===== メニュー =====
	constexpr float MenuX = 64.0f, MenuY = 278.0f;
	constexpr float MenuW = 420.0f, MenuH = 56.0f;
	constexpr float MenuLabelDx = 22.0f;
	// ※行内の縦位置は CenterInBox(MenuH, フォントの高さ) で都度求める

	//===== 右の成績 =====
	constexpr float PanelW = 420.0f, PanelY = 278.0f;
	constexpr float PanelRowH = 54.0f;
	// パネルの見出し。何の数字なのかが分からないと、
	// 「合計なのか今回ぶんなのか」を読み手が判断できない。
	constexpr float PanelTitleY  = 232.0f;   // 枠の上に置く
	constexpr float PanelTitlePx = 40.0f;
	// ※行内の縦位置は CenterInBox(PanelRowH, フォントの高さ) で都度求める

	//===== 下段 =====
	constexpr float KeycapY = 700.0f;
}
