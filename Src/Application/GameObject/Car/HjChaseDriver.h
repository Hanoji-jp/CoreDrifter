#pragma once

#include "HjCarTrail.h"

class CarBase;

//==========================================================
// HjChaseDriver
//   前を走る車の跡を追いかけて、運転操作を作る。自作なので Hj 接頭辞。
//
//   ■ 車ではない
//   これは「運転する人」にあたる。車そのものは CarBase が持つ。
//   分けておくと、同じ運転手を先行にも後追いにも使える。
//     後追い … 手本＝プレイヤーの生きた跡
//     先行   … 手本＝録画した跡
//   追いかける処理は同じで、跡の出どころが違うだけ。
//
//   ■ 出す値の形
//     出力 ＝ 土台 ＋ 補正
//
//   土台 … 手本が既に出している答え(相手の踏み量・いまの滑り角)
//   補正 … 自分と目標のズレ
//
//   ドリフト中の釣り合いをゼロから計算するのは難しいが、
//   前を走っている人が既に正解を出しているので、それを借りる。
//
//   ■ 土台は「近い」ときだけ正しい
//   押し出されて状態が離れると、手本の値は毒になる。
//   横向きで止まりかけているのに相手の全開を真似ることになる。
//   なのでズレが大きいほど土台の重みを下げ、
//   支えきれない角度まで回ったら別の運転へ切り替える。
//==========================================================
class HjChaseDriver
{
public:
	// 調整のために外から見る値。
	// 目標と実際を並べて出さないと、振れているのが
	// P が強いのか D が足りないのか分からない
	struct Debug
	{
		float targetDeg  = 0.0f;   // 目標の滑り角
		float actualDeg  = 0.0f;   // いまの滑り角
		float gap        = 0.0f;   // 目標点までの距離
		float trust      = 1.0f;   // 手本をどれだけ信じているか(0〜1)
		bool  recovering = false;  // 復帰中か
		bool  kicking    = false;  // サイドを引いている最中か
	};

	// この運転手が出す操作。
	// 引数で1つずつ返すと、変速を足すたびに呼ぶ側まで直すことになる
	struct Output
	{
		float throttle  = 0.0f;
		float steer     = 0.0f;
		bool  handbrake = false;
		bool  shiftUp   = false;
		bool  shiftDown = false;
	};

	// 手本の跡を見て、この車が出すべき操作を作る。
	// 物理には触らない。返した値を CarBase が使う。
	//
	// ※時刻は渡さない。跡の一番新しい点を基準にする。
	//   別の時計で数えると、走り出しを待っている間にずれて、
	//   ずっと昔の位置を目標にすることになる
	void Update(float dt, const CarBase& self, const HjCarTrail& trail,
	            Output& out);

	const Debug& GetDebug() const { return m_dbg; }

	// 走り直すときに畳む
	void Reset();

private:
	// 手本をどれだけ信じるか。ズレが大きいほど下げる
	static float Trust(float angleErrDeg, float distErr);

	// 前フレームの値。急に変えると人の操作に見えないので、
	// 動ける速さを制限するのに使う
	float m_throttle = 0.0f;
	float m_steer    = 0.0f;

	// 角度のズレの変化を見るために、前フレームのズレを覚えておく。
	// ズレだけを見て直すと、直した頃には行き過ぎている
	float m_prevAngleErr = 0.0f;
	bool  m_hasPrev      = false;

	// 滑り出しのきっかけ
	float m_kickTimer = 0.0f;   // 引いている残り時間
	float m_kickWait  = 0.0f;   // 次に引けるまでの残り時間

	// 復帰中か。入るときと出るときで閾値を変える
	// (同じにすると境目で細かく切り替わってばたつく)
	bool m_recovering = false;

	// 変速してから次までの待ち。
	// 待たないと、上げた直後に「回転が落ちた」と判断して下げ、
	// また上げる、を繰り返す
	float m_shiftWait = 0.0f;

	Debug m_dbg;
};
