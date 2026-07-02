#pragma once

// ポストプロセスエフェクトに関わる定数
namespace PostProcessConst
{
	// ---- モーションブラー ----

	// カメラ移動量がこれ未満ならブラーをスキップ（大きいほど発動しにくい）
	constexpr float MotionBlurSpeedThreshold = 0.2f;

	// 移動量をブラーの強さに変換するスケール
	constexpr float MotionBlurScale = 0.08f;

	// ブラーサンプリング数
	constexpr int MotionBlurSamplingRadius = 6;

	// ブラー強度の最大クランプ（UV オフセット上限）
	constexpr float MotionBlurMaxStrength = 0.03f;

	// ---- 被ダメ赤フラッシュ ----
	constexpr float DamageFlashDuration  = 0.4f;   // フラッシュ継続時間（秒）
	constexpr float DamageFlashIntensity = 0.55f;  // ビネット最大強度 (0〜1)
}

