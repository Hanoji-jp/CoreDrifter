#pragma once

#include "RoadConst.h"

// デリニエータ(視線誘導標)の定数。
//
// ■ 何の役に立つか
// 峠の暗い山道で情報を出しているのは、白線とこれの2つだけ。
// カーブの外側に等間隔で並ぶので、まだ路面が見えていない段階で
// 「この先どちらへどれだけ曲がるか」が読める。
//
// 飾りではなく、先を読むための道具として置く。
//
// ■ カーブにだけ置く
// 直線に並べても意味が無いうえ、数が増えて重くなる。
// 曲がっている所の外側にだけ出す。
namespace DelineatorConst
{
	//===== どこに置くか =====
	// 道の中心からの距離(m)。路肩の外
	constexpr float Offset = RoadConst::HalfWidth + 0.7f;

	// 置く間隔(m)。実物は曲がりがきついほど詰める
	constexpr float SpacingStraight = 22.0f;
	constexpr float SpacingTight    = 9.0f;

	// これ以上曲がっていたら置く(1mあたりのラジアン)。
	// 半径150mでおよそ 0.0067
	constexpr float TurnMin = 0.006f;

	// この曲がりで間隔が SpacingTight になる
	constexpr float TurnTight = 0.030f;

	//===== 形 =====
	constexpr float PostH    = 0.95f;   // 高さ(m)
	constexpr float PostHalf = 0.035f;  // 太さ(半径m)
	constexpr float PostSink = 0.20f;   // 地面へ埋める量(m)

	// 反射板。柱の上のほうに付く四角
	constexpr float PlateY    = 0.72f;  // 根元からの高さ(m)
	constexpr float PlateW    = 0.075f; // 幅(m)
	constexpr float PlateH    = 0.13f;  // 高さ(m)
	constexpr float PlateOut  = 0.008f; // 柱から道側へ出す量(m)

	//===== 色 =====
	constexpr float PostR = 0.86f, PostG = 0.86f, PostB = 0.84f;

	// 反射板。左は白、右は橙(実際の道路と同じ決まり)。
	//
	// 進む向きに対して右が橙。夜に見たとき、
	// どちら側の路肩を見ているのかが色で分かる
	constexpr float LeftR  = 0.90f, LeftG  = 0.90f, LeftB  = 0.88f;
	constexpr float RightR = 0.92f, RightG = 0.45f, RightB = 0.10f;

	// 反射板は明るめの色にする。
	//
	// 自己発光は使わない。材質は1枚しか持たないので、
	// 発光を足すと柱まで光ってしまう

	//===== 上限 =====
	// 壊れた道で数万本立てないための保険
	constexpr int MaxPosts = 4000;
}
