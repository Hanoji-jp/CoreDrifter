#pragma once

#include "HjUI.h"
#include "RunHudConst.h"

class CarBase;
class DriftScore;

//==========================================================
// RunHudUI
//   走行中のHUD。速度・ギア・回転数・ドリフト角・スコアを出す。
//
//   ■ 車から切り出してある
//   以前はCarBase::DrawSpriteが直接バーを描いていたが、
//   車の役目は走ることで、画面の作りとは関係がない。
//   表示だけを持つオブジェクトにして、車とスコアからは値を読むだけにする。
//
//   ■ 右上には何も置かない
//   採点の内訳(角度・ライン・スタイル)は走り終えた後に読むもので、
//   走行中に出ても操作へ反映できない。動かない表示は目が無視するようになり、
//   画面の一等地が死ぬ。四隅のかぎ括弧が角を締めているので、
//   何も置かなくても画面は成立する。
//
//   ■ 映像の上に置く前提
//   文字は紙色、罫線とかぎ括弧で位置を示し、囲みは作らない。
//   塗るのは「今それが起きている」ものだけ。
//==========================================================
class RunHudUI : public KdGameObject
{
public:
	void DrawSprite() override;

	void SetCar(const std::weak_ptr<CarBase>& car)      { m_wpCar = car; }
	void SetScore(const std::weak_ptr<DriftScore>& sc)  { m_wpScore = sc; }

private:
	void DrawFrame();                           // 四隅のかぎ括弧と中心の印
	void DrawRunInfo();                         // 左上(何本目・コース・区間)
	void DrawCourseLine();                      // 左下(走行ラインと通過点)
	void DrawDriftAngle(float angleDeg);        // 上中央
	void DrawScore(double live, int combo, float hold);   // 上中央
	void DrawTach(float rpmRatio);              // 下右(回転計)
	void DrawSpeed(float kmh, int gear);        // 下右(速度・ギア)

	std::weak_ptr<CarBase>    m_wpCar;
	std::weak_ptr<DriftScore> m_wpScore;
};
