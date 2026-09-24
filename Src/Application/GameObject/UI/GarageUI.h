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
		std::string maker;   // 作り手(小さい方)
		std::string model;   // 型番(大きい方)
		std::string tier;      // 等級
		Math::Color swatch;    // サムネイルの色。車の煙の色を使う
		std::vector<Stat> stats;

		// 諸元表の1行。値は組み立てて持つので文字列
		struct SpecRow { const char* label; std::string value; };
		std::vector<SpecRow> spec;
	};

	// 車の設定を読んで一覧を作る。起動時に1回
	void BuildEntries();

	// 切り替えの動きを進める。
	//
	// 棒と表が瞬時に入れ替わると、同じ画面のまま中身が変わったのではなく、
	// 別の画面へ飛んだように見える。追いかける形にして繋ぐ
	void UpdateAnim(float dt);

	//===== 描画 =====
	void DrawHeader() const;
	void DrawList() const;
	void DrawStage(const Entry& e) const;
	void DrawStats(const Entry& e) const;
	// 諸元表。左の列の下に置く
	void DrawSpec(const Entry& e) const;
	// 下の帯の札の位置。当たり判定と描画で同じ式を使う。
	// 別々に書くと、押せる所と見える所がずれる
	void StripLayout(int n, float& outX, float& outW) const;

	void DrawStrip() const;

	std::vector<Entry> m_entries;

	int  m_sel     = 0;
	bool m_decided = false;
	bool m_back    = false;

	std::weak_ptr<HjCarPortrait> m_wpPortrait;

	// 背景のぐにゃぐにゃの回り

	// 絵を引きずって車を回している最中か。
	// 押した瞬間だけでは足りないので、状態として持つ
	//===== 切り替えの動き =====
	// 進み(0〜1)。1で落ち着いた状態。
	//
	// 0 から始める。開いた1回目も同じ動きで入ってくる。
	// 1 にしておくと、初回だけ全部そこに在る状態で始まって浮く
	float m_animT = 0.0f;

	// 前のフレームの選択。変わったら追いかけ直す
	int m_selPrev = 0;

	// 今出ている棒の長さ。描くのはこちらで、車の値ではない
	std::vector<float> m_barNow;

	// 切り替えが始まった時点の長さ。ここから今の車の値へ寄せる。
	// 0 から伸ばし直すと、毎回一度空になってちらつく
	std::vector<float> m_barFrom;

	// 選択の地の位置。行へ遅れて追いつく
	float m_selY    = 0.0f;
	bool  m_selInit = false;

	bool  m_dragging  = false;
	float m_lastMouse = 0.0f;
};
