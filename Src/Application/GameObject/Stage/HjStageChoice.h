#pragma once

#include "../../Const/StageChoiceConst.h"

//==========================================================
// HjStageChoice
//   どのステージを走るか。自作なので Hj 接頭辞。
//
//   ■ なぜ独立して持つか
//   選択は場面をまたいで残る。走行画面で替えたものが、
//   次に走り出したときも効いていないと意味がない。
//   場面が持つと、場面を作り直すたびに消える。
//
//   ■ ステージそのものは持たない
//   持ち主は場面の側。ここは「どれを作るか」を答えるだけ。
//   車種の選択(HjCarChoice)と同じ作法にしてある。
//
//   ■ なぜ定数をやめたか
//   もともと TerrainConst::UseTerrain という定数で分けていた。
//   組み直さないと切り替えられないので、見比べができなかった。
//==========================================================
class HjStageChoice
{
public:
	using Kind = StageChoiceConst::Kind;

	static HjStageChoice& Instance()
	{
		static HjStageChoice inst;
		return inst;
	}

	void Load();
	void Save() const;

	Kind Get() const { return m_kind; }
	void Set(Kind k);

	// 画面に出す名前
	const char* Name() const;
	static const char* NameOf(Kind k);

	// 一言の説明。何が違うのか、選ぶ前に分かるようにする
	static const char* NoteOf(Kind k);

	// 自作の地形を使うステージか。
	//
	// 場面はこれを見て、地形と道を作るか、借り物のモデルを置くかを決める
	bool UsesTerrain() const { return m_kind == Kind::Nexus; }

private:
	Kind m_kind = StageChoiceConst::Fallback;
};
