#pragma once

// 選べるステージ。
//
// ■ なぜ番号で持つか
// 保存にも通信にも同じものを使う。
// 名前の文字列を送ると長さが変わって扱いづらく、
// 表示の文言を直した瞬間に、保存済みの選択が読めなくなる。
//
// ■ 並びを変えないこと
// 番号がそのまま保存される。
// 途中に足すと、前に選んでいたステージが別のものになる。
// 追加は必ず末尾へ。
//
// ■ 今は2つ
// 借りてきた峠のモデルと、自作の地形＋道。
// 差し替えの途中なので両方残してある。
namespace StageChoiceConst
{
	enum class Kind
	{
		Nexus = 0,   // 自作の地形＋スプラインの道(HjTerrain + HjRoad)
		Count
	};

	// 画面に出す名前。並びは Kind と揃える
	constexpr const char* Names[] =
	{
		"NEXUS TOUGE",
	};

	// 一言の説明。何が違うのか、選ぶ前に分かるようにする
	constexpr const char* Notes[] =
	{
		"generated terrain",
	};

	// 選んだステージを覚えておくファイル。
	// いまは save.dat の "stage" 項目へ入る
	constexpr const char* SavePath = "Asset/Data/StageChoice.txt";
	constexpr const char* SaveKey  = "stage";

	// 番号が範囲から外れていたら既定へ倒す。
	// 手で書き換えられることも、古い保存を読むこともある
	constexpr Kind Fallback = Kind::Nexus;
}
