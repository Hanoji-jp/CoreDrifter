#pragma once

#include "RoadConst.h"

// 道路標識(カーブ注意)の定数。
//
// ■ なぜカーブ注意だけか
// 図柄が形だけで作れる。黄色の菱形と黒い矢印なので、
// 板と多角形を組めばそれで標識になる。
//
// 速度制限の丸標識は数字が要り、文字を焼き付ける仕組みが別に必要。
// そこは後で足す。
//
// ■ カーブの手前に置く
// 標識は「これから起きること」を伝えるものなので、
// カーブの中に置いても意味が無い。入口の手前へ下げる。
//
// ■ 日本は左側通行
// 標識は進行方向の左に立つ。右に立てると、
// 対向車線へ向けた標識になってしまう
namespace RoadSignConst
{
	//===== どこに置くか =====
	// 道の中心からの距離(m)。路肩の外
	constexpr float Offset = RoadConst::HalfWidth + 1.4f;

	// カーブと見なす曲がり(1mあたりのラジアン)。
	// デリニエータより鈍くする。標識は本当にきつい所だけ
	constexpr float TurnMin = 0.014f;

	// 入口からどれだけ手前へ下げるか(m)
	constexpr float LeadIn = 32.0f;

	// 標識どうしの最小の間隔(m)。
	// 連続するカーブで数メートルおきに立つのを防ぐ
	constexpr float MinGap = 55.0f;

	//===== 柱 =====
	constexpr float PoleH    = 2.1f;    // 板の下端までの高さ(m)
	constexpr float PoleHalf = 0.045f;  // 太さ(半径m)
	constexpr float PoleSink = 0.30f;   // 地面へ埋める量(m)

	//===== 板(菱形) =====
	// 実物は一辺45cm・60cm・90cmの3種。ここは60cm相当
	constexpr float PlateR      = 0.42f;   // 中心から角までの距離(m)
	constexpr float PlateBorder = 0.045f;  // 黒縁の太さ(m)
	constexpr float PlateLift   = 0.012f;  // 縁と図柄を前へ出す量(m)

	//===== 図柄(矢印) =====
	// 直角に曲がる矢印。縦棒 → 横棒 → 三角の頭
	constexpr float ArrowW     = 0.072f;  // 棒の太さ(m)
	constexpr float ArrowDown  = 0.20f;   // 縦棒の長さ(m)
	constexpr float ArrowSide  = 0.13f;   // 横棒の長さ(m)
	constexpr float ArrowHead  = 0.105f;  // 頭の大きさ(m)

	//===== 色 =====
	// 警戒標識の黄色。実物はかなり鮮やか
	constexpr float PlateR_ = 0.94f, PlateG_ = 0.78f, PlateB_ = 0.10f;

	// 縁と図柄の黒。真っ黒にすると輪郭の後処理と喧嘩する
	constexpr float InkR = 0.09f, InkG = 0.09f, InkB = 0.10f;

	// 柱。亜鉛めっきの灰
	constexpr float PoleR = 0.62f, PoleG = 0.63f, PoleB = 0.62f;

	//===== 上限 =====
	constexpr int MaxSigns = 400;
}
