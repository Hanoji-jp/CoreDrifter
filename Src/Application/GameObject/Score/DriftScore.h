#pragma once

#include "../UI/HjUI.h"
#include "../../Const/DriftScoreConst.h"

class CarBase;   // 前方宣言(実体はcppでinclude)

//==========================================================
// DriftScore
//   ドリフト採点(角度×速度×時間＋コンボ＋GREAT/PERFECT判定)と、
//   そのスコアを数字パンチで見せる演出。マンダラート「スコア・採点」＋
//   「演出：スコア表示」に対応。車(CarBase)の速度・向きを参照して判定する。
//==========================================================
class DriftScore : public KdGameObject
{
public:
	void Update()     override;
	void DrawSprite() override;

	// 採点対象の車をセット
	void SetCar(const std::weak_ptr<CarBase>& car) { m_wpCar = car; }

	// 総合スコア(他システムから参照用)
	double GetTotal() const { return m_total; }

	// 走行中に伸びている点(確定前)。倍率を掛けた見えている値
	double GetLiveScore() const { return m_chain * m_combo; }
	int    GetCombo()     const { return m_combo; }
	bool   IsDrifting()   const { return m_drifting; }
	// チェーンが途切れるまでの残り(0〜1)。猶予の減り具合をそのまま出す。
	//
	// ※m_drifting は「チェーンが継続中か」で、途切れてから確定するまで
	//   true のまま。これで判定すると猶予の間も満タンを返してしまい、
	//   ゲージが動かない。今この瞬間に成立しているかは別に持つ。
	float  GetChainHold() const
	{
		if (m_active) { return 1.0f; }
		return std::clamp(m_graceTimer / DriftScoreConst::GraceTime, 0.0f, 1.0f);
	}

private:
	// 判定の段階。色と大きさを段で変えて、上位ほど派手にする
	// NICEは出さない(出す価値のある2段だけ)
	enum class Judge { None, Great, Perfect };

	// 判定の段階ごとの色
	Math::Color JudgeColor(Judge j) const;

	// 描画は役割ごとに分ける(1つの関数に演出を詰め込まない)。
	// ※スコアの数字はHUD(RunHudUI)が出すので、ここでは出さない。
	//   同じ値を2か所に出すと、片方だけ直したときに食い違う。
	void DrawJudge();         // GREAT/PERFECT の叩きつけ

	std::weak_ptr<CarBase> m_wpCar;

	double m_total     = 0.0;    // 確定済み総スコア
	double m_chain     = 0.0;    // 現チェーンの加算(倍率適用前)
	int    m_combo     = 1;      // 倍率
	float  m_chainTime = 0.0f;   // ドリフト継続時間(秒)
	float  m_graceTimer = 0.0f;  // 途切れ猶予の残り(秒)
	bool   m_drifting  = false;  // チェーン継続中か(猶予の間もtrue)
	bool   m_active    = false;  // 今この瞬間ドリフトが成立しているか

	// 確定時の演出(GREAT/PERFECT)
	float       m_judgeTimer = 0.0f;
	const char* m_judgeText  = "";
	Judge       m_judge      = Judge::None;
	// 出した瞬間だけ真。文字エフェクトへの焼き直しを1回に抑えるためのもので、
	// 毎フレーム渡すとRTを作り直すことになる
	bool        m_judgeFresh = false;

	// 着弾の瞬間だけ画面を白く飛ばす
	float m_flash = 0.0f;

	float m_driftIntensity = 0.0f;   // ドリフト強度(0..1、ビネット用に平滑化)
};
