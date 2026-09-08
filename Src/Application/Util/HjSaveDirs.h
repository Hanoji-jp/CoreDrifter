#pragma once

#include <filesystem>

//==========================================================
// HjSaveDirs
//   遊ぶ側が書き込む先のフォルダを用意する。自作なので Hj 接頭辞。
//
//   ■ なぜ要るか
//   保存物の置き場が Asset/Data/ の下にある。
//   開発中は Asset/ がフォルダとしてあるので気づかないが、
//   配布ビルドは Asset/ を exe へ埋め込むので、実体が無い。
//
//   ofstream はフォルダが無いと黙って失敗する。
//   設定も成績も車の合わせ込みも、書けないまま気づかれない。
//
//   ■ 読むほうは要らない
//   読みは埋め込み(AssetVault)から取れる。
//   ここで作るのは「書く先」だけ。
//==========================================================
namespace HjSaveDirs
{
	inline void Ensure()
	{
		// 無ければ作る。あっても何も起きない。
		// 例外は投げない形で呼ぶ(起動を止める理由にならない)
		std::error_code ec;

		// 遊ぶ側が MOD を置く所。
		// 入れ物が無いと、どこへ置けばいいのか分からない
		std::filesystem::create_directories("Mods/Body",  ec);
		std::filesystem::create_directories("Mods/Wheel", ec);

#ifndef DISTRIBUTE_BUILD
		// 道と地形の編集で書き出す所。
		//
		// 配布ビルドでは編集を切っているので作らない。
		// 作ると、遊ぶ側の手元に空の Asset/Data/terrain が生えて、
		// 何のフォルダなのか分からないまま残る
		std::filesystem::create_directories("Asset/Data/terrain", ec);
#endif
	}
}
