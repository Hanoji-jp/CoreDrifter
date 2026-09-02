#pragma once

// 道の制御点を編集するときの定数。
//
// 前の作品のエディタと同じ作りにしてある。
// あちらで実際に使えていた値をそのまま持ってきた。
namespace RoadEditConst
{
	//===== ピック =====
	// 制御点を掴める太さ(m)。前の作品と同じ
	constexpr float PickRadius = 6.0f;

	//===== ギズモ =====
	// 軸ハンドルの長さ(m)
	constexpr float GizmoLength = 12.0f;

	// 軸を掴める太さ(m)。
	// 線からこの距離までなら、その軸を掴んだとみなす
	constexpr float GizmoPickRadius = 1.5f;

	// 軸の先端に出す印の大きさ(m)
	constexpr float GizmoTipHalf = 0.8f;

	//===== スナップ =====
	// 目盛りに吸わせる幅(m)。
	// 道を等間隔に並べたいときに効く
	constexpr float DefaultSnapSize = 1.0f;

	//===== 印 =====
	// 制御点の印の大きさ(m)。
	// 道幅が約9mなので、それより小さくして道を隠さない
	constexpr float MarkRadius = 2.0f;

	// 選んでいる点。大きくして目立たせる
	constexpr float MarkRadiusHot = 3.5f;

	// 点と点を結ぶ線を出すか。
	// 道が細い所で、どの順に繋がっているかが分かる
	constexpr bool ShowLinks = true;
}
