#pragma once

#include "HjUI.h"

//==========================================================
// PlayModeUI
//   PLAY 選択後の「プレイモード選択」画面(暫定プレースホルダー)。
//   ※レイアウトは Claude Design 決定後に差し替え予定。今は3モードのカードのみ。
//==========================================================
class PlayModeUI : public KdGameObject
{
public:
	void Update()     override;
	void DrawSprite() override;

	// マルチのカードの位置。呼ぶ側が数字を直接書かなくて済むようにする
	static constexpr int MultiplayerIndex = 3;

	int  GetSelected() const { return m_sel; }
	bool ConsumeActivated() { bool a = m_activated; m_activated = false; return a; }
	// 選択中カードの矩形(デザイン座標)。モーフ遷移の起点に使う。
	void GetSelectedCardRect(float& dx, float& dy, float& w, float& h) const;

private:
	int  m_sel = 0;
	bool m_activated = false;
};
