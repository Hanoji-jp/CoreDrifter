#pragma once

// 走行中の通知の見た目と時間。デザイン座標(1536x864)。
namespace ToastConst
{
	constexpr int   MaxStack = 4;      // 同時に出す上限
	constexpr float X = 64.0f, Y = 152.0f;
	// 中のチップ(高さ42)より十分高くしないと、枠が中身に張り付いて窮屈になる。
	// 幅も、本文と数値が横に並んでぶつからない長さを取る。
	constexpr float W = 620.0f, H = 78.0f, Gap = 16.0f;

	// 左から滑り込む。位置が動くと視界の端でも気付ける
	constexpr float SlideTime = 0.24f, SlideDist = 40.0f;
	// 出したままにしない。走行の邪魔になる
	constexpr float HoldTime = 2.6f, FadeTime = 0.5f;

	// 中身の配置(枠の左上からの距離)
	constexpr float PadX = 20.0f;
	// 本文と数値の間に必ず空ける幅。これが無いと長い本文が数値へ食い込む
	constexpr float ValueGap = 24.0f;
	constexpr float BgAlpha = 0.50f;   // 映像が透ける程度に留める
}
