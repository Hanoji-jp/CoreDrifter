#pragma once

#include "../../Const/CarChoiceConst.h"

class CarBase;

//==========================================================
// HjCarChoice
//   どの車に乗るか。自作なので Hj 接頭辞。
//
//   ■ なぜ独立して持つか
//   選択は場面をまたいで残る。走行画面で替えたものが、
//   次に走り出したときも効いていないと意味がない。
//   場面が持つと、場面を作り直すたびに消える。
//
//   ■ 車そのものは持たない
//   持ち主は場面の側。ここは「どれを作るか」を答えるだけ。
//==========================================================
class HjCarChoice
{
public:
	using Kind = CarChoiceConst::Kind;

	static HjCarChoice& Instance()
	{
		static HjCarChoice inst;
		return inst;
	}

	void Load();
	void Save() const;

	Kind Get() const { return m_kind; }
	void Set(Kind k);

	// 画面に出す名前
	const char* Name() const;
	static const char* NameOf(Kind k);

	// 選ばれている車を作る。
	// 場面ごとに種類で分岐を書くと、車を足すたびに全部直すことになる
	static std::shared_ptr<CarBase> Create(Kind k);

	// 既にある車へ、車種ごとの設定を当てる。
	//
	// 相手の車は「作ってから車種が分かる」ので、
	// 作り方(Create)とは別に、当てるほうも要る
	static void ApplySpec(CarBase& car, Kind k);
	std::shared_ptr<CarBase> Create() const { return Create(m_kind); }

private:
    HjCarChoice() = default;

	Kind m_kind = CarChoiceConst::Fallback;

	HjCarChoice(const HjCarChoice&) = delete;
	void operator=(const HjCarChoice&) = delete;
};
