#pragma once

// 借りているものの出典。
//
// ■ なぜ必要か
// 使っているモデルと標高データには、表示を条件にした許諾が付いている。
// CC-BY は「共有する場所には必ず作者を書く」ことが条件で、
// 書かないまま配ると、そもそも使う権利が無い。
//
// ■ 画面に出すこと
// license.txt を同梱するだけでは足りない。
// 遊ぶ側が見られる所に出す必要がある。
// 設定画面の OTHER にまとめてある。
//
// ■ 増やすとき
// アセットを1つ借りたら、必ずここへ1行足す。
// 「あとでまとめて」にすると、出所の分からないものが残る。
namespace CreditsConst
{
	// 1件ぶん。題・作者・許諾・入手元
	struct Entry
	{
		const char* title;
		const char* author;
		const char* license;
		const char* source;
	};

	constexpr Entry Items[] =
	{
		{
			"'90 Honda NSX Flat-Shaded",
			"S.K. (sketchfab.com/Streetsraker)",
			"CC-BY-4.0",
			"sketchfab.com/3d-models/90-honda-nsx-flat-shaded",
		},
		{
			"Shapespark low poly exterior plants kit",
			"Shapespark (sketchfab.com/shapespark)",
			"CC-BY-4.0",
			"sketchfab.com/3d-models/shapespark-low-poly-exterior-plants-kit-de9e79fc07b748d1a6ac055b49ee5c67",
		},
		{
			"Trees and bush Pack LOWPOLY",
			"EFX (sketchfab.com/evan4129)",
			"CC-BY-4.0",
			"sketchfab.com/3d-models/trees-and-bush-pack-lowpoly-f2a25ee70df440c9ab03d57aba2dc3f2",
		},
	};

	constexpr int Count = static_cast<int>(sizeof(Items) / sizeof(Items[0]));

	// 見出し
	constexpr const char* Heading = "CREDITS";

	// CC-BY は「これを貼れ」と文面まで指定してくる。
	// 縮めると条件を満たさないので、そのまま出す
	constexpr const char* Note =
		"This work is based on \"'90 Honda NSX Flat-Shaded\" by S.K., "
		"licensed under CC-BY-4.0.";
}
