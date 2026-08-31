#pragma once

#include "UpdateNoticeConst.h"

//==========================================================
// HjUpdateNotice
//   タイトル画面の左下に出す、更新の知らせ。自作なので Hj 接頭辞。
//
//   ■ なぜ別の部品にするか
//   TitleMenuUI は既に大きい。
//   ここは「更新があるときだけ出る」もので、
//   タイトルの他の要素とは出る条件も寿命も違う。
//   混ぜると、更新まわりを直すのにタイトル全体を読むことになる。
//
//   ■ 何をするか
//   起動時の確認(HjUpdater)の結果を見て、
//   新しい版があるときだけ、文字と帯を出す。
//   帯は明るさを行き来させて、そこに何かあることを伝える。
//==========================================================
class HjUpdateNotice : public KdGameObject
{
public:
	void Update()     override;
	void DrawSprite() override;

private:
	// 出るまでの間と、出るときの濃さ。
	// 起動してすぐ出すと、画面が組み上がる前に現れてちらつく
	float m_age    = 0.0f;
	float m_appear = 0.0f;
};
