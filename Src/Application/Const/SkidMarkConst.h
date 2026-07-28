#pragma once

// 路面に残るタイヤ痕(スキッドマーク)の定数。
// 後輪の接地点を追いかけて点を打ち、隣り合う点の間を帯(四角形)で繋いで1本の痕にする。
namespace SkidMarkConst
{
	// 痕の本数。0,1=後輪の左右 / 2,3=前輪の左右
	constexpr int TrailCount = 4;
	// 1本あたりの点の上限。点は「一定距離進むごと」に打つので、
	// MaxPoints * MinSegLen がおおよその痕の最大長になる。
	constexpr int MaxPoints = 400;

	// 点を打つ最小間隔(m)。細かすぎると点が増えて重く、粗すぎるとカーブがカクつく
	constexpr float MinSegLen = 0.14f;
	constexpr float Width     = 0.24f;  // 痕の幅(m)。タイヤの接地幅に合わせる

	// 路面へのめり込み・Zファイティング対策の浮かせ量(m)。
	// 大きすぎると坂で痕が浮いて見えるので最小限に。
	constexpr float GroundOffset = 0.015f;

	// 消え方：しばらく濃いまま残り、後半で薄れて消える
	constexpr float Lifetime  = 7.0f;   // 点が消えるまでの時間(秒)
	constexpr float FadeStart = 0.55f;  // この寿命比までは濃さを保つ

	// スリップが弱いと薄く、強いと濃く。0〜1のスリップ量に掛ける
	constexpr float AlphaMax   = 0.62f; // 最大の濃さ
	constexpr float SlipMinFor = 0.15f; // これ未満のスリップ量では痕を残さない

	// 前輪：舵を切った向きに対して横へ滑っている量(タイヤ座標系の横速度)で判定する。
	// 駆動しない前輪は空転しないので、後輪と違って横滑りだけが摩擦痕になる。
	constexpr float FrontSlipThreshold = 2.2f;  // これ以上の横滑り(m/s)で痕が出始める
	constexpr float FrontSlipFull      = 7.0f;  // これで最大の濃さ
	constexpr float FrontAlphaMul      = 0.75f; // 前輪は後輪より薄く(ロックしないため)
	constexpr float FrontWidthMul      = 0.90f; // 前輪は接地幅がやや狭い

	// 色(黒に近い焦げ茶。真っ黒だとアスファルトに馴染まず浮く)
	constexpr float ColorR = 0.07f, ColorG = 0.06f, ColorB = 0.06f;
}
