#pragma once

#include "RoadConst.h"

// カーブミラーの定数。
//
// ■ なぜこれだけ別格か
// 峠の絵で一番「そこにある」もの。他の飾りは無くても道に見えるが、
// 見通しの悪いヘアピンにミラーが無いと、道として不自然に見える。
//
// ■ どこに立つか
// 見通しを塞いでいるのは内側(山側)の斜面。その向こうを見るには、
// 塞いでいるものの反対、つまりカーブの外側に立てる必要がある。
// 位置はカーブの一番きつい所(頂点)。
//
// ■ 双面鏡にする
// 峠のミラーはたいてい鏡が2枚付いている。1本で両方向から使えるので、
// 向きをどちらに決めるか悩まなくて済む
namespace CurveMirrorConst
{
	//===== どこに置くか =====
	// 道の中心からの距離(m)。路肩の外、ガードレールの少し先
	constexpr float Offset = RoadConst::HalfWidth + 1.6f;

	// これ以上曲がっていたら立てる(1mあたりのラジアン)。
	// 0.030 で半径33m。見通しが効かなくなるのはこのあたりから
	constexpr float TurnMin = 0.030f;

	// ミラー同士の最小の間隔(m)
	constexpr float MinGap = 45.0f;

	//===== 柱 =====
	// 実物は鋼管(φ76.3mm)。四角い柱のミラーは無い
	constexpr float PoleH      = 2.60f;   // 鏡の中心までの高さ(m)
	constexpr float PoleRadius = 0.042f;
	constexpr int   PoleSides  = 8;
	constexpr float PoleSink   = 0.10f;   // 基礎へ埋める量(m)

	//===== 基礎 =====
	// コンクリートの根巻き。これが無いと柱が地面へ刺さっただけに見える
	constexpr float BaseRadius = 0.15f;
	constexpr float BaseH      = 0.22f;
	constexpr float BaseSink   = 0.06f;   // 地面へ埋める量(m)

	//===== 腕 =====
	// 鏡は柱に直付けしない。腕で前へ持ち出す。
	// 直付けだと柱が鏡の真ん中を裏から突き上げる形になり、
	// 2枚付けたときに互いが柱へめり込む
	constexpr float ArmLen    = 0.34f;
	constexpr float ArmRadius = 0.028f;
	constexpr int   ArmSides  = 6;

	//===== 鏡 =====
	// 実物は直径60cm・80cm・100cmの3種。ここは80cm相当
	constexpr float MirrorR     = 0.40f;   // 半径(m)
	constexpr float MirrorFrame = 0.055f;  // 縁の太さ(m)

	// 何角形で丸を作るか。少ないと丸に見えない
	constexpr int MirrorSides = 20;

	// 凸面の出っ張り(m)。
	//
	// カーブミラーが広い範囲を映せるのは面が凸だから。
	// 平らな円板だと、鏡ではなく白い看板に見える
	constexpr float Bulge = 0.075f;

	// 裏の椀の深さ(m)。実物の裏は平らではなく丸く膨らんでいる
	constexpr float BackDepth = 0.14f;

	//===== 庇 =====
	// 縁に沿った曲面。雪と雨を避ける。
	// 平らな箱を乗せると、鏡に板を立てかけたようにしか見えない
	constexpr float HoodArcDeg = 150.0f;  // 縁の上側を何度ぶん覆うか
	constexpr float HoodDepth  = 0.17f;   // 前へ出す奥行き(m)
	constexpr float HoodRise   = 0.05f;   // 外へ開く量(m)

	// 鏡を道の内側へ振る角度(度)。
	// 真後ろを向けても、塞いでいる斜面が映るだけ。
	// カーブの先を映すには内側へ振る必要がある
	constexpr float ToeInDeg = 34.0f;

	//===== 色 =====
	// 縁は橙。実物は反射材が巻いてあって、夜に光る
	constexpr float FrameR = 0.93f, FrameG = 0.46f, FrameB = 0.09f;

	// 鏡面。青みがかった明るい灰。
	// 真っ白にすると紙に見えるので、少し青へ寄せる
	constexpr float GlassR = 0.72f, GlassG = 0.76f, GlassB = 0.80f;

	// 柱と腕と庇。亜鉛めっきの灰
	constexpr float PoleColR = 0.62f, PoleColG = 0.63f, PoleColB = 0.62f;

	// 基礎のコンクリート
	constexpr float BaseColR = 0.58f, BaseColG = 0.58f, BaseColB = 0.56f;

	//===== 上限 =====
	constexpr int MaxMirrors = 300;
}
