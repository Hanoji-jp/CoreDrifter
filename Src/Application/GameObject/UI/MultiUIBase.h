#pragma once

#include "HjUI.h"
#include "MultiConst.h"

//==========================================================
// MultiUIBase
//   LOBBY / MATCHMAKING / RESULTS で共通の骨格を描く。
//
//   3画面とも「見出し → 中身 → 下段の操作」で組む。
//   画面ごとに骨格が違うと、遷移するたびに目が置き場所を探し直すことになる。
//   ここに共通部分を置いて、各画面は中身だけを書く。
//==========================================================
class MultiUIBase : public KdGameObject
{
protected:
	// 背景・装飾・見出しまで。中身は各画面が続けて描く
	void DrawFrame(const char* kicker, const char* title);
	// 右上の「ラベル＋値」。画面の状態を1つだけ添える
	void DrawStatRight(const char* label, const char* value);
	// 薄いガイド線と切れ目の印。余白を締めるためだけのもの
	void DrawGuides(float x, float breakTop, float breakBottom);

	// ※キー入力は HjKeyInput に集約した。
	//   各クラスが前フレーム状態を持つと、画面を跨いだときに
	//   押しっぱなしが新規入力として拾われる。
};
