#pragma once

// マルチプレイ周りの画面(LOBBY / MATCHMAKING / RESULTS)で共通の配置。
// デザイン座標(1536x864, 左上原点)。
//
// 3画面とも「見出し → 中身 → 下段の操作」の同じ骨格で組む。
// 画面ごとに骨格が違うと、遷移するたびに目が置き場所を探し直すことになる。
namespace MultiConst
{
	constexpr float CanvasW = 1536.0f, CanvasH = 864.0f;
	constexpr float PadX = 64.0f;
	constexpr float ContentW = CanvasW - PadX * 2.0f;

	//===== 見出し =====
	constexpr float KickerY = 56.0f;    // 小さい上の文字
	constexpr float TitleY  = 118.0f;   // 大見出し
	constexpr float TitlePx = 47.0f;

	//===== 表 =====
	constexpr float TableY = 212.0f;
	constexpr float RowH   = 74.0f;
	constexpr float HeadH  = 44.0f;     // HjUI::TableHead の高さと合わせる
	// ※行の中の文字は CenterInBox(RowH, フォントの高さ) で都度求める。
	//   共通の固定値だと、大きいフォントほど下へはみ出す。

	//===== 絞り込みのチップ =====
	// 文字の高さに対して上下の余白を広く取る。
	// 詰まっていると窮屈に見えるうえ、押せる範囲も狭くなる。
	constexpr float FilterY = 152.0f, FilterH = 58.0f;
	constexpr float FilterGap = 12.0f, FilterPadX = 22.0f;

	//===== 下段の操作 =====
	constexpr float ButtonH = 50.0f;
	constexpr float ButtonW = 210.0f, ButtonGap = 30.0f;
	constexpr float FooterGap = 34.0f;   // 表の下端からの距離

	//===== 装飾 =====
	// 罫線と点だけで余白を締める。塗りはアシッドだけに取っておく
	constexpr float GuideAlpha = 0.07f;
	constexpr float TickAlpha  = 0.22f;
	constexpr float DotAlpha   = 0.45f;
	constexpr float DotGap     = 13.0f;

	//===== MATCHMAKING =====
	// 参加者の枠。埋まればアシッド、次の1つが点滅、残りは薄い枠
	constexpr int   PipMax = 4;
	constexpr float PipSize = 74.0f, PipGap = 12.0f, PipY = 320.0f;
	constexpr float PipBlinkHz = 1.6f;
	// 下の3つ組(区分・待ち時間・種目)
	constexpr float CellY = 486.0f, CellW = 200.0f, CellH = 92.0f;
	constexpr float CellLabelDy = 32.0f, CellValueDy = 66.0f;
	constexpr float ReadoutY = 418.0f;

	//===== RESULTS =====
	// 得点の棒。1位を基準に長さを決めるので、差がそのまま長さの差になる
	constexpr float BarW = 130.0f, BarH = 12.0f;
	constexpr float BarGap = 12.0f;
}
