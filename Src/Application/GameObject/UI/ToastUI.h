#pragma once

#include "HjUI.h"
#include "ToastConst.h"

//==========================================================
// ToastUI
//   走行中の通知を左上へ積んで出す。
//
//   出来事そのものは採点側が起こし、HjToastQueue へ積まれる。
//   ここは並べて動かして消すだけ。
//==========================================================
class ToastUI : public KdGameObject
{
public:
	void Update()     override;
	void DrawSprite() override;

	// ポーズ中も動く。通知は演出であって、止める必要がない
	bool UpdatesWhileFrozen() const override { return true; }

};
