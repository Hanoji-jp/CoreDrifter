#pragma once

// ドリフト採点＆スコア表示演出の定数。
// マンダラート「スコア・採点」＋「演出：スコア表示(数字パンチ)」に対応。
namespace DriftScoreConst
{
	// ── ドリフト判定 ──
	constexpr float MinSpeedKmh = 25.0f;   // これ未満は採点しない
	constexpr float MinAngleDeg = 8.0f;    // 横滑り角これ未満はドリフト扱いしない
	constexpr float GraceTime   = 0.6f;    // 途切れ猶予(秒)。超えたらチェーン確定

	// ── 加点(角度×速度×時間) ──
	constexpr float ScoreRate   = 0.10f;   // 加点係数: angleDeg * speedMps * Rate * dt

	// ── コンボ(倍率) ──
	constexpr float ComboStep   = 1.2f;    // この秒数継続ごとに倍率+1
	constexpr int   ComboMax    = 9;

	// ── 判定しきい値(チェーン確定スコア) ──
	constexpr float NiceScore    = 500.0f;
	constexpr float GreatScore   = 2500.0f;
	constexpr float PerfectScore = 7000.0f;

	// ── 演出(パンチ・フラッシュ) ──
	constexpr float PunchMax    = 0.7f;    // ライブ数字の追加スケール上限(1.0+これ)
	constexpr float PunchGain   = 0.0006f; // 加点1あたりのパンチ増分
	constexpr float PunchDecay  = 5.0f;    // パンチ減衰速度(/秒)
	constexpr float BankPunch   = 0.7f;    // 確定時のパンチ量
	constexpr float BankFlash   = 1.3f;    // GREAT/PERFECT表示時間(秒)

	// ── ドリフト中ビネット(演出③) ──
	constexpr float VignetteMax    = 1.0f;   // 最大強度
	constexpr float VignetteSmooth = 6.0f;   // 追従の速さ(大きいほど機敏)
	constexpr float VignetteAngle  = 40.0f;  // この角度(deg)で強度最大
	constexpr float VignetteSpeed  = 80.0f;  // この速度(km/h)以上で強度フル

	// ── 表示位置(デザイン座標 1536x864) ──
	constexpr float LiveCx  = 768.0f, LiveCy  = 150.0f;   // ライブスコア(中央上)
	constexpr float ComboCx = 768.0f, ComboCy = 210.0f;   // コンボ倍率
	constexpr float JudgeCx = 768.0f, JudgeCy = 300.0f;   // GREAT/PERFECT
	constexpr float TotalCx = 1400.0f, TotalCy = 70.0f;   // 総合スコア(右上)
}
