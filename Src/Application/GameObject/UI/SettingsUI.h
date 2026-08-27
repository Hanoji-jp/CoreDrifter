#pragma once

#include "HjUI.h"

//==========================================================
// SettingsUI
//   Drift Project UI の SCREEN2「SETTINGS」。左タブ＋右設定パネル。
//   ↑↓で行選択、←→でトグル/ステップ。HjUIの再利用ウィジェットで描く。
//==========================================================
class SettingsUI : public KdGameObject
{
public:
	void Init()       override;
	void Update()     override;
	void DrawSprite() override;

private:
	int  m_tab = 0;   // 選択中タブ
	int  m_row = 0;   // 選択中の設定行

	// 設定値(トグル/選択肢インデックス)
	bool m_toggles[5] = { true, true, true, true, true };  // TRACTION/STABILITY/ABS/STEERING/REWIND
	int  m_diff = 1, m_trans = 0, m_damage = 1, m_units = 0;
};
