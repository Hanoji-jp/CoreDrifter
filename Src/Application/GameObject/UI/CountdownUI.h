#pragma once

#include "HjUI.h"
#include "CountdownConst.h"

//==========================================================
// CountdownUI
//   走り出しの合図。3→2→1→GO。
//
//   数えている間は車を動かさない。
//   いきなり操作できるより、始まりが揃っている方が
//   「今から走る」という区切りになる。
//==========================================================
class CountdownUI : public KdGameObject
{
public:
	void Update()     override;
	void DrawSprite() override;

	// 数えている間はゲーム側を止めるので、自分は動き続ける
	bool UpdatesWhileFrozen() const override { return true; }

	// 数え終わったか。シーンがこれを見て操作を許す
	bool IsDone() const { return m_done; }

private:
	float m_timer = 0.0f;
	bool  m_done  = false;
};
