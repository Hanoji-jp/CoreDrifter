#pragma once

// 道を上から見て引くための定数。
//
// ■ なぜ地図で引くか
// 3Dで点を掴む形は、狙いが合わずに使い物にならなかった。
// 視線を軸へ落とす計算、画面の中でのマウス位置、
// カメラからの距離——どれか1つずれると掴めない。
//
// 上から見た2次元の絵の上でなら、マウスの位置がそのまま
// 地図の座標になる。ずれようがない。
//
// 道は「地図の上で線を引く」ものなので、操作としても素直。
//
// ■ 高さは触らない
// 地図には高さが無い。道の高さは地形から拾って
// 勾配の上限に収めるので、平面の位置だけ決めれば足りる。
namespace RoadMapConst
{
	//===== 地図の大きさ =====
	// ImGui に置く絵の大きさ(ピクセル)。
	// 小さいと細かく引けず、大きいとパネルからはみ出す
	constexpr float ViewSize = 520.0f;

	//===== 陰影 =====
	// 地形を灰色の濃淡で描く。
	//
	// 標高をそのまま明るさにすると、尾根も谷も似た灰色になって
	// 道を引ける所が分からない。
	// 斜めから光を当てた陰影を混ぜると、地形の形が読める。
	constexpr float LightX = -0.5f;
	constexpr float LightY =  0.7f;
	constexpr float LightZ =  0.5f;

	// 陰影の効き。0で標高だけ、1で陰影だけ
	constexpr float ShadeMix = 0.75f;

	// 地図に使う画素の数(1辺)。
	// 地形の格子をそのまま使うと重いので、間引いて作る
	constexpr int TexSize = 256;

	//===== 点 =====
	// 点の大きさ(ピクセル)
	constexpr float PointRadius = 5.0f;
	constexpr float PointRadiusHot = 8.0f;

	// 点を掴める太さ(ピクセル)。
	// 見た目より広く取らないと、狙っているのに外れる
	constexpr float GrabRadius = 12.0f;

	// 線の太さ(ピクセル)
	constexpr float LineWidth = 2.0f;

	// 道幅の目安を出すときの太さ(ピクセル)。
	// 実際の幅を地図の縮尺で描くと細すぎて見えないので、
	// 別に太い線を薄く重ねる
	constexpr float RoadBandWidth = 7.0f;

	//===== 色 =====
	// 点と線。地形の灰色の上で目立つ色にする
	constexpr unsigned int ColLine    = 0xFFE0C060;   // 水色寄りの線(ABGR)
	constexpr unsigned int ColBand    = 0x40E0C060;   // 道幅の帯(薄く)
	constexpr unsigned int ColPoint   = 0xFFFF9040;
	constexpr unsigned int ColPointHot= 0xFF40A0FF;
	constexpr unsigned int ColCar     = 0xFF40FF40;   // 車の位置
}
