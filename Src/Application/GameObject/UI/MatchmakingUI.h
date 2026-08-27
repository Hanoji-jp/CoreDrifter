#pragma once

#include "MultiUIBase.h"

//==========================================================
// MatchmakingUI
//   相手を探している画面。
//
//   ■ 今は実際には探していない
//   マッチングサーバーが無いので、繋がる相手は見つからない。
//   経過時間と点滅だけは本物の時間で動かして、
//   「動いている画面」としては成立させる。
//   見つかった人数は通信側から入れる。
//==========================================================
class MatchmakingUI : public MultiUIBase
{
public:
	void Update()     override;
	void DrawSprite() override;

	// 見つかった人数(通信側が入れる)
	void SetFound(int found) { m_found = found; }

	bool ConsumeCancel() { const bool a = m_cancel; m_cancel = false; return a; }

private:
	void DrawPips();
	void DrawReadout();
	void DrawCells();

	int   m_found = 0;
	float m_elapsed = 0.0f;   // 探し始めてからの秒数
	float m_blink   = 0.0f;   // 次の枠の点滅
	bool  m_cancel  = false;
};
