#pragma once

#include "HjUI.h"
#include "PauseConst.h"

//==========================================================
// PauseUI
//   走行を止めている間のメニュー。
//
//   止めた映像を暗く落として、メニューが読める明るさを作る。
//   落とさないと背景の情報量に負けて、どこが選ばれているか分からない。
//==========================================================
class PauseUI : public KdGameObject
{
public:
	// 選べる操作。実際に行き先があるものだけを並べる
	enum class Action
	{
		None,
		Resume,     // 走行へ戻る
		Restart,    // スポーン地点からやり直す
		EndRun,     // 走行を終えて成績へ
	};

	void Update()     override;
	// 判定演出(文字流体化)より手前へ描く。
	// 普通の DrawSprite だと、演出が後から合成されてポーズ画面の上に出る。
	void DrawSpriteOverlay() override;

	// 止まっている間も動かないと、メニューが操作できない
	bool UpdatesWhileFrozen() const override { return true; }

	// 開いているか。シーンが毎フレーム入れる。
	// 閉じている間は描かないだけでなく、入力も見ない。
	// 見てしまうと、走行中のキーをポーズ側が拾ってしまう。
	void SetVisible(bool visible);

	// 表示する成績(シーンが毎フレーム入れる)
	void SetScore(double total, int combo) { m_total = total; m_combo = combo; }

	// 選ばれた操作(1回だけ返す)
	Action ConsumeAction() { const Action a = m_action; m_action = Action::None; return a; }

private:
	void DrawMenu();
	void DrawScorePanel();

	//===== 設定ウィンドウ =====
	// 走行中に音量だけ直したい、という用がほとんどなので、
	// 画面ごと切り替えず小さな窓で出す。
	// 画面を移ると走行の映像が消えて、どこで止めたのか分からなくなる。
	void UpdateSettingsWindow();
	void DrawSettingsWindow();
	// 音量の行を1段階動かす
	void StepVolume(int row, int dir);
	// 行の値を取り出す
	float VolumeOf(int row) const;
	// 行へ値を入れる(バスへの反映も行う)
	void SetVolumeOf(int row, float v);

	bool m_settingsOpen = false;
	int  m_settingsRow  = 0;


	bool   m_visible = false;
	int    m_sel   = 0;
	double m_total = 0.0;
	int    m_combo = 1;
	Action m_action = Action::None;
};
