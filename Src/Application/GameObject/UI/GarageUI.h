#pragma once

#include "HjUI.h"
#include "../../Const/GarageConst.h"
#include "../../Const/CarChoiceConst.h"

class HjCarPortrait;

//==========================================================
// GarageUI
//   Drift Project の GARAGE / CAR SELECT。
//
//   ■ 何をする画面か
//   どの車に乗るかを決める。それだけ。
//
//   見た目の差し替え(MOD)と合わせ込みは走行中のTABメニューが持つ。
//   混ぜると、選ぶ話と弄る話が同じ階層に並んで意味の段が狂う。
//
//   ■ 性能は実際の値から出す
//   表に手で書くと、車を触るたびに画面の数字と食い違う。
//   車の設定(Silvia::Setup / Nsx::Setup)を当てて読む。
//==========================================================
class GarageUI : public KdGameObject
{
public:
	void Init()       override;
	void Update()     override;
	void DrawSprite() override;

	// 決定されたか。場面が拾って、車を切り替えてから戻る
	bool ConsumeDecided()
	{
		const bool v = m_decided;
		m_decided = false;
		return v;
	}

	// 戻るが押されたか
	bool ConsumeBack()
	{
		const bool v = m_back;
		m_back = false;
		return v;
	}

	CarChoiceConst::Kind Selected() const;

	// 車の絵。場面が持っている見せ札(HjCarPortrait)から預かる。
	// UIが3Dを描く側まで抱えるとゴッドクラスになるので、
	// 描き上がった絵だけを受け取る
	void SetPortrait(const std::weak_ptr<HjCarPortrait>& p) { m_wpPortrait = p; }

private:
	//===== 画面に出す1台ぶん =====
	// 車の設定から作る。手で書いた表は持たない
	struct Stat { const char* label; float value; };   // value 0〜1

	struct Entry
	{
		CarChoiceConst::Kind kind = CarChoiceConst::Kind::Silvia;
		std::string name;      // 画面に出す名前
		std::string model;     // 型式(大きく出す)
		std::string tier;      // 等級
		Math::Color swatch;    // サムネイルの色。車の煙の色を使う
		std::vector<Stat> stats;
	};

	// 車の設定を読んで一覧を作る。起動時に1回
	void BuildEntries();

	//===== 描画 =====
	void DrawHeader() const;
	void DrawList() const;
	void DrawStage(const Entry& e) const;
	void DrawStats(const Entry& e) const;
	void DrawStrip() const;

	std::vector<Entry> m_entries;

	int  m_sel     = 0;
	bool m_decided = false;
	bool m_back    = false;

	std::weak_ptr<HjCarPortrait> m_wpPortrait;

	// 背景のぐにゃぐにゃの回り
	float m_spin = 0.0f;
};
