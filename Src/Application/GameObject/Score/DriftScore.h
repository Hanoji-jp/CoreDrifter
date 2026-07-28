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

private:
	// コンボ倍率に応じた色(白→金→虹)
	Math::Color ComboColor(int combo) const;

	std::weak_ptr<CarBase> m_wpCar;

	double m_total     = 0.0;    // 確定済み総スコア
	double m_chain     = 0.0;    // 現チェーンの加算(倍率適用前)
	int    m_combo     = 1;      // 倍率
	float  m_chainTime = 0.0f;   // ドリフト継続時間(秒)
	float  m_graceTimer = 0.0f;  // 途切れ猶予の残り(秒)
	bool   m_drifting  = false;  // チェーン継続中か
	float  m_punch     = 0.0f;   // ライブ数字の追加スケール(0..PunchMax)

	// 確定時のフラッシュ(GREAT/PERFECT)
	float       m_judgeTimer = 0.0f;
	const char* m_judgeText  = "";

	float m_driftIntensity = 0.0f;   // ドリフト強度(0..1、ビネット用に平滑化)
};
