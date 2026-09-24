#pragma once

// 選べる車種。
//
// ■ なぜ番号で持つか
// 保存にも通信にも同じものを使う。
// 名前の文字列を送ると長さが変わって扱いづらく、
// 表示の文言を直した瞬間に、保存済みの選択が読めなくなる。
//
// ■ 並びを変えないこと
// 番号がそのまま保存され、通信でも流れる。
// 途中に足すと、前に選んでいた車が別の車になる。
// 追加は必ず末尾へ。
namespace CarChoiceConst
{
	enum class Kind
	{
		Silvia = 0,
		Nsx    = 1,
		Count
	};

	// 画面に出す名前。並びは Kind と揃える
	constexpr const char* Names[] =
	{
		"SILVIA S15",
		"HONDA NSX",
	};

	// 作り手の名前。中央の見出しの小さい方に出す
	constexpr const char* Makers[] =
	{
		"NISSAN",
		"HONDA",
	};

	// 型番。中央の見出しの大きい方(96px)に出す。
	//
	// ■ モデルのファイル名を使わないこと
	// 見た目の差し替え(MOD)を当てるとファイル名が変わるので、
	// 車を替えていないのに型番が変わってしまう。
	// しかも自動出力の名前は "silvia_body" のように長く、
	// 大見出しに収まらず台の下へ潜り込む
	constexpr const char* Models[] =
	{
		"S15",
		"NSX",
	};

	// 選んだ車を覚えておくファイル。
	// 1行しかないので、既存の「名前 値」の作法に合わせる
	constexpr const char* SavePath = "Asset/Data/CarChoice.txt";
	constexpr const char* SaveKey  = "car";

	// 番号が範囲から外れていたら既定へ倒す。
	// 手で書き換えられることも、古い保存を読むこともある
	constexpr Kind Fallback = Kind::Silvia;
}
