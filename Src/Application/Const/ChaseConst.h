#pragma once

// 追走(D1形式)でCPUが運転するときの定数。
//
// ■ 何をする機構か
// 前を走る車の軌跡を追いかけて、ステアとアクセルを毎フレーム作る。
// 物理は本物を回すので、ぶつかれば飛ぶしスピンもする。
//
// ■ 作りの骨
//   出す値 ＝ 土台 ＋ 補正
//
//   土台 … 手本が既に出している答え(相手の踏み量・いまの滑り角)
//   補正 … 自分と目標のズレ
//
// ゼロから釣り合う操作を計算するのは難しいが、
// 前を走っている人が既に正解を出しているので、それを借りる。
//
// ■ 土台は「近い」ときだけ正しい
// 押し出されて状態が離れると、手本の値は毒になる。
// 横向きで止まりかけているのに、相手の全開をそのまま真似ることになる。
// なので、ズレが大きいほど土台の重みを下げる。
namespace ChaseConst
{
	//===== どこを追うか =====
	// 相手の何秒前の位置を狙うか。
	// 小さいほど詰めるが、当てやすくなる。D1は接触が減点なので余裕を持たせる
	constexpr float FollowDelay = 0.55f;

	// 開始時にプレイヤーの何メートル後ろへ置くか。
	// 重ねて出すと、最初のフレームで押し合って両方が飛ぶ
	constexpr float StartGap = 8.0f;

	// 目標より少し先を見る時間(秒)。
	// 目標点そのものを見ると、着いた時にはもう遅れている。
	// 人が「次のコーナー」を見るのと同じで、先を見ないと曲がり始めが遅れる
	constexpr float LookAhead = 0.35f;

	//===== ステア =====
	// ■ 土台：カウンター
	// ドリフト中、前輪は進んでいる向きを向いていないとグリップを失う。
	// つまり必要なカウンターは滑り角そのもの。
	// これを先に当てておかないと、ズレを見てから直す形になり、
	// 直した頃には行き過ぎていて左右に振れる
	constexpr float CounterGain = 0.95f;   // 1.0=滑り角ぶんそのまま

	// ■ 補正：向きのズレ
	// P だけだと必ず行き過ぎる。D で「いま深くなりつつある」段階から効かせる
	constexpr float SteerP = 1.30f;
	constexpr float SteerD = 0.28f;

	// ステアが動ける速さ(1秒あたり、切れ角に対する割合)。
	// 制限しないと計算どおりに一瞬で切れて、人の操作に見えない
	constexpr float SteerRate = 6.0f;

	//===== アクセル =====
	// ■ 土台：手本の踏み量をそのまま借りる
	constexpr float ThrottleFF = 1.0f;

	// ■ 補正：角度のズレ
	// ドリフト中に角度を支配しているのはアクセル。
	// ステアだけで支えようとすると切れ角の上限に当たって足りなくなる。
	// 深くなったら戻す、浅かったら踏む
	constexpr float AngleP = 0.030f;   // 1度あたりの踏み量
	constexpr float AngleD = 0.008f;

	// ■ 補正：距離のズレ
	// 角度より弱くする。両方を強くすると、
	// 「追いつきたいから踏む」と「深いから戻す」が毎フレーム喧嘩する
	constexpr float GapP = 0.28f;
	constexpr float GapDeadZone = 0.6f;   // この距離(m)以内は詰めにいかない

	// 踏み込みの動ける速さ(1秒あたり)。
	// 制限しないと0と1を往復して、音も挙動もばたつく
	constexpr float ThrottleRate = 4.0f;

	//===== 土台の重み =====
	// ズレがこの範囲なら手本を全面的に信じる
	constexpr float TrustNearDeg = 8.0f;    // 角度のズレ(度)
	constexpr float TrustNearM   = 2.0f;    // 位置のズレ(m)

	// ここまで離れたら手本を一切使わない。
	// 間は滑らかに下げる(境目で挙動が飛ばないように)
	constexpr float TrustFarDeg = 35.0f;
	constexpr float TrustFarM   = 8.0f;

	//===== 変速 =====
	// 人と同じで、CPUも自分でギアを変える必要がある。
	// 1速のままだと吹け切って、そこから先へ進めない
	constexpr float ShiftUpRpm   = 0.86f;   // 回転がここまで上がったら上げる
	constexpr float ShiftDownRpm = 0.32f;   // ここまで落ちたら下げる

	// 変速してから次まで待つ時間(秒)。
	// 待たないと、上げた直後に「回転が落ちた」と判断して下げ、
	// また上げる、を繰り返す
	constexpr float ShiftWait = 0.45f;

	//===== 滑り出し =====
	// 角度が欲しいのに滑っていないとき、連続の制御では作れない。
	// きっかけとしてサイドを短く引く
	constexpr float KickNeedDeg  = 14.0f;   // 目標がこれ以上深いのに
	constexpr float KickHaveDeg  = 5.0f;    // 実際はこれ以下しか滑っていない
	constexpr float KickMinSpeed = 8.0f;    // この速度(m/s)未満では引かない
	constexpr float KickHold     = 0.22f;   // 引いている時間(秒)
	constexpr float KickCooldown = 1.20f;   // 次に引けるまで

	//===== 復帰 =====
	// カウンターで支えきれない角度。
	// 前輪の切れ角には上限があるので、それを超えたら姿勢は戻せない
	constexpr float SpinDeg      = 70.0f;
	// 回転が速すぎる(rad/s)
	constexpr float SpinYawRate  = 2.6f;
	// 線から離れすぎ(m)
	constexpr float LostDist     = 14.0f;

	// 復帰中の踏み量の上限。
	// 横を向いたまま踏むと、戻るどころか回り続ける
	constexpr float RecoverThrottle = 0.45f;

	// これらを下回ったら通常へ戻す。
	// 入るときと同じ値にすると、境目で細かく切り替わってばたつく
	constexpr float BackDeg      = 30.0f;
	constexpr float BackYawRate  = 1.2f;
	constexpr float BackDist     = 8.0f;

	//===== 軌跡の記録 =====
	// 何秒ぶん覚えておくか。追う遅れより十分に長く持つ
	constexpr float TrailSec = 4.0f;
	// 覚える間隔(秒)。細かすぎると数が増えるだけで、間は補間で足りる
	constexpr float TrailStep = 0.033f;
}
