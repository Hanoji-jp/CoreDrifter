#pragma once

#include "../Const/ModConst.h"

//==========================================================
// HjModProfile
//   差し替えモデルごとの合わせ込み。自作なので Hj 接頭辞。
//
//   ■ なぜモデルごとに分けるか
//   外から持ってきたモデルは、原点も向きも大きさもバラバラで、
//   1つ合わせても次のモデルには通用しない。
//
//   合わせた値を車の調整(CarTune)へ書くと、
//   ・モデルを替えるたびに前の値が消える
//   ・標準の車に戻したとき、標準の値まで壊れている
//   という状態になる。実際そうなっていた。
//
//   モデルの道を鍵にして1つずつ覚えておけば、
//   差し替えて戻すだけで、それぞれの合わせ込みが復活する。
//
//   ■ なぜ即時に書かないか
//   合わせている最中は行き過ぎたり戻したりする。
//   その途中の値を毎回ファイルへ書くと、
//   「やっぱり前のほうが良かった」が効かない。
//   触った値は手元に持っておき、書き出すのは指示されたときだけにする。
//
//   ■ なぜ項目を名前で持つか
//   決まった形(構造体)で持つと、調整の項目を1つ足すたびに
//   ここと、書き出しと、受け渡しの3か所を揃えて直すことになる。
//   1か所でも忘れると、ずれた値が黙って入る。
//
//   名前と値の対で持てば、足す場所は車の一覧だけで済む。
//==========================================================
class HjModProfile
{
public:
	// 1モデル分の合わせ込み。名前 → 値
	using Entry = std::unordered_map<std::string, float>;

	static HjModProfile& Instance()
	{
		static HjModProfile inst;
		return inst;
	}

	// ファイルから全部読む。起動時に1回
	void Load();
	// ファイルへ全部書く。指示されたときだけ
	void Save() const;

	// この道の合わせ込みがあるか
	bool Has(const std::string& path) const;

	// 取り出す。無ければ空を返す。
	// 「無い」を呼ぶ側で分岐させると、その判定が各所に散る
	Entry Get(const std::string& path) const;

	// 覚える。ファイルへは書かない(Save で書く)
	void Set(const std::string& path, const Entry& e);

	// 忘れる。合わせ込みを捨てて、置いたままの状態へ戻すとき
	void Erase(const std::string& path);

	// 書き出していない変更があるか。
	// 画面に印を出して、閉じる前に気づけるようにする
	bool IsDirty() const { return m_dirty; }

private:
	HjModProfile() = default;

	std::unordered_map<std::string, Entry> m_map;

	// Save を呼ぶまで true のまま。
	// const な Save から下ろすので mutable
	mutable bool m_dirty = false;

	HjModProfile(const HjModProfile&) = delete;
	void operator=(const HjModProfile&) = delete;
};
