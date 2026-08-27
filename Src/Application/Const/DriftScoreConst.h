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
	constexpr float GreatScore   = 2500.0f;
	constexpr float PerfectScore = 7000.0f;

	// ── 演出 ──
	// ※数字の拡大(パンチ)はスコア表示ごとHUDへ移したので持たない
	constexpr float BankFlash   = 1.3f;    // GREAT/PERFECT表示時間(秒)

	// ── ドリフト中ビネット(演出③) ──
	constexpr float VignetteMax    = 1.0f;   // 最大強度
	constexpr float VignetteSmooth = 6.0f;   // 追従の速さ(大きいほど機敏)
	constexpr float VignetteAngle  = 40.0f;  // この角度(deg)で強度最大
	constexpr float VignetteSpeed  = 80.0f;  // この速度(km/h)以上で強度フル

	// ※ドリフト成立を示すバナーは廃止。
	//   角度はHUD(RunHudUI)が中央下で出しており、二重になっていた。

	// ── 判定文字(GREAT/PERFECT)の動き ──
	// 出る・保つ・抜ける の3段に分ける。単なるフェードだと当たった感じが出ない。
	constexpr float JudgeSlamTime  = 0.11f;  // 大きい状態から叩きつけるまでの時間(秒)
	constexpr float JudgeSlamScale = 2.8f;   // 叩きつけ開始時の大きさ
	constexpr float JudgeOutRatio  = 0.28f;  // 終わりのこの割合で抜けていく
	constexpr float JudgeShakeTime = 0.16f;  // 着弾直後に揺れる時間(秒)
	constexpr float JudgeShakeAmp  = 9.0f;   // 揺れ幅(デザインpx)

	// ── 判定文字の見た目(既存の文字流体化を使う) ──
	// 自前のフォント描画ではなく、KdPostProcessShaderの文字エフェクトへ渡す。
	// 文字を焼いたRTをドメインワープでウネらせる仕組みが既にあるので、
	// 判定という一番の見せ場にはそちらを使う。
	//   Style 1 = 虹スモーク → PERFECT(最上位。滅多に出ない色)
	//   Style 2 = 炎         → GREAT
	constexpr int FluidStylePerfect = 1;
	constexpr int FluidStyleGreat   = 2;

	// 表示枠(画面比。0.5=中央)
	constexpr float FluidCX = 0.5f, FluidCY = 0.35f;
	constexpr float FluidW  = 0.58f, FluidH = 0.20f;
	constexpr float FluidShake = 0.010f;   // 着弾時に揺らす幅(画面比)

	// 着弾に合わせて広がる輪。衝撃の向きと大きさを目で示す
	constexpr float RingTime   = 0.42f;
	constexpr float RingRadius = 300.0f;

	// 着弾の瞬間だけ画面を白く飛ばす。強すぎると走行が見えなくなる
	constexpr float FlashTime = 0.22f;
	constexpr float FlashMax  = 0.30f;

	// ── 表示位置(デザイン座標 1536x864) ──
	// ※スコアと倍率はHUD(RunHudUI)が出すので、ここには位置を持たない
	constexpr float JudgeCx = 768.0f, JudgeCy = 300.0f;   // GREAT/PERFECT
}
